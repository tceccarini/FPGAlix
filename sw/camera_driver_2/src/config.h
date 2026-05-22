/* SPDX-License-Identifier: GPL-2.0 */
#ifndef FPGALIX_CONFIG_H
#define FPGALIX_CONFIG_H

#include <linux/dmaengine.h>
#include <media/v4l2-common.h>

/* ---- Frame geometry ---------------------------------------------------- */
#define FPGALIX_FRAME_W         640
#define FPGALIX_FRAME_H         480
#define FPGALIX_FRAME_BPP       1
#define FPGALIX_FRAME_LEN       (FPGALIX_FRAME_W * FPGALIX_FRAME_H * FPGALIX_FRAME_BPP)

/* ---- Buffer management ------------------------------------------------- */
/* Minimum number of vb2 buffers: one in DMA, one with userspace, one queued */
#define FPGALIX_MIN_BUFFERS     3

/* ---- Pixel format ------------------------------------------------------- */
#define FPGALIX_PIXEL_FMT       V4L2_PIX_FMT_SGRBG8

/* ---- Frame rate --------------------------------------------------------- */
#define FPGALIX_FPS_NUM         1
#define FPGALIX_FPS_DEN         30

/* ---- Device identification --------------------------------------------- */
#define FPGALIX_DRV_NAME        "fpgalix_camera"
#define FPGALIX_CARD_NAME       "FPGAlix Camera"
#define FPGALIX_BUS_INFO        "platform:fpgalix"

/* ---- Watchdog ---------------------------------------------------------- */
/* Time (ms) with no DMA callback before the stream is declared dead. */
#define FPGALIX_DMA_TIMEOUT_MS      30000

/* ---- DMA --------------------------------------------------------------- */
/* Bus width must match the mSGDMA data width set in the FPGA Qsys design */
#define FPGALIX_DMA_BUS_WIDTH   DMA_SLAVE_BUSWIDTH_8_BYTES

#endif /* FPGALIX_CONFIG_H */
