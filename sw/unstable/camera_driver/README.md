# camera_driver

V4L2 driver for OV7670 camera on Cyclone V SoC.  
Captures 640×480 Bayer RGGB8 at 30 fps via Altera mSGDMA (S2MM).

---

## Kernel configuration (make menuconfig)

### DMA Engine — mandatory
```
Device Drivers
  └─ DMA Engine support                        [CONFIG_DMADEVICES=y]
       └─ Altera mSGDMA Engine                 [CONFIG_ALTERA_MSGDMA=y]
```

### Video4Linux2 — mandatory

In menuconfig (kernel 6.1):
```
Device Drivers
  └─ Multimedia support                        [CONFIG_MEDIA_SUPPORT=y]
```
> Leave **Filter media drivers** unchecked (default).
> With the filter off, `MEDIA_CAMERA_SUPPORT` and `VIDEO_DEV` are
> auto-enabled — they do not appear in the menu, nothing else to do here.

### videobuf2 DMA-contiguous allocator — mandatory

`CONFIG_VIDEOBUF2_DMA_CONTIG` has no menuconfig entry (hidden symbol).
The simplest way to enable it is to activate a lightweight platform driver
that selects it automatically:

```
Device Drivers
  └─ Multimedia support
       └─ Media drivers
            └─ V4L platform devices              [CONFIG_V4L_PLATFORM_DRIVERS=y]
                 └─ Aspeed AST2400/AST2500 Video Engine  [CONFIG_VIDEO_ASPEED=y]
```

`VIDEO_ASPEED=y` causes the kernel to build `videobuf2-dma-contig.ko` as a
side effect. The Aspeed module itself never needs to be loaded on the target.

### CMA — recommended

Step 1 — enable the CMA framework:
```
Memory Management options
  └─ Contiguous Memory Allocator               [CONFIG_CMA=y]
```

Step 2 — enable the DMA allocator and check the size:
```
Library routines
  └─ DMA Contiguous Memory Allocator           [CONFIG_DMA_CMA=y]
       └─ Size in Mega Bytes                   16  (check: must be >= N_BUF × W × H × BPP)
```

Minimum required size for this driver:
```
3 buffers × 640 × 480 × 1 byte = 921600 bytes ≈ 1 MB
```
The default of 16 MB is sufficient. Increase it only if the value is smaller than 1 MB.

---

## Buildroot — userspace packages

Add in Buildroot `make menuconfig` (or directly in `configs/<board>_defconfig`):

> `insmod` and `rmmod` are provided by BusyBox, which is included in
> Buildroot by default — no extra package needed.
> If BusyBox is not present, enable the full kmod package:
> `Target packages → System tools → kmod  [BR2_PACKAGE_KMOD=y]`

```
Target packages
  └─ Libraries
       └─ Hardware handling
            └─ libv4l                             [BR2_PACKAGE_LIBV4L=y]
                 └─ v4l-utils tools              [BR2_PACKAGE_LIBV4L_UTILS=y]
```
> Provides `v4l2-ctl` to capture frames and save them to disk.

```
Target packages
  └─ Networking applications
       └─ openssh                [BR2_PACKAGE_OPENSSH=y]
```
> For deployment via `scp`. Alternatively `dropbear` (`BR2_PACKAGE_DROPBEAR=y`).

---

## Build

```bash
# dalla directory camera_driver/
make

# Risultato: bin/camera.ko
```

Cross-compiler richiesto: `arm-none-linux-gnueabihf-`  
Kernel sorgente: `../../repos/linux-socfpga`

```bash
# pulizia completa
make clean
```

---

## Deploy sul target

```bash
scp bin/fpgalix_camera.ko root@<ip-target>:/tmp/
```

---

## Utilizzo

### Caricamento
```bash
insmod /tmp/fpgalix_camera.ko
dmesg | tail -5
# atteso: FPGAlix: DMA channel acquired: ...
#         FPGAlix: camera ready on /dev/video0
```

### Verifica formato
```bash
v4l2-ctl -d /dev/video0 --get-fmt-video
# Width/Height: 640/480
# Pixel Format: 'RGGB' (SRGGB8)
```

### Test streaming (10 frame, dump su file)
```bash
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=10 \
         --stream-to=/tmp/raw.bin
```

### Ispezione raw Bayer in Python
```python
import numpy as np, cv2
raw = np.fromfile('/tmp/raw.bin', dtype=np.uint8)
frame = raw[:640*480].reshape(480, 640)
bgr = cv2.cvtColor(frame, cv2.COLOR_BayerRG2BGR)
cv2.imwrite('/tmp/frame.png', bgr)
```

### Live preview on PC via VLC (optional)

Requires `ffmpeg` on the target (`BR2_PACKAGE_FFMPEG=y` in Buildroot).

Bayer RGGB8 is 1 byte/pixel — identical to grayscale in memory. ffmpeg streams
it as MJPEG without any demosaicing. The image will look like a color mosaic
but is sufficient to verify that frames are arriving correctly.

On the target:
```bash
ffmpeg -f v4l2 -video_size 640x480 -framerate 30 -i /dev/video0 \
       -pix_fmt gray -c:v mjpeg -q:v 5 \
       -f mpjpeg -listen 1 http://0.0.0.0:8080
```

On the PC, open VLC and go to **Media → Open Network Stream**:
```
http://<target-ip>:8080/
```

### Unload
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
| Timeout DMA dopo 30 s | Pattern generator VHDL non attivo o clock mancante |
| Frame tutti scartati (EOP prematuro) | Risoluzione o timing Avalon-ST non corrispondente a 640×480×1 |
