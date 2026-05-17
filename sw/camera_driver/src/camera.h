/* SPDX-License-Identifier: GPL-2.0 */
#ifndef FPGALIX_CAMERA_H
#define FPGALIX_CAMERA_H

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-v4l2.h>

#include "config.h"
#include "msgdma.h"

/* Forward declaration needed by FPGAlix_cam_buf_t */
typedef struct FPGAlix_cam_dev FPGAlix_cam_dev_t;

/*
 * Per-buffer descriptor.  vb must be first so vb2 can cast between
 * vb2_v4l2_buffer and FPGAlix_cam_buf_t with a plain container_of().
 */
typedef struct {
	struct vb2_v4l2_buffer  vb;
	struct list_head        list;   /* entry in FPGAlix_cam_dev_t::buf_queue */
	FPGAlix_cam_dev_t      *cam;    /* back-pointer, set in buf_queue callback */
} FPGAlix_cam_buf_t;

/*
 * Main device context, allocated once at module_init and freed at exit.
 */
struct FPGAlix_cam_dev {
	struct v4l2_device  v4l2_dev;
	struct video_device vdev;
	struct vb2_queue    queue;
	struct mutex        mlock;          /* serializes ioctls and vb2 ops */

	FPGAlix_dma_chan_t  dma;
	struct delayed_work timeout_work;

	/* Buffers queued by userspace, waiting to be submitted to DMA.
	 * Always accessed with lock held. */
	struct list_head    buf_queue;

	/* Buffers currently in-flight (submitted to DMA, not yet returned to vb2).
	 * Always accessed with lock held. */
	struct list_head    active_list;

	spinlock_t          lock;    /* protects buf_queue and active_list */

	bool                streaming;
};

#endif /* FPGALIX_CAMERA_H */