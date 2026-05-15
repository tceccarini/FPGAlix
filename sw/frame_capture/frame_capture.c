// SPDX-License-Identifier: GPL-2.0
/*
 * frame_capture.c - Three consecutive frame captures via mSGDMA (S2MM)
 *
 * Captures three complete frames (FRAME_W x FRAME_H x BYTES_PER_PIXEL bytes each).
 * Saves raw buffers to /tmp/buf1.dump, /tmp/buf2.dump, /tmp/buf3.dump.
 * Prints first and last 5 bytes of each buffer.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define FRAME_W          640
#define FRAME_H          480
#define BYTES_PER_PIXEL  1
#define FRAME_LEN        (FRAME_W * FRAME_H * BYTES_PER_PIXEL)
#define NUM_FRAMES       3

static DECLARE_COMPLETION(dma_done);

static void dma_callback(void *arg)
{
	complete(&dma_done);
}

static int capture_frame(struct dma_chan *chan, void *buf)
{
	struct dma_async_tx_descriptor *tx;
	struct scatterlist sg;
	dma_cookie_t cookie;
	int mapped;

	sg_init_one(&sg, buf, FRAME_LEN);
	mapped = dma_map_sg(chan->device->dev, &sg, 1, DMA_FROM_DEVICE);
	if (!mapped) {
		pr_err("frame_capture: dma_map_sg failed\n");
		return -ENOMEM;
	}

	tx = dmaengine_prep_slave_sg(chan, &sg, 1, DMA_DEV_TO_MEM,
				     DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!tx) {
		pr_err("frame_capture: prep_slave_sg failed\n");
		dma_unmap_sg(chan->device->dev, &sg, 1, DMA_FROM_DEVICE);
		return -ENXIO;
	}

	tx->callback       = dma_callback;
	tx->callback_param = NULL;

	reinit_completion(&dma_done);
	cookie = dmaengine_submit(tx);
	if (dma_submit_error(cookie)) {
		pr_err("frame_capture: submit error\n");
		dma_unmap_sg(chan->device->dev, &sg, 1, DMA_FROM_DEVICE);
		return -EIO;
	}

	dma_async_issue_pending(chan);

	if (!wait_for_completion_timeout(&dma_done, HZ * 30)) {
		pr_err("frame_capture: timeout\n");
		dmaengine_terminate_sync(chan);
		dma_unmap_sg(chan->device->dev, &sg, 1, DMA_FROM_DEVICE);
		return -ETIMEDOUT;
	}

	dma_unmap_sg(chan->device->dev, &sg, 1, DMA_FROM_DEVICE);
	return 0;
}

static int save_dump(const char *path, const void *buf, size_t len)
{
	struct file *f;
	ssize_t written;

	f = filp_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (IS_ERR(f)) {
		pr_err("frame_capture: cannot open %s: %ld\n", path, PTR_ERR(f));
		return PTR_ERR(f);
	}

	written = kernel_write(f, buf, len, &f->f_pos);
	filp_close(f, NULL);

	if (written < 0) {
		pr_err("frame_capture: write %s failed: %zd\n", path, written);
		return (int)written;
	}
	if ((size_t)written != len) {
		pr_err("frame_capture: short write %s: %zd/%zu\n", path, written, len);
		return -EIO;
	}

	pr_info("frame_capture: saved %zu bytes to %s\n", len, path);
	return 0;
}

static void print_edges(const char *label, const void *buf)
{
	const u8 *b = buf;
	int i;

	pr_info("frame_capture: --- %s (first 5 bytes) ---\n", label);
	for (i = 0; i < 5; i++)
		pr_info("  [%6d] 0x%02x\n", i, b[i]);

	pr_info("frame_capture: --- %s (last 5 bytes) ---\n", label);
	for (i = FRAME_LEN - 5; i < FRAME_LEN; i++)
		pr_info("  [%6d] 0x%02x\n", i, b[i]);
}

static const char *dump_paths[NUM_FRAMES] = {
	"/tmp/buf1.dump",
	"/tmp/buf2.dump",
	"/tmp/buf3.dump",
};

static int __init frame_capture_init(void)
{
	dma_cap_mask_t mask;
	struct dma_chan *chan;
	struct dma_slave_config cfg;
	void *bufs[NUM_FRAMES];
	char label[32];
	int i, ret;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	chan = dma_request_channel(mask, NULL, NULL);
	if (!chan) {
		pr_err("frame_capture: no DMA channel\n");
		return -ENODEV;
	}
	pr_info("frame_capture: channel %s — %dx%d x %d bytes = %d bytes/frame\n",
		dma_chan_name(chan), FRAME_W, FRAME_H, BYTES_PER_PIXEL, FRAME_LEN);

	for (i = 0; i < NUM_FRAMES; i++) {
		bufs[i] = kmalloc(FRAME_LEN, GFP_KERNEL);
		if (!bufs[i]) {
			pr_err("frame_capture: kmalloc failed (frame %d)\n", i + 1);
			ret = -ENOMEM;
			goto err_free;
		}
		memset(bufs[i], 0, FRAME_LEN);
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.direction      = DMA_DEV_TO_MEM;
	cfg.src_addr_width = DMA_SLAVE_BUSWIDTH_8_BYTES;
	cfg.dst_addr_width = DMA_SLAVE_BUSWIDTH_8_BYTES;
	ret = dmaengine_slave_config(chan, &cfg);
	if (ret) {
		pr_err("frame_capture: slave_config failed: %d\n", ret);
		goto err_free;
	}

	for (i = 0; i < NUM_FRAMES; i++) {
		pr_info("frame_capture: capturing frame %d...\n", i + 1);
		ret = capture_frame(chan, bufs[i]);
		if (ret) {
			pr_err("frame_capture: frame %d failed: %d\n", i + 1, ret);
			goto err_free;
		}
		pr_info("frame_capture: frame %d done\n", i + 1);
	}

	for (i = 0; i < NUM_FRAMES; i++) {
		save_dump(dump_paths[i], bufs[i], FRAME_LEN);
		snprintf(label, sizeof(label), "buffer %d", i + 1);
		print_edges(label, bufs[i]);
	}

	ret = 0;

err_free:
	for (i = 0; i < NUM_FRAMES; i++)
		kfree(bufs[i]);
	dma_release_channel(chan);
	return ret;
}

static void __exit frame_capture_exit(void) {}

module_init(frame_capture_init);
module_exit(frame_capture_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Three-frame capture via Altera mSGDMA S2MM");
