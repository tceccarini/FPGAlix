# camera_driver_2

V4L2 driver for OV7670 camera on Cyclone V SoC.  
Captures 640×480 RGGB Bayer (SRGGB8) at 30 fps via Altera mSGDMA (S2MM).

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
# Pixel Format: 'RGGB' (SRGGB8)
```

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
bgr = cv2.cvtColor(frame, cv2.COLOR_BayerRG2BGR_EA)
cv2.imwrite('/tmp/frame.png', bgr)
```

### Scaricamento
```bash
rmmod fpgalix_camera
```

---

## Debug utilities

Scripts and tools in `utils/debug/`.

### End-to-end example

All commands run on the target unless noted otherwise.

**1. Release pclk domain reset**
```bash
./resetCtrl.sh          # choose 2 — Deassert reset
```

**2. Configure OV7670 via I2C**
```bash
./ov7670_ctrlif_set.sh  # choose Bayer mode (1 = Raw, 2 = Processed)
```
Registers are written and read back automatically at the end.

**3. Enable frame acquisition**
```bash
./ov7670_dataif_ctrl.sh  # choose 1 — Enable frame acquisition
```

**4. Load the kernel module**
```bash
insmod /tmp/fpgalix_camera.ko
```

**5. Stream to PC and display**
```bash
# run on PC:
ssh root@192.168.0.238 \
  "v4l2-ctl -d /dev/video0 --stream-mmap --stream-to=- 2>/dev/null" \
  | python3 utils/debug/view.py -dwg
```

---

### ov7670_ctrlif_set.sh — Camera initialisation

Programs the minimal OV7670 register set for VGA Bayer RGB at 30 fps over
SCCB (I2C bus 1, address 0x21), then reads back every register to verify.

#### Register 0xB0 — Processed Bayer RGB

When `COM7 = 0x05` (Processed Bayer RGB), register **0xB0 must be set to 0x8C**.
This register is undocumented in the OV7670 datasheet. Without it the image
has a strong green cast and is washed out regardless of any white-balance
adjustment. `ov7670_ctrlif_set.sh` applies this write automatically when
Processed Bayer mode is selected.

> **Provenance note.** Register 0xB0 does not appear in the official Omnivision
> OV7670 documentation [datasheet v1.4, 2006]. The value 0x8C is derived from
> the Linux mainline driver `drivers/media/i2c/ov7670.c`, where a similar write
> is explicitly labelled a *"magic reserved value"*. The corrective effect on
> colour channels (R/G) is empirically documented by Połeć (2012) and in the
> ArduCAM implementation, where the variant 0x8C also appears.
>
> **References**
> 1. Omnivision Technologies, *OV7670/OV7171 CMOS VGA CameraChip Implementation
>    Guide*, v1.0, 2005 (NDA; publicly leaked).
> 2. Omnivision Technologies, *OV7670/OV7171 Datasheet*, v1.4, August 2006 —
>    <https://people.ece.cornell.edu/land/courses/ece4760/FinalProjects/f2021/jfw225_aei23_dsb298/jfw225_aei23_dsb298/OV7670_2006.pdf>
> 3. Linux kernel, `drivers/media/i2c/ov7670.c` — in-tree; original author
>    Jonathan Corbet (2008); commit history on kernel.org.
> 4. Połeć J., "OV7670 YUV demystified", *ThinkSmallThings* blog, 3 Nov 2012.
> 5. ArduCAM, *Arduino/OV7670FIFO* repository, `OV7670FIFO.ino`,
>    function `SetupCameraUndocumentedRegisters()`.

### view.py — Live preview

Reads raw Bayer frames from stdin and displays them in a window. A processing
mode flag is mandatory; `-F` adds fullscreen.

| Flag   | Pipeline                                                |
|--------|---------------------------------------------------------|
| `-r`   | Raw Bayer as greyscale — no processing                  |
| `-d`   | Edge-Aware demosaicing (RGGB pattern)                   |
| `-dw`  | Demosaicing + per-channel white balance (1–99% stretch) |
| `-dwg` | Demosaicing + white balance + gamma 2.2                 |
| `-F`   | Fullscreen (combinable with any mode)                   |

```bash
ssh root@192.168.0.238 \
  "v4l2-ctl -d /dev/video0 --stream-mmap --stream-to=- 2>/dev/null" \
  | python3 utils/debug/view.py -dwg
```

Press `q` to quit, or `Ctrl+C` in the terminal.

---

## Diagnostics

| Sintomo | Causa probabile |
|---|---|
| `no DMA channel available` | `CONFIG_ALTERA_MSGDMA` non abilitato o overlay DT mancante |
| `v4l2_device_register failed` | `CONFIG_VIDEO_DEV` non abilitato |
| `vb2_queue_init failed` | `CONFIG_VIDEOBUF2_DMA_CONTIG` non abilitato |
| DMA timeout dopo 30 s al secondo stream | Kernel non patchato (`alloc_chan_resources` non fa reset) |
| DMA timeout dopo 30 s al primo stream | Pattern generator VHDL non attivo o clock mancante |
| Frame tutti corti (log continuo) | Risoluzione o timing Avalon-ST non corrispondente a 640×480×1 |
| Colori sbagliati con sensore reale | Verificare Bayer order: cambiare `FPGALIX_PIXEL_FMT` in `config.h` |
