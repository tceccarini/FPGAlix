# camera_driver_2

V4L2 driver for OV7670 camera on Cyclone V SoC.  
Captures 640×480 Bayer BGGR8 at 30 fps via Altera mSGDMA (S2MM).

Rewrite of `camera_driver` incorporating fixes discovered during testing:
- kernel patch to `altera-msgdma.c` required (see below)
- stop/restart streaming works without rmmod/insmod
- short frames are logged and discarded, not passed to userspace

---

## Kernel patch (mandatory)

Two bugs in `drivers/dma/altera-msgdma.c` must be fixed before loading this driver.
The patch is in `repos/linux-socfpga` on the current branch.

**1. Remove `STOP_ON_EARLY_TERMINATION` from `msgdma_reset()`**

With `END_ON_EOP` enabled on descriptors (S2MM from an Avalon-ST source), the
hardware halts the dispatcher on every short frame. Short frames must be handled
in software via the response FIFO; removing this bit keeps the dispatcher running.

**2. Add `msgdma_reset()` at the start of `alloc_chan_resources()`**

Reinitialises all descriptor lists and resets the hardware between sessions.
Without this, releasing and re-requesting the DMA channel (done by this driver
at each stop_streaming) leaves the dispatcher in a stale state.

---

## Kernel configuration (make menuconfig)

### DMA Engine — mandatory
```
Device Drivers
  └─ DMA Engine support                        [CONFIG_DMADEVICES=y]
       └─ Altera mSGDMA Engine                 [CONFIG_ALTERA_MSGDMA=y]
```

### Video4Linux2 — mandatory
```
Device Drivers
  └─ Multimedia support                        [CONFIG_MEDIA_SUPPORT=y]
```
> Leave **Filter media drivers** unchecked. `MEDIA_CAMERA_SUPPORT` and
> `VIDEO_DEV` are auto-enabled.

### videobuf2 DMA-contiguous allocator — mandatory

`CONFIG_VIDEOBUF2_DMA_CONTIG` has no menuconfig entry (hidden symbol).
Enable a lightweight platform driver that selects it automatically:

```
Device Drivers
  └─ Multimedia support
       └─ Media drivers
            └─ V4L platform devices              [CONFIG_V4L_PLATFORM_DRIVERS=y]
                 └─ Aspeed AST2400/AST2500 Video Engine  [CONFIG_VIDEO_ASPEED=y]
```

`VIDEO_ASPEED=y` causes the kernel to build `videobuf2-dma-contig.ko` as a
side effect. The Aspeed module itself never needs to be loaded on the target.

### I2C Master Intel FPGA IP — required for FPGA I2C bus

```
Device Drivers
  └─ I2C support
       └─ I2C Hardware Bus support
            └─ Altera Soft IP I2C                 [CONFIG_I2C_ALTERA=y]
```

Enables the `i2c-altera` driver for the `altera_avalon_i2c` Qsys component
(`compatible = "altr,softip-i2c-v1.0"`). Without this the FPGA I2C bus is
invisible to Linux and the camera cannot be configured from userspace when
I2C is routed through the FPGA instead of the HPS.

### CMA — recommended

```
Memory Management options
  └─ Contiguous Memory Allocator               [CONFIG_CMA=y]

Library routines
  └─ DMA Contiguous Memory Allocator           [CONFIG_DMA_CMA=y]
       └─ Size in Mega Bytes                   16
```

Minimum required: 3 buffers × 640 × 480 × 1 byte ≈ 1 MB. The default 16 MB
is sufficient.

---

## Buildroot — userspace packages

```
Target packages
  └─ Libraries
       └─ Hardware handling
            └─ libv4l                             [BR2_PACKAGE_LIBV4L=y]
                 └─ v4l-utils tools              [BR2_PACKAGE_LIBV4L_UTILS=y]
```
> Provides `v4l2-ctl`.

```
Target packages
  └─ Networking applications
       └─ openssh                [BR2_PACKAGE_OPENSSH=y]
```
> For `scp` deployment. Alternatively `dropbear` (`BR2_PACKAGE_DROPBEAR=y`).

> `insmod` / `rmmod` are provided by BusyBox (included by default).

---

## Build

```bash
# from camera_driver_2/
make

# output: bin/fpgalix_camera.ko
```

Cross-compiler: `arm-none-linux-gnueabihf-`  
Kernel source: `../../repos/linux-socfpga`

```bash
make clean
```

---

## Deploy

```bash
scp bin/fpgalix_camera.ko root@<ip-target>:/tmp/
```

---

## Utilizzo

### Caricamento
```bash
insmod /tmp/fpgalix_camera.ko
dmesg | tail -5
# atteso: FPGAlix: DMA channel acquired: dma0chan0
#         FPGAlix: camera ready on video0
```

Il parametro `ignore_residue` (default `0`) controlla il log dei frame corti:
```bash
# disabilitare il log (utile se i frame corti sono attesi e numerosi)
insmod /tmp/fpgalix_camera.ko ignore_residue=1
```

### Verifica formato
```bash
v4l2-ctl -d /dev/video0 --get-fmt-video
# Width/Height: 640/480
# Pixel Format: 'BA81' (SBGGR8)
```

### Live preview su PC

Sul PC (richiede `numpy` e `opencv-python`):
```bash
ssh root@192.168.0.238 \
  "v4l2-ctl -d /dev/video0 --set-fmt-video=width=640,height=480,pixelformat=BA81 \
   --stream-mmap --stream-to=- 2>/dev/null" \
  | python3 utils/view.py
```

Premere `q` nella finestra per fermare, oppure `Ctrl+C` nel terminale.

Lo stream può essere fermato e riavviato senza rmmod/insmod: al prossimo
`STREAMON` il driver rilascia e riacquisisce il canale DMA, ottenendo un
reset hardware pulito tramite `alloc_chan_resources`.

### Test streaming su file (10 frame)
```bash
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=10 --stream-to=/tmp/raw.bin
```

### Ispezione frame in Python
```python
import numpy as np, cv2
raw = np.fromfile('/tmp/raw.bin', dtype=np.uint8)
frame = raw[:640*480].reshape(480, 640)
bgr = cv2.cvtColor(frame, cv2.COLOR_BayerBG2BGR)
cv2.imwrite('/tmp/frame.png', bgr)
```

### Scaricamento
```bash
rmmod fpgalix_camera
```

---

## Diagnostica

| Sintomo | Causa probabile |
|---|---|
| `no DMA channel available` | `CONFIG_ALTERA_MSGDMA` non abilitato o overlay DT mancante |
| `v4l2_device_register failed` | `CONFIG_VIDEO_DEV` non abilitato |
| `vb2_queue_init failed` | `CONFIG_VIDEOBUF2_DMA_CONTIG` non abilitato |
| DMA timeout dopo 30 s al secondo stream | Kernel non patchato (`alloc_chan_resources` non fa reset) |
| DMA timeout dopo 30 s al primo stream | Pattern generator VHDL non attivo o clock mancante |
| Frame tutti corti (log continuo) | Risoluzione o timing Avalon-ST non corrispondente a 640×480×1 |
| Colori sbagliati con sensore reale | Verificare Bayer order: cambiare `FPGALIX_PIXEL_FMT` in `config.h` |
