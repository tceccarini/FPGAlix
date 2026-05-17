# TODO

## ✓ Completed — driver functional with pattern generator (2026-05-17)

`fpgalix_camera.ko` loads, acquires dma0chan0, registers /dev/video0,
captures raw Bayer frames from the VHDL counter pattern generator.

---

---

## Verify Bayer pattern with real OV7670

`FPGALIX_PIXEL_FMT` in `src/config.h` is currently set to `V4L2_PIX_FMT_SBGGR8` (BGGR),
based on the OV7670 datasheet ("row-alternating BG/GR Bayer pattern").

The Linux OV7670 kernel driver uses `GBRG` instead, which differs by one pixel column.
An off-by-one capture offset in the FPGA logic would produce the same shift.

Once the real sensor is connected, capture a frame and inspect the colours.
If they look wrong, change the define to `V4L2_PIX_FMT_SGBRG8` and recompile.

## Move driver in-tree

When the driver is stable, move it into the kernel source tree to get proper
Kconfig integration (automatic dependency resolution via `select`).

Steps:
1. Copy sources to `repos/linux-socfpga/drivers/media/platform/fpgalix/`
2. Add `Kconfig` with `select VIDEOBUF2_DMA_CONTIG` — eliminates the hidden symbol workaround
3. Add `Makefile` with `obj-$(CONFIG_VIDEO_FPGALIX) += camera.o msgdma.o sensor.o`
4. Hook into parent: one line in `drivers/media/platform/Kconfig` and one in `drivers/media/platform/Makefile`

Do this after the driver is validated with the real OV7670 sensor.

---

## Implement sensor init (sensor.c)

`FPGAlix_sensor_init()` and `FPGAlix_sensor_release()` are currently stubs.
Implement:
- OV7670 I2C register configuration
- FPGA acquisition module enable/disable
