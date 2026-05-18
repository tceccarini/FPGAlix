// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "camera.h"
#include "config.h"
#include "msgdma.h"

/* If true, ignore DMA residue and treat all frames as complete. */
static bool fpgalix_ignore_residue = false;
module_param_named(ignore_residue, fpgalix_ignore_residue, bool, 0644);
MODULE_PARM_DESC(ignore_residue, "Ignore DMA residue and treat all frames as complete");

typedef struct {
	FPGAlix_dma_chan_t     *dma;
	FPGAlix_dma_callback_t  cb;
	void                   *cb_param;
	u32                     stream_seq;
	u32                     chan_seq;
	size_t                  expected_len;
	u32                     residue;
	struct work_struct       work;
} FPGAlix_dma_ctx_t;

/*
 * Work handler — runs in process context on dma->wq.
 * Residue is captured in FPGAlix_dma_result_cb before the descriptor is
 * recycled, so no dmaengine_tx_status call is needed here.
 */
static void FPGAlix_dma_work(struct work_struct *work)
{
	FPGAlix_dma_ctx_t     *ctx      = container_of(work, FPGAlix_dma_ctx_t, work);
	FPGAlix_dma_callback_t cb       = ctx->cb;
	void                  *cb_param = ctx->cb_param;
	FPGAlix_cam_buf_t     *buf      = cb_param;
	bool                   frame_ok;

	/* Drop work items from a previous stop/start cycle. */
	if (atomic_read(&ctx->dma->chan_seq) != ctx->chan_seq) {
		kfree(ctx);
		return;
	}

	if (!buf || !buf->cam) {
		pr_warn("FPGAlix_dma_work: missing buffer/cam, dropping\n");
		if (atomic_dec_and_test(&ctx->dma->inflight))
			wake_up_all(&ctx->dma->idle_wait);
		kfree(ctx);
		return;
	}

	if (!buf->queued || buf->stream_seq != ctx->stream_seq) {
		if (atomic_dec_and_test(&ctx->dma->inflight))
			wake_up_all(&ctx->dma->idle_wait);
		kfree(ctx);
		return;
	}

	if (atomic_read(&ctx->dma->stopping)) {
		if (atomic_dec_and_test(&ctx->dma->inflight))
			wake_up_all(&ctx->dma->idle_wait);
		kfree(ctx);
		return;
	}

	frame_ok = (ctx->residue == 0);

	if (!frame_ok)
		pr_warn_ratelimited("FPGAlix: short frame — expected %zu, got %zu bytes\n",
				    ctx->expected_len,
				    ctx->expected_len - ctx->residue);

	if (atomic_dec_and_test(&ctx->dma->inflight))
		wake_up_all(&ctx->dma->idle_wait);
	kfree(ctx);
	cb(cb_param, frame_ok);
}

/*
 * Result callback — runs in tasklet context.
 * Captures residue from the hardware descriptor before it is recycled.
 */
static void FPGAlix_dma_result_cb(void *param,
				  const struct dmaengine_result *result)
{
	FPGAlix_dma_ctx_t *ctx = param;

	ctx->residue = (result != NULL) ? result->residue : 0;
	if (fpgalix_ignore_residue)
		ctx->residue = 0;

	if (ctx->dma && ctx->dma->wq)
		queue_work(ctx->dma->wq, &ctx->work);
	else
		queue_work(system_wq, &ctx->work);
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
	atomic_set(&dma->inflight, 0);
	atomic_set(&dma->chan_seq, 0);
	init_waitqueue_head(&dma->idle_wait);

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

	/* Do NOT call dmaengine_terminate_sync here: on the Altera mSGDMA it
	 * sets STOP_DISPATCHER in the CSR, which prevents the next streaming
	 * session from issuing new descriptors.  Instead, set stopping so
	 * in-flight work items skip callbacks, drain the workqueue, then bump
	 * chan_seq so any late completions are silently discarded. */
	atomic_set(&dma->stopping, 1);
	drain_workqueue(dma->wq);
	atomic_set(&dma->inflight, 0);
	atomic_inc(&dma->chan_seq);
	wake_up_all(&dma->idle_wait);
	atomic_set(&dma->stopping, 0);
}

void FPGAlix_dma_release(FPGAlix_dma_chan_t *dma)
{
	if (!dma->chan)
		return;
	atomic_set(&dma->stopping, 1);
	dmaengine_terminate_sync(dma->chan);
	destroy_workqueue(dma->wq);
	dma->wq = NULL;
	dma_release_channel(dma->chan);
	dma->chan = NULL;
}

int FPGAlix_dma_submit(FPGAlix_dma_chan_t *dma, dma_addr_t addr,
		       size_t len, FPGAlix_dma_callback_t cb, void *cb_param,
		       u32 stream_seq)
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
	ctx->stream_seq   = stream_seq;
	ctx->chan_seq     = atomic_read(&dma->chan_seq);
	ctx->expected_len = len;
	ctx->residue      = 0;
	INIT_WORK(&ctx->work, FPGAlix_dma_work);

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

	atomic_inc(&dma->inflight);
	dma_async_issue_pending(dma->chan);
	return 0;
}

int FPGAlix_dma_wait_idle(FPGAlix_dma_chan_t *dma, unsigned long timeout_ms)
{
	long ret;

	if (timeout_ms == 0) {
		wait_event(dma->idle_wait, atomic_read(&dma->inflight) == 0);
		return 0;
	}

	ret = wait_event_timeout(dma->idle_wait,
				 atomic_read(&dma->inflight) == 0,
				 msecs_to_jiffies(timeout_ms));
	if (ret == 0)
		return -ETIMEDOUT;
	return 0;
}
