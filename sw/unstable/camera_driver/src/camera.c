// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>

#include "camera.h"
#include "msgdma.h"
#include "sensor.h"

static FPGAlix_cam_dev_t *cam_dev;

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

/*
 * Fill a v4l2_format with our fixed, immutable frame parameters.
 * We support exactly one format: 640x480 Bayer RGGB 8-bit, 30 fps.
 */
static void FPGAlix_fill_fmt(struct v4l2_format *f)
{
	struct v4l2_pix_format *pix = &f->fmt.pix;

	pix->width        = FPGALIX_FRAME_W;
	pix->height       = FPGALIX_FRAME_H;
	pix->pixelformat  = FPGALIX_PIXEL_FMT;
	pix->field        = V4L2_FIELD_NONE;
	pix->bytesperline = FPGALIX_FRAME_W * FPGALIX_FRAME_BPP;
	pix->sizeimage    = FPGALIX_FRAME_LEN;
	pix->colorspace   = V4L2_COLORSPACE_RAW;
}

/* --------------------------------------------------------------------------
 * DMA callback — runs in IRQ context
 * -------------------------------------------------------------------------- */

/*
 * Invoked by msgdma.c when a DMA transfer completes.
 * frame_ok=false means the Avalon-ST EOP arrived before the full frame was
 * written (short frame): we re-submit the same buffer and drop it from
 * userspace. The next buffer was already pre-submitted so the FIFO stays full.
 */
/*
 * Watchdog work — fires if no DMA callback arrives within FPGALIX_DMA_TIMEOUT_MS.
 * This is a fatal, non-recoverable condition: the Altera mSGDMA driver does not
 * expose a software reset, so the hardware state cannot be restored.
 * The error is printed to the kernel log and to the console (pr_crit level),
 * and vb2_queue_error() unblocks any userspace thread stuck on DQBUF with -EIO.
 * Recovery requires rmmod + insmod.
 */
static void FPGAlix_timeout_work(struct work_struct *work)
{
	FPGAlix_cam_dev_t *cam =
		container_of(to_delayed_work(work), FPGAlix_cam_dev_t, timeout_work);
	unsigned long flags;

	/* If no buffers are in-flight, there's nothing to time out. */
	spin_lock_irqsave(&cam->lock, flags);
	if (!cam->streaming || list_empty(&cam->active_list)) {
		spin_unlock_irqrestore(&cam->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&cam->lock, flags);

	pr_crit("FPGAlix: DMA timeout — no frame in %u ms, hardware unresponsive\n",
		FPGALIX_DMA_TIMEOUT_MS);
	pr_crit("FPGAlix: mSGDMA reset not available via dmaengine — rmmod/insmod required\n");

	/* vb2_queue_error does not need q->lock — acquiring cam->mlock here
	 * would deadlock with stop_streaming, which holds it via vb2. */
	vb2_queue_error(&cam->queue);
}

static void FPGAlix_dma_callback(void *param, bool frame_ok)
{
	unsigned long flags;
	FPGAlix_cam_buf_t *buf;
	FPGAlix_cam_dev_t *cam;

	if (param == NULL){
		pr_alert("FPGAlix_dma_callback: param is null\n");
		return; /* Should never happen, but be defensive against buggy DMA engine */
	}
	if (((FPGAlix_cam_buf_t *)param)->cam == NULL){
		pr_alert("FPGAlix_dma_callback: buf->cam is null\n");
		return; /* Should never happen, but be defensive against buggy DMA engine */
	}
	buf = param;
	cam = buf->cam;
	

	if (!cam->streaming)
		return;

	/* Hardware is alive — reset the watchdog */
	mod_delayed_work(system_wq, &cam->timeout_work,
			 msecs_to_jiffies(FPGALIX_DMA_TIMEOUT_MS));

	if (!frame_ok) {
		if (!buf->queued) {
			pr_warn_ratelimited("FPGAlix_dma_callback: buf %p not queued; ignoring short-frame completion\n", buf);
			return;
		}
		pr_warn_ratelimited("FPGAlix_dma_callback: short frame on buf %p queued=%d list_next=%p list_prev=%p cam=%p; resubmitting\n",
				buf, buf->queued, buf->list.next, buf->list.prev, cam);
		FPGAlix_dma_submit(&cam->dma,
				   vb2_dma_contig_plane_dma_addr(&buf->vb.vb2_buf, 0),
				   FPGALIX_FRAME_LEN,
				   FPGAlix_dma_callback, buf,
				   buf->stream_seq);
		return;
	}

	/* Full frame: remove from active_list and hand the buffer to vb2. */
	spin_lock_irqsave(&cam->lock, flags);
	if (buf->queued) {
		list_del_init(&buf->list);
		buf->queued = false;
	} else {
		pr_warn_ratelimited("FPGAlix_dma_callback: buf %p not queued; ignoring late DMA completion\n", buf);
		spin_unlock_irqrestore(&cam->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&cam->lock, flags);

	buf->vb.vb2_buf.timestamp = ktime_get_ns();
	vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
}

/* --------------------------------------------------------------------------
 * vb2 ops
 * -------------------------------------------------------------------------- */

static int FPGAlix_queue_setup(struct vb2_queue *q,
			       unsigned int *num_buffers,
			       unsigned int *num_planes,
			       unsigned int sizes[],
			       struct device *alloc_devs[])
{
	if (*num_buffers < FPGALIX_MIN_BUFFERS)
		*num_buffers = FPGALIX_MIN_BUFFERS;

	*num_planes = 1;
	sizes[0]    = FPGALIX_FRAME_LEN;
	return 0;
}

static int FPGAlix_buf_prepare(struct vb2_buffer *vb)
{
	if (vb2_plane_size(vb, 0) < FPGALIX_FRAME_LEN) {
		pr_err("FPGAlix: buffer too small (%lu < %u)\n",
		       vb2_plane_size(vb, 0), FPGALIX_FRAME_LEN);
		return -EINVAL;
	}
	vb2_set_plane_payload(vb, 0, FPGALIX_FRAME_LEN);
	return 0;
}

/*
 * Called when userspace issues QBUF.
 * If streaming is already active, submit the buffer to DMA immediately
 * so the mSGDMA FIFO stays as full as possible.
 * Otherwise, park it in buf_queue until start_streaming.
 */
static void FPGAlix_buf_queue(struct vb2_buffer *vb)
{
	FPGAlix_cam_dev_t *cam = vb2_get_drv_priv(vb->vb2_queue);
	FPGAlix_cam_buf_t *buf =
		container_of(to_vb2_v4l2_buffer(vb), FPGAlix_cam_buf_t, vb);
	unsigned long flags;
	bool was_empty = false;

	buf->cam = cam;
	buf->queued = false;
	buf->stream_seq = cam->stream_seq;

	spin_lock_irqsave(&cam->lock, flags);
	if (cam->streaming) {
		/* Streaming active: go directly to active_list and submit */
		was_empty = list_empty(&cam->active_list);
		list_add_tail(&buf->list, &cam->active_list);
		buf->queued = true;
		spin_unlock_irqrestore(&cam->lock, flags);
		if (was_empty)
			mod_delayed_work(system_wq, &cam->timeout_work,
					 msecs_to_jiffies(FPGALIX_DMA_TIMEOUT_MS));
		FPGAlix_dma_submit(&cam->dma,
				   vb2_dma_contig_plane_dma_addr(vb, 0),
				   FPGALIX_FRAME_LEN,
				   FPGAlix_dma_callback, buf,
				   buf->stream_seq);
		return;
	}
	/* Not streaming yet: park in buf_queue until start_streaming */
	list_add_tail(&buf->list, &cam->buf_queue);
	buf->queued = true;
	spin_unlock_irqrestore(&cam->lock, flags);
}

/*
 * Pre-submit all queued buffers to the mSGDMA so the descriptor FIFO is full
 * before the first frame arrives. vb2 guarantees at least min_buffers_needed
 * (3) are in the queue before calling us.
 */
static int FPGAlix_start_streaming(struct vb2_queue *q, unsigned int count)
{
	FPGAlix_cam_dev_t *cam = vb2_get_drv_priv(q);
	FPGAlix_cam_buf_t *buf;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&cam->lock, flags);
	cam->streaming = true;
	cam->stream_seq++;

	schedule_delayed_work(&cam->timeout_work,
			      msecs_to_jiffies(FPGALIX_DMA_TIMEOUT_MS));

	/* Move buffers from buf_queue to active_list one at a time.
	 * list_first_entry + list_move_tail is safe: we re-acquire the lock
	 * between each submit so a concurrent DMA callback cannot corrupt
	 * the iterator. */
	while (!list_empty(&cam->buf_queue)) {
		buf = list_first_entry(&cam->buf_queue, FPGAlix_cam_buf_t, list);
		buf->stream_seq = cam->stream_seq;
		list_move_tail(&buf->list, &cam->active_list);
		/* buf->queued already true from buf_queue */
		spin_unlock_irqrestore(&cam->lock, flags);

		ret = FPGAlix_dma_submit(&cam->dma,
					 vb2_dma_contig_plane_dma_addr(&buf->vb.vb2_buf, 0),
					 FPGALIX_FRAME_LEN,
					 FPGAlix_dma_callback, buf,
					 buf->stream_seq);
		if (ret) {
			pr_err("FPGAlix: DMA submit failed at start: %d\n", ret);
			spin_lock_irqsave(&cam->lock, flags);
			list_del(&buf->list);
			buf->queued = false;
			spin_unlock_irqrestore(&cam->lock, flags);
			vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
		}

		spin_lock_irqsave(&cam->lock, flags);
	}
	spin_unlock_irqrestore(&cam->lock, flags);
	return 0;
}

static void FPGAlix_stop_streaming(struct vb2_queue *q)
{
	FPGAlix_cam_dev_t *cam = vb2_get_drv_priv(q);
	FPGAlix_cam_buf_t *buf;
	unsigned long flags;

	cancel_delayed_work_sync(&cam->timeout_work);

	/* Terminate all in-flight DMA and drain the work queue.
	 * FPGAlix_dma_wait_idle cannot be used here: if the hardware is stuck
	 * (no more interrupts), inflight never reaches 0 and we deadlock. */
	FPGAlix_dma_stop(&cam->dma);

	spin_lock_irqsave(&cam->lock, flags);
	cam->streaming = false;
	spin_unlock_irqrestore(&cam->lock, flags);

	/* Return all buffers still waiting in buf_queue (not yet submitted) */
	spin_lock_irqsave(&cam->lock, flags);
	while (!list_empty(&cam->buf_queue)) {
		buf = list_first_entry(&cam->buf_queue, FPGAlix_cam_buf_t, list);
		list_del(&buf->list);
		buf->queued = false;
		spin_unlock_irqrestore(&cam->lock, flags);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
		spin_lock_irqsave(&cam->lock, flags);
	}
	/* Return all buffers that were in-flight (DMA terminated above) */
	while (!list_empty(&cam->active_list)) {
		buf = list_first_entry(&cam->active_list, FPGAlix_cam_buf_t, list);
		list_del(&buf->list);
		buf->queued = false;
		spin_unlock_irqrestore(&cam->lock, flags);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
		spin_lock_irqsave(&cam->lock, flags);
	}
	spin_unlock_irqrestore(&cam->lock, flags);
}

static const struct vb2_ops fpgalix_vb2_ops = {
	.queue_setup     = FPGAlix_queue_setup,
	.buf_prepare     = FPGAlix_buf_prepare,
	.buf_queue       = FPGAlix_buf_queue,
	.start_streaming = FPGAlix_start_streaming,
	.stop_streaming  = FPGAlix_stop_streaming,
	.wait_prepare    = vb2_ops_wait_prepare,
	.wait_finish     = vb2_ops_wait_finish,
};

/* --------------------------------------------------------------------------
 * V4L2 ioctl handlers
 * -------------------------------------------------------------------------- */

static int fpgalix_querycap(struct file *file, void *priv,
			    struct v4l2_capability *cap)
{
	strscpy(cap->driver,   FPGALIX_DRV_NAME,   sizeof(cap->driver));
	strscpy(cap->card,     FPGALIX_CARD_NAME,  sizeof(cap->card));
	strscpy(cap->bus_info, FPGALIX_BUS_INFO,   sizeof(cap->bus_info));
	return 0;
}

static int fpgalix_enum_fmt(struct file *file, void *priv,
			    struct v4l2_fmtdesc *f)
{
	if (f->index != 0)
		return -EINVAL;
	f->pixelformat = FPGALIX_PIXEL_FMT;
	return 0;
}

static int fpgalix_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	FPGAlix_fill_fmt(f);
	return 0;
}

static int fpgalix_try_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	FPGAlix_fill_fmt(f);
	return 0;
}

static int fpgalix_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	FPGAlix_fill_fmt(f);
	return 0;
}

static int fpgalix_enum_input(struct file *file, void *priv,
			      struct v4l2_input *inp)
{
	if (inp->index != 0)
		return -EINVAL;
	strscpy(inp->name, "Camera", sizeof(inp->name));
	inp->type = V4L2_INPUT_TYPE_CAMERA;
	return 0;
}

static int fpgalix_g_input(struct file *file, void *priv, unsigned int *i)
{
	*i = 0;
	return 0;
}

static int fpgalix_s_input(struct file *file, void *priv, unsigned int i)
{
	return (i == 0) ? 0 : -EINVAL;
}

static int fpgalix_g_parm(struct file *file, void *priv,
			  struct v4l2_streamparm *sp)
{
	struct v4l2_captureparm *cp = &sp->parm.capture;

	if (sp->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	cp->capability               = V4L2_CAP_TIMEPERFRAME;
	cp->timeperframe.numerator   = FPGALIX_FPS_NUM;
	cp->timeperframe.denominator = FPGALIX_FPS_DEN;
	return 0;
}

static int fpgalix_s_parm(struct file *file, void *priv,
			  struct v4l2_streamparm *sp)
{
	/* Frame rate is fixed — ignore the requested value and return actual. */
	return fpgalix_g_parm(file, priv, sp);
}

static const struct v4l2_ioctl_ops fpgalix_ioctl_ops = {
	.vidioc_querycap         = fpgalix_querycap,
	.vidioc_enum_fmt_vid_cap = fpgalix_enum_fmt,
	.vidioc_g_fmt_vid_cap    = fpgalix_g_fmt,
	.vidioc_try_fmt_vid_cap  = fpgalix_try_fmt,
	.vidioc_s_fmt_vid_cap    = fpgalix_s_fmt,
	.vidioc_enum_input       = fpgalix_enum_input,
	.vidioc_g_input          = fpgalix_g_input,
	.vidioc_s_input          = fpgalix_s_input,
	.vidioc_g_parm           = fpgalix_g_parm,
	.vidioc_s_parm           = fpgalix_s_parm,
	/* vb2 provides the buffer management ioctls */
	.vidioc_reqbufs          = vb2_ioctl_reqbufs,
	.vidioc_querybuf         = vb2_ioctl_querybuf,
	.vidioc_qbuf             = vb2_ioctl_qbuf,
	.vidioc_dqbuf            = vb2_ioctl_dqbuf,
	.vidioc_streamon         = vb2_ioctl_streamon,
	.vidioc_streamoff        = vb2_ioctl_streamoff,
	.vidioc_expbuf           = vb2_ioctl_expbuf,
};

/* --------------------------------------------------------------------------
 * file_operations — delegated entirely to vb2 and v4l2 helpers
 * -------------------------------------------------------------------------- */

static const struct v4l2_file_operations fpgalix_fops = {
	.owner          = THIS_MODULE,
	.open           = v4l2_fh_open,
	.release        = vb2_fop_release,
	.read           = vb2_fop_read,
	.poll           = vb2_fop_poll,
	.mmap           = vb2_fop_mmap,
	.unlocked_ioctl = video_ioctl2,
};

/* --------------------------------------------------------------------------
 * Module init / exit
 * -------------------------------------------------------------------------- */

/*
 * Teardown levels — each case undoes one init step then falls through
 * to undo all previous ones. Call with the level reached before the failure,
 * or with LEVEL_FULL from module_exit to tear everything down.
 */
enum {
	LEVEL_ALLOC  = 0,
	LEVEL_DMA    = 1,
	LEVEL_SENSOR = 2,
	LEVEL_V4L2   = 3,
	LEVEL_VB2    = 4,
	LEVEL_FULL   = 5,
};

static void FPGAlix_camera_teardown(FPGAlix_cam_dev_t *cam, int level)
{
	switch (level) {
	case LEVEL_FULL:  video_unregister_device(&cam->vdev);      fallthrough;
	case LEVEL_VB2:   vb2_queue_release(&cam->queue);           fallthrough;
	case LEVEL_V4L2:  v4l2_device_unregister(&cam->v4l2_dev);  fallthrough;
	case LEVEL_SENSOR: FPGAlix_sensor_release();                fallthrough;
	case LEVEL_DMA:   FPGAlix_dma_release(&cam->dma);           fallthrough;
	default:          kfree(cam);
	}
}

static int __init FPGAlix_camera_init(void)
{
	FPGAlix_cam_dev_t *cam;
	struct vb2_queue *q;
	int ret;

	cam = kzalloc(sizeof(*cam), GFP_KERNEL);
	if (!cam)
		return -ENOMEM;

	mutex_init(&cam->mlock);
	spin_lock_init(&cam->lock);
	INIT_LIST_HEAD(&cam->buf_queue);
	INIT_LIST_HEAD(&cam->active_list);
	INIT_DELAYED_WORK(&cam->timeout_work, FPGAlix_timeout_work);

	ret = FPGAlix_dma_init(&cam->dma);
	if (ret) {
		FPGAlix_camera_teardown(cam, LEVEL_ALLOC);
		return ret;
	}

	ret = FPGAlix_sensor_init();
	if (ret) {
		pr_err("FPGAlix: sensor_init failed: %d\n", ret);
		FPGAlix_camera_teardown(cam, LEVEL_DMA);
		return ret;
	}

	/* name must be set before v4l2_device_register when dev=NULL */
	strscpy(cam->v4l2_dev.name, FPGALIX_DRV_NAME, sizeof(cam->v4l2_dev.name));
	ret = v4l2_device_register(NULL, &cam->v4l2_dev);
	if (ret) {
		pr_err("FPGAlix: v4l2_device_register failed: %d\n", ret);
		FPGAlix_camera_teardown(cam, LEVEL_SENSOR);
		return ret;
	}

	q                     = &cam->queue;
	q->type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes           = VB2_MMAP | VB2_DMABUF;
	q->drv_priv           = cam;
	q->buf_struct_size    = sizeof(FPGAlix_cam_buf_t);
	q->ops                = &fpgalix_vb2_ops;
	q->mem_ops            = &vb2_dma_contig_memops;
	q->timestamp_flags    = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->min_buffers_needed = FPGALIX_MIN_BUFFERS;
	q->lock               = &cam->mlock;
	/* Use the DMA engine's device for dma_alloc_coherent */
	q->dev                = cam->dma.chan->device->dev;

	ret = vb2_queue_init(q);
	if (ret) {
		pr_err("FPGAlix: vb2_queue_init failed: %d\n", ret);
		FPGAlix_camera_teardown(cam, LEVEL_V4L2);
		return ret;
	}

	strscpy(cam->vdev.name, FPGALIX_CARD_NAME, sizeof(cam->vdev.name));
	cam->vdev.v4l2_dev    = &cam->v4l2_dev;
	cam->vdev.fops        = &fpgalix_fops;
	cam->vdev.ioctl_ops   = &fpgalix_ioctl_ops;
	cam->vdev.release     = video_device_release_empty;
	cam->vdev.queue       = q;
	cam->vdev.lock        = &cam->mlock;
	cam->vdev.device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	video_set_drvdata(&cam->vdev, cam);

	ret = video_register_device(&cam->vdev, VFL_TYPE_VIDEO, -1);
	if (ret) {
		pr_err("FPGAlix: video_register_device failed: %d\n", ret);
		FPGAlix_camera_teardown(cam, LEVEL_VB2);
		return ret;
	}

	cam_dev = cam;
	pr_info("FPGAlix: camera ready on %s\n",
		video_device_node_name(&cam->vdev));
	return 0;
}

static void __exit FPGAlix_camera_exit(void)
{
	FPGAlix_camera_teardown(cam_dev, LEVEL_FULL);
}

module_init(FPGAlix_camera_init);
module_exit(FPGAlix_camera_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FPGAlix OV7670 camera driver — V4L2 + Altera mSGDMA");
