// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>

#include "msgdma.h"
#include "camera.h"
#include "config.h"

struct msgdma_test_buf {
	FPGAlix_cam_buf_t cam_buf;
	void *vaddr;
	dma_addr_t dma_addr;
};

static int buf_count = 4;
module_param(buf_count, int, 0644);
MODULE_PARM_DESC(buf_count, "Number of DMA buffers to keep in flight");

static int total_frames = 1000;
module_param(total_frames, int, 0644);
MODULE_PARM_DESC(total_frames, "Total number of DMA completions to run (0 = run forever)");

static atomic_t remaining;
static atomic_t completed;
static atomic_t short_frames;
static atomic_t running;

static struct completion done;
static struct msgdma_test_buf *test_bufs;
static FPGAlix_cam_dev_t test_cam;
static FPGAlix_dma_chan_t test_dma;

static void msgdma_test_cb(void *param, bool frame_ok)
{
	FPGAlix_cam_buf_t *cam_buf = param;
	struct msgdma_test_buf *tbuf;
	int left;
	int ret;

	if (!cam_buf)
		return;

	tbuf = container_of(cam_buf, struct msgdma_test_buf, cam_buf);

	if (!atomic_read(&running))
		return;

	atomic_inc(&completed);
	if (!frame_ok)
		atomic_inc(&short_frames);

	if (total_frames > 0) {
		left = atomic_dec_return(&remaining);
		if (left <= 0) {
			atomic_set(&running, 0);
			complete(&done);
			return;
		}
	}

	ret = FPGAlix_dma_submit(&test_dma,
				 tbuf->dma_addr,
				 FPGALIX_FRAME_LEN,
				 msgdma_test_cb,
				 cam_buf,
				 cam_buf->stream_seq);
	if (ret) {
		pr_err("msgdma_test: submit failed: %d\n", ret);
		atomic_set(&running, 0);
		complete(&done);
	}
}

static int __init msgdma_test_init(void)
{
	int i;
	int ret;

	if (buf_count <= 0)
		return -EINVAL;
	if (total_frames < 0)
		return -EINVAL;

	init_completion(&done);
	atomic_set(&completed, 0);
	atomic_set(&short_frames, 0);
	atomic_set(&running, 1);
	if (total_frames > 0)
		atomic_set(&remaining, total_frames);

	spin_lock_init(&test_cam.lock);
	INIT_LIST_HEAD(&test_cam.buf_queue);
	INIT_LIST_HEAD(&test_cam.active_list);
	test_cam.streaming = true;
	test_cam.stream_seq = 1;

	ret = FPGAlix_dma_init(&test_dma);
	if (ret)
		return ret;

	test_bufs = kcalloc(buf_count, sizeof(*test_bufs), GFP_KERNEL);
	if (!test_bufs) {
		FPGAlix_dma_release(&test_dma);
		return -ENOMEM;
	}

	for (i = 0; i < buf_count; i++) {
		test_bufs[i].vaddr = dma_alloc_coherent(test_dma.chan->device->dev,
							 FPGALIX_FRAME_LEN,
							 &test_bufs[i].dma_addr,
							 GFP_KERNEL);
		if (!test_bufs[i].vaddr) {
			ret = -ENOMEM;
			goto err_free;
		}

		INIT_LIST_HEAD(&test_bufs[i].cam_buf.list);
		test_bufs[i].cam_buf.cam = &test_cam;
		test_bufs[i].cam_buf.queued = true;
		test_bufs[i].cam_buf.stream_seq = test_cam.stream_seq;

		ret = FPGAlix_dma_submit(&test_dma,
					 test_bufs[i].dma_addr,
					 FPGALIX_FRAME_LEN,
					 msgdma_test_cb,
					 &test_bufs[i].cam_buf,
					 test_bufs[i].cam_buf.stream_seq);
		if (ret)
			goto err_free;
	}

	pr_info("msgdma_test: started with %d buffers, total_frames=%d\n",
		buf_count, total_frames);
	return 0;

err_free:
	atomic_set(&running, 0);
	FPGAlix_dma_stop(&test_dma);
	for (i = 0; i < buf_count; i++) {
		if (test_bufs[i].vaddr)
			dma_free_coherent(test_dma.chan->device->dev,
					 FPGALIX_FRAME_LEN,
					 test_bufs[i].vaddr,
					 test_bufs[i].dma_addr);
	}
	kfree(test_bufs);
	FPGAlix_dma_release(&test_dma);
	return ret;
}

static void __exit msgdma_test_exit(void)
{
	int i;

	atomic_set(&running, 0);
	if (total_frames > 0)
		wait_for_completion_timeout(&done, msecs_to_jiffies(5000));

	FPGAlix_dma_wait_idle(&test_dma, 5000);
	FPGAlix_dma_stop(&test_dma);

	for (i = 0; i < buf_count; i++) {
		if (test_bufs && test_bufs[i].vaddr)
			dma_free_coherent(test_dma.chan->device->dev,
					 FPGALIX_FRAME_LEN,
					 test_bufs[i].vaddr,
					 test_bufs[i].dma_addr);
	}
	kfree(test_bufs);

	FPGAlix_dma_release(&test_dma);

	pr_info("msgdma_test: completed=%d short_frames=%d\n",
		atomic_read(&completed),
		atomic_read(&short_frames));
}

module_init(msgdma_test_init);
module_exit(msgdma_test_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FPGAlix mSGDMA standalone test module");
