// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

#include "config.h"
#include "msgdma.h"

/*
 * Per-transfer context — allocated with GFP_ATOMIC in FPGAlix_dma_submit and
 * freed at the start of FPGAlix_dma_internal_cb, before invoking the caller's
 * callback. One instance per in-flight descriptor.
 */
typedef struct {
	FPGAlix_dma_chan_t     *dma;
	FPGAlix_dma_callback_t  cb;
	void                   *cb_param;
	size_t                  expected_len;
	dma_cookie_t            cookie;
} FPGAlix_dma_ctx_t;

/*
 * Raw DMA callback — runs in tasklet (softirq) context.
 * dmaengine_tx_status is NOT called here because the mSGDMA tasklet holds
 * an internal spinlock when invoking this callback — calling tx_status
 * would deadlock. Short frame detection is therefore disabled for now.
 * See TODO.md for the planned workqueue-based fix.
 */
static void FPGAlix_dma_internal_cb(void *param)
{
	FPGAlix_dma_ctx_t     *ctx      = param;
	FPGAlix_dma_callback_t cb       = ctx->cb;
	void                  *cb_param = ctx->cb_param;

	kfree(ctx);
	cb(cb_param, true);
}

int FPGAlix_dma_init(FPGAlix_dma_chan_t *dma)
{
	struct dma_slave_config cfg;
	dma_cap_mask_t mask;
	int ret;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	dma->chan = dma_request_channel(mask, NULL, NULL);
	if (!dma->chan) {
		pr_err("FPGAlix: no DMA channel available\n");
		return -ENODEV;
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.direction      = DMA_DEV_TO_MEM;
	cfg.src_addr_width = FPGALIX_DMA_BUS_WIDTH;
	cfg.dst_addr_width = FPGALIX_DMA_BUS_WIDTH;

	ret = dmaengine_slave_config(dma->chan, &cfg);
	if (ret) {
		pr_err("FPGAlix: slave_config failed: %d\n", ret);
		dma_release_channel(dma->chan);
		dma->chan = NULL;
		return ret;
	}

	pr_info("FPGAlix: DMA channel acquired: %s\n", dma_chan_name(dma->chan));
	return 0;
}

void FPGAlix_dma_stop(FPGAlix_dma_chan_t *dma)
{
	if (!dma->chan)
		return;
	dmaengine_terminate_sync(dma->chan);
}

void FPGAlix_dma_release(FPGAlix_dma_chan_t *dma)
{
	if (!dma->chan)
		return;
	dmaengine_terminate_sync(dma->chan);
	dma_release_channel(dma->chan);
	dma->chan = NULL;
}

int FPGAlix_dma_submit(FPGAlix_dma_chan_t *dma, dma_addr_t addr,
		       size_t len, FPGAlix_dma_callback_t cb, void *cb_param)
{
	struct dma_async_tx_descriptor *tx;
	FPGAlix_dma_ctx_t              *ctx;

	ctx = kmalloc(sizeof(*ctx), GFP_ATOMIC);
	if (!ctx) {
		pr_err("FPGAlix: cannot allocate DMA context\n");
		return -ENOMEM;
	}

	/* vb2_dma_contig guarantees physically contiguous buffers,
	 * so prep_slave_single is sufficient (no scatter-gather needed). */
	tx = dmaengine_prep_slave_single(dma->chan, addr, len,
					 DMA_DEV_TO_MEM,
					 DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!tx) {
		pr_err("FPGAlix: prep_slave_single failed\n");
		kfree(ctx);
		return -ENXIO;
	}

	ctx->dma          = dma;
	ctx->cb           = cb;
	ctx->cb_param     = cb_param;
	ctx->expected_len = len;

	tx->callback       = FPGAlix_dma_internal_cb;
	tx->callback_param = ctx;

	ctx->cookie = dmaengine_submit(tx);
	if (dma_submit_error(ctx->cookie)) {
		pr_err("FPGAlix: dmaengine_submit failed\n");
		kfree(ctx);
		return -EIO;
	}

	dma_async_issue_pending(dma->chan);
	return 0;
}