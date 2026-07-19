# 04. camera_driver_2

## 1. Introduction

`sw/camera_driver_2` is a V4L2 driver for the OV7670 camera, capturing frames through the Altera mSGDMA engine ([01. Hardware Overview](01_Hardware.md)) into a standard `/dev/videoN` device. Its development was assisted by Claude Code, an AI coding assistant, including the kernel-side analysis described in [Section 4](#4-detecting-short-frames).

## 2. Building

```bash
cd sw/camera_driver_2
export CROSS_COMPILE=arm-none-linux-gnueabihf-
make
```

This produces `bin/fpgalix_camera.ko`, cross-compiled against the kernel source tree ([02. Board Bring-up, Section 6](02_BoardBringUp.md#6-building-the-linux-kernel)).

## 3. V4L2 buffer allocation

V4L2 (Video4Linux2) is the Linux kernel's video capture framework, and videobuf2 (vb2) is the buffer-management layer most V4L2 drivers, including this one, are built on. vb2 supports several memory models for exchanging frame buffers with userspace: `MMAP` (buffers allocated by the driver, mapped into the application's address space), `USERPTR` (buffers allocated by the application, their address passed to the driver instead), and `DMABUF` (buffers imported from another device).

This driver uses `MMAP`, backed by vb2's DMA-contiguous allocator (`vb2_dma_contig_memops`): frame buffers are allocated by the driver itself, each one internally contiguous in physical memory, and userspace only maps them for reading, through `mmap()`, rather than allocating or owning them. This follows directly from a hardware constraint rather than a design preference: the mSGDMA engine writes to a single physical start address and length per transfer, so the buffer it writes each individual frame into must itself be physically contiguous, a guarantee arbitrary userspace-allocated memory cannot provide; the several buffers making up the capture ring need not be contiguous with one another.

## 4. Detecting short frames

The Avalon-ST pipeline feeding the mSGDMA ([01. Hardware Overview, Section 5](01_Hardware.md#5-the-avalon-st-pipeline-from-ov7670_data_interface-to-msgdma)) marks the end of every frame with an EOP (End-Of-Packet) marker. If EOP arrives before the buffer mSGDMA was writing to is full, the result is a short frame, and the Altera mSGDMA driver already present in the Linux kernel (`drivers/dma/altera-msgdma.c`), written independently of this project, gave `camera_driver_2` no way to detect this.

This problem was found empirically, through trial and error: streaming from the VHDL test pattern generators under `hw/my_vhdl/qsys_components/` showed frames coming out wrong. Tracing it to its root cause was then done by setting Claude Code to analyse the driver's source directly, over considerable iteration, several analysis passes rather than a single one, before the fault was narrowed down to three compounding problems: the DMA descriptors were not even configured to react to an early EOP in the first place; once that was corrected, the completion information the hardware did report for each transfer was still being read and then discarded, rather than passed up to `camera_driver_2`; and the first check used to detect this, once it was passed up, proved unreliable on some mSGDMA hardware revisions and had to be made more robust. All three were fixed.

With this in place, `camera_driver_2` can tell a complete frame from a short one, and, as intended, logs and discards the latter rather than passing it to userspace as valid.

A further, unrelated problem in the same driver caused the DMA engine to stall permanently after the very first short frame, and a second capture session to start from corrupted internal state left behind by the first. Both were fixed as well, and are documented, with the exact patch, in [camera_driver_2/README.md, "Kernel patch (mandatory)"](../../sw/camera_driver_2/README.md#kernel-patch-mandatory).

> **Note:** All four changes are already committed to this project's Linux kernel fork (`linux-socfpga`), which arrives as a submodule ([02. Board Bring-up, Section 4](02_BoardBringUp.md#4-getting-the-repository)). No manual patching is required.

## 5. Current limitations

The driver, as it stands, supports a single configuration: Bayer mosaic (`SRGGB8`), 640×480, 30 fps, matching the OV7670's native sensor output and this project's Avalon-ST pipeline. Other pixel formats and resolutions are left to a future version.

The OV7670 sensor itself is neither initialized nor reset by the driver: this is done separately, over I2C, before the driver is loaded or a stream started, using the external utilities described in [Section 6](#6-usage).

## 6. Usage

Beyond the DE1-SoC board itself, this requires an OV7670 camera module and the custom camera adapter connecting it to the board's `GPIO_0` header ([01. Hardware Overview, Section 2.2](01_Hardware.md#22-rationale-for-a-custom-adapter-board)).

> **Warning:** do not leave the camera in RESET or POWER-DOWN for extended periods. A hardware bug in the adapter's line buffer can cause it to overheat and be permanently damaged. See [`WARNING.md`](../../WARNING.md) for the full explanation and the two available fixes.

[Section 6.1](#61-deploying-the-module-and-utilities) shows one way of getting the files onto the board, over `scp`; any other transfer method works just as well. If transferring over SSH, though, it has to be as the regular user created in [02. Board Bring-up, Section 11.1](02_BoardBringUp.md#111-logging-in-and-network-access): `root` has no working SSH login. Every step from [Section 6.2](#62-releasing-the-pclk-domain-reset) onward needs `root`, since loading a kernel module and accessing hardware registers directly both require it, reached one of two ways:

- from that same SSH session, with `su` (the regular user's own password does not work here: it is the `root` password set in [02. Board Bring-up, Section 7.1.1](02_BoardBringUp.md#711-base-configuration));
- from the serial console instead, already logged in as `root`; in this case `cd` into wherever the files were transferred to first, since the `root` login starts elsewhere.

### 6.1 Deploying the module and utilities

The compiled module ([Section 2](#2-building)) and the debug scripts used in the following steps are copied onto the board together, over `scp`, into a dedicated `camera_driver_2/` folder in the regular user's home directory:

```bash
cd sw/camera_driver_2
ssh <username>@<board-ip> mkdir -p camera_driver_2
scp -p bin/fpgalix_camera.ko utils/debug/resetCtrl.sh utils/debug/ov7670_ctrlif_set.sh utils/debug/ov7670_dataif_ctrl.sh <username>@<board-ip>:camera_driver_2/
```

On the board, `cd` into that folder, then `su` switches to `root` for the remaining steps, keeping the same working directory:

```bash
cd camera_driver_2
su
```

### 6.2 Releasing the pclk domain reset

```bash
./resetCtrl.sh
```

Option 2, "Deassert reset", is selected. This releases `pclk_reset_controller`'s `sw_release` bit ([01. Hardware Overview, Section 3.3](01_Hardware.md#33-usage-sequence)), a prerequisite for the camera to respond over I2C at all.

### 6.3 Configuring the OV7670 over I2C

```bash
./ov7670_ctrlif_set.sh
```

The Bayer mode is selected (`1` for Raw, `2` for Processed). Registers are written and then read back automatically, confirming the camera answered correctly.

### 6.4 Enabling frame acquisition

```bash
./ov7670_dataif_ctrl.sh
```

Option 1, "Enable frame acquisition", is selected.

### 6.5 Loading the driver

```bash
insmod fpgalix_camera.ko
```

### 6.6 Viewing the stream

`utils/debug/view.py` is a small OpenCV viewer, run on a PC, that displays the raw frames read from the board's `/dev/video0` over SSH:

```bash
ssh <username>@<board-ip> "v4l2-ctl -d /dev/video0 --stream-mmap --stream-to=-" | python3 view.py -dwg
```

`v4l2-ctl`, run on the board, streams raw frames to its own standard output instead of a file; piped over SSH, `view.py` reads them from its standard input and displays them. Its processing modes (`-r`/`-d`/`-dw`/`-dwg`/`-F`) are documented in [camera_driver_2/README.md, "view.py — Live preview"](../../sw/camera_driver_2/README.md#viewpy--live-preview). Its dependencies, `python3`, `python3-numpy` and `python3-opencv`, are already covered in [00. Prepare Your Dev Environment, Section 2.1](00_PrepareDevEnvironment.md#21-apt-packages).

`v4l2-ctl` itself reads directly from `/dev/video0`, and needs no elevated privileges beyond read access to that device node.

## 7. References

- [camera_driver_2/README.md](../../sw/camera_driver_2/README.md), this component's own documentation. Exact kernel patch (Section 4) and `view.py` processing modes (Section 6.6).
- [`WARNING.md`](../../WARNING.md). The camera adapter's line buffer hardware bug (Section 6).
