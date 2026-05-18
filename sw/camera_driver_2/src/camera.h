/* SPDX-License-Identifier: GPL-2.0 */
#ifndef FPGALIX_CAMERA_H
#define FPGALIX_CAMERA_H

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-v4l2.h>

#include "config.h"
#include "msgdma.h"

typedef struct FPGAlix_cam_dev FPGAlix_cam_dev_t;

/*
 * Per-buffer descriptor.  vb must be first so vb2 can cast between
 * vb2_v4l2_buffer and FPGAlix_cam_buf_t with container_of().
 */
typedef struct {
	struct vb2_v4l2_buffer  vb;
	struct list_head         list;
	FPGAlix_cam_dev_t       *cam;
	bool                     queued;
	u32                      stream_seq;
} FPGAlix_cam_buf_t;

struct FPGAlix_cam_dev {
	struct v4l2_device   v4l2_dev;
	struct video_device  vdev;
	struct vb2_queue     queue;
	struct mutex         mlock;

	FPGAlix_dma_chan_t   dma;
	struct delayed_work  timeout_work;

	/* Buffers queued by userspace, waiting for start_streaming. */
	struct list_head     buf_queue;

	/* Buffers currently submitted to DMA. */
	struct list_head     active_list;

	spinlock_t           lock;   /* protects buf_queue, active_list, streaming */

	bool                 streaming;
	u32                  stream_seq;
};

#endif /* FPGALIX_CAMERA_H */
