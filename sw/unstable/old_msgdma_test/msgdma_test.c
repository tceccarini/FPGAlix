// SPDX-License-Identifier: GPL-2.0
/*
 * msgdma_test.c - mSGDMA S2MM interrupt-driven test
 *
 * DEV_TO_MEM: counter_source → mSGDMA → kmalloc buffer
 * Expect incrementing 64-bit values (0, 1, 2, ...) if the FPGA counter is
 * running with INITIAL_VALUE=0 and INCREMENT=1.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>

#define XFER_LEN  512

static DECLARE_COMPLETION(dma_done);

static void dma_callback(void *arg)
{
	complete(&dma_done);
}

static int __init msgdma_test_init(void)
{
	dma_cap_mask_t mask;
	struct dma_chan *chan;
	struct dma_slave_config cfg;
	struct dma_async_tx_descriptor *tx;
	struct scatterlist sg;
	dma_cookie_t cookie;
	void *buf;
	int i, ret, mapped;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	chan = dma_request_channel(mask, NULL, NULL);
	if (!chan) {
		pr_err("msgdma_test: no channel\n");
		return -ENODEV;
	}
	pr_info("msgdma_test: channel %s\n", dma_chan_name(chan));

	buf = kmalloc(XFER_LEN, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto err_release;
	}
	memset(buf, 0, XFER_LEN);

	sg_init_one(&sg, buf, XFER_LEN);
	mapped = dma_map_sg(chan->device->dev, &sg, 1, DMA_FROM_DEVICE);
	if (!mapped) {
		pr_err("msgdma_test: dma_map_sg failed\n");
		ret = -ENOMEM;
		goto err_free;
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.direction      = DMA_DEV_TO_MEM;
	cfg.src_addr_width = DMA_SLAVE_BUSWIDTH_8_BYTES;
	cfg.dst_addr_width = DMA_SLAVE_BUSWIDTH_8_BYTES;
	ret = dmaengine_slave_config(chan, &cfg);
	if (ret) {
		pr_err("msgdma_test: slave_config failed %d\n", ret);
		goto err_unmap;
	}

	tx = dmaengine_prep_slave_sg(chan, &sg, 1, DMA_DEV_TO_MEM,
				     DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!tx) {
		pr_err("msgdma_test: prep_slave_sg failed\n");
		ret = -ENXIO;
		goto err_unmap;
	}
	tx->callback       = dma_callback;
	tx->callback_param = NULL;

	reinit_completion(&dma_done);
	cookie = dmaengine_submit(tx);
	if (dma_submit_error(cookie)) {
		pr_err("msgdma_test: submit error\n");
		ret = -EIO;
		goto err_unmap;
	}
	dma_async_issue_pending(chan);

	if (!wait_for_completion_timeout(&dma_done, HZ * 5)) {
		pr_err("msgdma_test: timeout!\n");
		dmaengine_terminate_sync(chan);
		ret = -ETIMEDOUT;
		goto err_unmap;
	}

	dma_unmap_sg(chan->device->dev, &sg, 1, DMA_FROM_DEVICE);

	pr_info("msgdma_test: OK — first 8 beats (64-bit):\n");
	for (i = 0; i < 8; i++)
		pr_info("  [%d] 0x%016llx%s\n", i, ((u64 *)buf)[i],
			((u64 *)buf)[i] == (u64)i ? "" : " *** MISMATCH ***");

	kfree(buf);
	dma_release_channel(chan);
	return 0;

err_unmap:
	dma_unmap_sg(chan->device->dev, &sg, 1, DMA_FROM_DEVICE);
err_free:
	kfree(buf);
err_release:
	dma_release_channel(chan);
	return ret;
}

static void __exit msgdma_test_exit(void) {}

module_init(msgdma_test_init);
module_exit(msgdma_test_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Altera mSGDMA S2MM interrupt-driven test");
