/* SPDX-License-Identifier: GPL-2.0 */
#ifndef FPGALIX_MSGDMA_H
#define FPGALIX_MSGDMA_H

#include <linux/dmaengine.h>
#include <linux/types.h>
#include <linux/wait.h>

/*
 * Callback invoked from process context (workqueue) when a DMA transfer
 * completes.  frame_ok=false means the Avalon-ST EOP arrived before @len
 * bytes were written (short frame).
 */
typedef void (*FPGAlix_dma_callback_t)(void *param, bool frame_ok);

typedef struct {
	struct dma_chan          *chan;
	struct workqueue_struct  *wq;
	atomic_t                  stopping; /* set during stop/release to skip callbacks */
	atomic_t                  inflight; /* submitted descriptors not yet completed */
	atomic_t                  chan_seq; /* bumped on dma_stop; late work items are dropped */
	wait_queue_head_t         idle_wait;
} FPGAlix_dma_chan_t;

/* Request the mSGDMA channel and configure it for S2MM transfers. */
int  FPGAlix_dma_init(FPGAlix_dma_chan_t *dma);

/* Drain in-flight work without releasing the channel (use at stop_streaming). */
void FPGAlix_dma_stop(FPGAlix_dma_chan_t *dma);

/* Wait until all submitted descriptors have completed or timeout expires.
 * Returns 0 on success, -ETIMEDOUT on timeout. */
int  FPGAlix_dma_wait_idle(FPGAlix_dma_chan_t *dma, unsigned long timeout_ms);

/* Terminate any pending transfer and release the channel (use at module exit). */
void FPGAlix_dma_release(FPGAlix_dma_chan_t *dma);

/* Submit a single S2MM transfer of @len bytes into the buffer at @addr.
 * @cb is invoked from process context when the transfer completes. */
int  FPGAlix_dma_submit(FPGAlix_dma_chan_t *dma, dma_addr_t addr,
			size_t len, FPGAlix_dma_callback_t cb, void *cb_param,
			u32 stream_seq);

#endif /* FPGALIX_MSGDMA_H */
