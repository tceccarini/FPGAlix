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
static bool fpgalix_ignore_residue = true;
module_param_named(ignore_residue, fpgalix_ignore_residue, bool, 0644);
MODULE_PARM_DESC(ignore_residue, "Ignore DMA residue and treat all frames as complete");

typedef struct {
	FPGAlix_dma_chan_t     *dma;
	FPGAlix_dma_callback_t  cb;
	void                   *cb_param;
	u32                     stream_seq;
	u32                     chan_seq;    /* snapshot of dma->chan_seq at submit time */
	size_t                  expected_len;
	u32                     residue;
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
	FPGAlix_cam_buf_t     *buf      = cb_param;
	bool                   frame_ok;

	pr_debug("FPGAlix_dma_work: ctx=%p cb_param=%p expected=%zu\n",
			 ctx, cb_param, ctx->expected_len);

	/* Drop work items from a previous stop/start cycle without touching
	 * inflight: dma_stop already reset it to 0 and incremented chan_seq. */
	if (atomic_read(&ctx->dma->chan_seq) != ctx->chan_seq) {
		kfree(ctx);
		return;
	}

	if (!buf || !buf->cam) {
		pr_warn("FPGAlix_dma_work: missing buffer/cam, dropping ctx=%p cb_param=%p\n", ctx, cb_param);
		if (atomic_dec_and_test(&ctx->dma->inflight))
			wake_up_all(&ctx->dma->idle_wait);
		kfree(ctx);
		return;
	}

	if (!buf->queued || buf->stream_seq != ctx->stream_seq) {
		pr_debug("FPGAlix_dma_work: stale completion ctx=%p buf=%p queued=%d buf_seq=%u ctx_seq=%u streaming=%d\n",
			 ctx, buf, buf->queued, buf->stream_seq, ctx->stream_seq, buf->cam->streaming);
		if (atomic_dec_and_test(&ctx->dma->inflight))
			wake_up_all(&ctx->dma->idle_wait);
		kfree(ctx);
		return;
	}

	if (atomic_read(&ctx->dma->stopping)) {
		pr_debug("FPGAlix_dma_work: dma stopping, skipping cb ctx=%p cb_param=%p\n", ctx, cb_param);
		if (atomic_dec_and_test(&ctx->dma->inflight))
			wake_up_all(&ctx->dma->idle_wait);
		kfree(ctx);
		/* Do not call cb: stop_streaming's active_list cleanup handles
		 * the buffer, and calling vb2_buffer_done(DONE) during teardown
		 * would corrupt vb2 state. */
		return;
	}

	frame_ok = (ctx->residue == 0);

	if (!frame_ok)
		pr_warn_ratelimited("FPGAlix: short frame — expected %zu bytes, arrived %zu bytes, discarding\n",
				    ctx->expected_len,
				    ctx->expected_len - ctx->residue);

	pr_debug("FPGAlix_dma_work: invoking cb ctx=%p cb_param=%p frame_ok=%d residue=%u\n",
			 ctx, cb_param, frame_ok, ctx->residue);
	if (atomic_dec_and_test(&ctx->dma->inflight))
		wake_up_all(&ctx->dma->idle_wait);
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
	if (fpgalix_ignore_residue)
		ctx->residue = 0;

	/*
	 * The dma->wq may be destroyed during module teardown; avoid
	 * queueing work on a freed pointer. If the DMA workqueue is
	 * unavailable, fall back to `system_wq` so the deferred work
	 * still runs in process context.
	 */
	pr_debug("FPGAlix_dma_result_cb: ctx=%p cb_param=%p seq=%u dma_wq=%p\n", ctx, ctx->cb_param,
			 ctx->stream_seq, ctx->dma ? ctx->dma->wq : NULL);
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
	 * session from processing new descriptors.  Instead:
	 *  1. Set stopping so in-flight work items skip vb2 callbacks.
	 *  2. Drain work items that are already queued.
	 *  3. Reset inflight and bump chan_seq.  Any work items that arrive
	 *     late (orphaned descriptors from this cycle completing after
	 *     chan_seq is incremented) are silently dropped by FPGAlix_dma_work
	 *     without touching inflight, so the next session is unaffected.
	 * terminate_sync is reserved for module release (FPGAlix_dma_release). */
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
	/* prevent future use of the freed pointer */
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
