// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "config.h"
#include "msgdma.h"

typedef struct {
	FPGAlix_dma_chan_t     *dma;
	FPGAlix_dma_callback_t  cb;
	void                   *cb_param;
	size_t                  expected_len;
	u32                     residue;     /* captured from hw descriptor before recycle */
	struct work_struct      work;
} FPGAlix_dma_ctx_t;

/*
 * Work handler — runs in process context on dma->wq.
 * Residue was already captured from the hardware descriptor in
 * FPGAlix_dma_result_cb (tasklet context) before the descriptor was recycled,
 * so no dmaengine_tx_status call is needed here.
 */
static void FPGAlix_dma_work(struct work_struct *work)
{
	FPGAlix_dma_ctx_t     *ctx      = container_of(work, FPGAlix_dma_ctx_t, work);
	FPGAlix_dma_callback_t cb       = ctx->cb;
	void                  *cb_param = ctx->cb_param;
	bool                   frame_ok;

	if (atomic_read(&ctx->dma->stopping)) {
		kfree(ctx);
		cb(cb_param, true);
		return;
	}

	frame_ok = (ctx->residue == 0);

	if (!frame_ok)
		pr_warn_ratelimited("FPGAlix: short frame — expected %zu bytes, arrived %zu bytes, discarding\n",
				    ctx->expected_len,
				    ctx->expected_len - ctx->residue);

	kfree(ctx);
	cb(cb_param, frame_ok);
}

/*
 * Result callback — runs in tasklet context.
 * The dmaengine_result carries residue populated directly from the mSGDMA
 * hardware descriptor response BEFORE the descriptor is recycled.
 * We capture it and defer all other work to process context.
 */
static void FPGAlix_dma_result_cb(void *param,
				  const struct dmaengine_result *result)
{
	FPGAlix_dma_ctx_t *ctx = param;

	/* result may be NULL if the mSGDMA driver does not populate it */
	ctx->residue = (result != NULL) ? result->residue : 0;
	queue_work(ctx->dma->wq, &ctx->work);
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

	atomic_set(&dma->stopping, 0);

	dma->wq = alloc_ordered_workqueue("fpgalix_dma", 0);
	if (!dma->wq) {
		pr_err("FPGAlix: failed to allocate workqueue\n");
		dma_release_channel(dma->chan);
		dma->chan = NULL;
		return -ENOMEM;
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.direction      = DMA_DEV_TO_MEM;
	cfg.src_addr_width = FPGALIX_DMA_BUS_WIDTH;
	cfg.dst_addr_width = FPGALIX_DMA_BUS_WIDTH;

	ret = dmaengine_slave_config(dma->chan, &cfg);
	if (ret) {
		pr_err("FPGAlix: slave_config failed: %d\n", ret);
		destroy_workqueue(dma->wq);
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
	atomic_set(&dma->stopping, 1);
	dmaengine_terminate_sync(dma->chan);
	drain_workqueue(dma->wq);
	atomic_set(&dma->stopping, 0);
}

void FPGAlix_dma_release(FPGAlix_dma_chan_t *dma)
{
	if (!dma->chan)
		return;
	atomic_set(&dma->stopping, 1);
	dmaengine_terminate_sync(dma->chan);
	destroy_workqueue(dma->wq);
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
	ctx->residue      = 0;
	INIT_WORK(&ctx->work, FPGAlix_dma_work);

	/* Use callback_result to get residue from the hw descriptor
	 * before it is recycled by the mSGDMA driver. */
	tx->callback        = NULL;
	tx->callback_result = FPGAlix_dma_result_cb;
	tx->callback_param  = ctx;

	{
		dma_cookie_t cookie = dmaengine_submit(tx);

		if (dma_submit_error(cookie)) {
			pr_err("FPGAlix: dmaengine_submit failed\n");
			kfree(ctx);
			return -EIO;
		}
	}

	dma_async_issue_pending(dma->chan);
	return 0;
}
