/* SPDX-License-Identifier: GPL-2.0 */
#ifndef FPGALIX_MSGDMA_H
#define FPGALIX_MSGDMA_H

#include <linux/dmaengine.h>
#include <linux/types.h>
#include <linux/wait.h>

/*
 * Callback invoked (from IRQ context) when a DMA transfer completes.
 * @param:    opaque pointer passed to FPGAlix_dma_submit()
 * @frame_ok: true if the full frame was received, false if the Avalon-ST
 *            EOP arrived early (short frame) — caller must discard the buffer.
 */
typedef void (*FPGAlix_dma_callback_t)(void *param, bool frame_ok);

typedef struct {
	struct dma_chan          *chan;
	struct workqueue_struct *wq;
	atomic_t                 stopping; /* set during stop/release to skip vb2 callbacks */
	atomic_t                 inflight; /* submitted DMA descriptors not yet completed */
	atomic_t                 chan_seq; /* incremented on each dma_stop; work items from
	                                   * older sessions are dropped without touching inflight */
	wait_queue_head_t        idle_wait;
} FPGAlix_dma_chan_t;

/* Request the mSGDMA channel from the dmaengine and configure it for
 * S2MM (device-to-memory) transfers. Must be called once at module init. */
int  FPGAlix_dma_init(FPGAlix_dma_chan_t *dma);

/* Abort any in-flight transfer and wait for the DMA engine to be idle.
 * Does not release the channel — use during stop_streaming. */
void FPGAlix_dma_stop(FPGAlix_dma_chan_t *dma);

/* Wait until all submitted DMA descriptors have completed or timeout expires.
 * Returns 0 on success, -ETIMEDOUT on timeout. */
int  FPGAlix_dma_wait_idle(FPGAlix_dma_chan_t *dma, unsigned long timeout_ms);

/* Terminate any pending transfer and release the DMA channel back to the
 * dmaengine. Must be called at module exit or on error unwind. */
void FPGAlix_dma_release(FPGAlix_dma_chan_t *dma);

/* Submit a single S2MM transfer of @len bytes into the physically contiguous
 * buffer at DMA address @addr.  @cb is invoked from IRQ context when the
 * transfer completes; @frame_ok will be false if EOP arrived before @len
 * bytes were written (short frame). */
int  FPGAlix_dma_submit(FPGAlix_dma_chan_t *dma, dma_addr_t addr,
			size_t len, FPGAlix_dma_callback_t cb, void *cb_param,
			u32 stream_seq);

#endif /* FPGALIX_MSGDMA_H */