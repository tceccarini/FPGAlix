# TODO

## ✓ Completato (2026-05-18)

- Driver funzionante con pattern generator VHDL (640×480, 30 fps)
- Stop/restart streaming senza rmmod/insmod
- Frame corti: loggati e scartati, non passati a userspace
- Kernel patchato: rimosso `STOP_ON_EARLY_TERMINATION`, aggiunto
  `msgdma_reset()` in `alloc_chan_resources()`

---

## ✓ Bayer order verificato con OV7670 reale (2026-05-22)

`FPGALIX_PIXEL_FMT` aggiornato a `V4L2_PIX_FMT_SRGGB8` (RGGB).
Verificato empiricamente su frame reale — pattern BayerRG in OpenCV.

---

## Implementare sensor.c

`FPGAlix_sensor_init()` e `FPGAlix_sensor_release()` sono stub.
Da implementare:
- configurazione registri OV7670 via I2C
- abilitazione/disabilitazione del modulo di acquisizione FPGA

---

## Portare il driver in-tree

Quando il driver è stabile, spostarlo nel kernel source tree:

1. Copiare i sorgenti in `repos/linux-socfpga/drivers/media/platform/fpgalix/`
2. Aggiungere `Kconfig` con `select VIDEOBUF2_DMA_CONTIG`
3. Aggiungere `Makefile` con `obj-$(CONFIG_VIDEO_FPGALIX) += camera.o msgdma.o sensor.o`
4. Agganciare al parent: una riga in `drivers/media/platform/Kconfig` e una in `Makefile`
