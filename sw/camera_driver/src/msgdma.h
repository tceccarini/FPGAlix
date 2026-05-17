/* SPDX-License-Identifier: GPL-2.0 */
#ifndef FPGALIX_MSGDMA_H
#define FPGALIX_MSGDMA_H

#include <linux/dmaengine.h>
#include <linux/types.h>

/*
 * Callback invoked (from IRQ context) when a DMA transfer completes.
 * @param:    opaque pointer passed to FPGAlix_dma_submit()
 * @frame_ok: true if the full frame was received, false if the Avalon-ST
 *            EOP arrived early (short frame) — caller must discard the buffer.
 */
typedef void (*FPGAlix_dma_callback_t)(void *param, bool frame_ok);

typedef struct {
	struct dma_chan          *chan;
	struct workqueue_struct *wq;      /* ordered wq for safe dmaengine_tx_status calls */
	atomic_t                 stopping; /* set during stop/release to skip tx_status */
} FPGAlix_dma_chan_t;

/* Request the mSGDMA channel from the dmaengine and configure it for
 * S2MM (device-to-memory) transfers. Must be called once at module init. */
int  FPGAlix_dma_init(FPGAlix_dma_chan_t *dma);

/* Abort any in-flight transfer and wait for the DMA engine to be idle.
 * Does not release the channel — use during stop_streaming. */
void FPGAlix_dma_stop(FPGAlix_dma_chan_t *dma);

/* Terminate any pending transfer and release the DMA channel back to the
 * dmaengine. Must be called at module exit or on error unwind. */
void FPGAlix_dma_release(FPGAlix_dma_chan_t *dma);

/* Submit a single S2MM transfer of @len bytes into the physically contiguous
 * buffer at DMA address @addr.  @cb is invoked from IRQ context when the
 * transfer completes; @frame_ok will be false if EOP arrived before @len
 * bytes were written (short frame). */
int  FPGAlix_dma_submit(FPGAlix_dma_chan_t *dma, dma_addr_t addr,
			size_t len, FPGAlix_dma_callback_t cb, void *cb_param);

#endif /* FPGALIX_MSGDMA_H */