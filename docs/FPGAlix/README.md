# FPGAlix

FPGAlix interfaces an OV7670 camera module with a Terasic DE1-SoC's
Cyclone V HPS through custom FPGA hardware, using DMA (mSGDMA) so that
transferring the video stream into memory requires no CPU involvement in
the handling of individual bytes. Unlike its bare-metal successor,
FPGAsteel, FPGAlix runs a full embedded Linux system (Buildroot) on the
HPS: a V4L2 kernel driver exposes the camera as a standard `/dev/video0`,
and a userspace pipeline, `vision_core`, filters the stream and serves it
out over RTSP. It also documents, step by step, the full toolchain and
development environment needed to build and run it.

## Quick Start

The prebuilt SD card image already contains the bootloader, kernel, root
filesystem and FPGA bitstream, letting the DE1-SoC boot straight into a
running system, without building anything from source.

<div align="center">

> **Hardware bug:** do not leave the camera in RESET or POWER-DOWN for
> extended periods. See [`WARNING.md`](../../WARNING.md).

</div>

### Hardware needed

- A Terasic DE1-SoC board.
- An OV7670 camera module.
- The custom camera adapter (transmitter and receiver boards, connected
  by a ribbon cable), wiring the camera to the DE1-SoC's `GPIO_0`
  header.
- A microSD card, at least 8 GB.
- An Ethernet cable, for network access.
- A PC on the same network, to reach the board over SSH and view the
  video stream.
- A micro-USB cable, for the board's serial console. Optional: only
  needed to log in as `root` directly, or for troubleshooting.
- A power supply for the board.

### Preparing the SD card

1. Download the SD card image from [download.md](download.md) and
   extract it.
2. Write the extracted image to the microSD card (at least 8 GB) with
   any disk-imaging tool, e.g. `dd` on Linux or balenaEtcher on
   Windows/Mac.
3. Insert the card into the DE1-SoC.

### Board configuration

Before powering on, set the DE1-SoC's **SW10** DIP switch, on the
underside of the board, so that switches 1 through 5 are ON (SW10 uses
negative logic: ON means logic 0). This lets the HPS configure the FPGA
fabric from the SD card at boot
([02. Board Bring-up, Section 8.1](02_BoardBringUp.md#81-msel-configuration-sw10)).

### Booting and logging in

Connect the Ethernet cable, insert the SD card, and power on the board.
The board's IP address is shown automatically, a few seconds later, on
the 7-segment displays, cycled one octet at a time by `hexip_daemon`
([03. hexip_daemon](03_HexipDaemon.md)).

Two ways to log in, default password `1q2w3e4r` for both accounts:

- over SSH, once the IP is known, as `wheel`, then `su` for anything
  that needs root (loading `camera_driver_2`, accessing hardware
  registers directly):
  ```bash
  ssh wheel@<board-ip>
  ```
- over the serial console (115200 baud, 8N1, e.g. `picocom`), directly
  as `root`; no network needed for this path
  ([02. Board Bring-up, Section 11](02_BoardBringUp.md#11-booting-the-board)).

### Running the software

`hexip_daemon` ([03. hexip_daemon](03_HexipDaemon.md)) is already
running once the board boots; no action needed. The prebuilt SD card
image also already contains `camera_driver_2` and `vision_core`, built
and placed in `wheel`'s own home directory
(`/home/wheel/camera_driver_2/`, `/home/wheel/vision_core/`); no
building or `scp`-ing is needed here. Building from source and deploying
over `scp`, covered in full in [04. camera_driver_2](04_CameraDriver2.md)
and [05. vision_core](05_VisionCore.md), only matters after changing
their source.

Log in as `wheel` (or `root` from the serial console, `cd`-ing to
`/home/wheel` first), then bring the camera up and load the driver
([Section 6](04_CameraDriver2.md#6-usage)):

```bash
cd camera_driver_2
su
./resetCtrl.sh          # option 2, deassert reset
./ov7670_ctrlif_set.sh  # select Bayer mode (1)
./ov7670_dataif_ctrl.sh # option 1, enable frame acquisition
insmod fpgalix_camera.ko
```

Then run `vision_core` ([Section 5](05_VisionCore.md#5-usage)), still as
root, from the sibling directory:

```bash
cd ../vision_core
./vision_core
```

It prompts interactively; every prompt accepts its default by pressing
Enter, except the source, which must be set explicitly to `ov7670`:

```
Source [webcam/ov7670, default: webcam]: ov7670
Encoding [mjpeg/h264/raw, default: mjpeg]:
```

`ov7670` is mandatory on the board, the webcam source being only for the
native `make intel` build; MJPEG, left at its default by pressing Enter,
is what this project recommends for full-color output here, the HPS
having no hardware video encoder
([05. vision_core, Section 1](05_VisionCore.md#1-introduction)).

Once running, view the stream on a PC:

```bash
ffplay -fflags nobuffer -flags low_delay -framedrop rtsp://<board-ip>:8554/stream
```

## Documentation

The full, numbered documentation starts at
[00_PrepareDevEnvironment.md](00_PrepareDevEnvironment.md) and covers, in
order, setting up the development environment, the hardware design,
board bring-up, and every application under `sw/`.

## Repository layout

- `hw/`: FPGA hardware project (Quartus Prime + Platform Designer).
  - `camera_adapter/`: custom PCB bridging the OV7670 to the DE1-SoC's
    `GPIO_0` header.
  - `my_vhdl/`: custom Platform Designer VHDL components written for
    this project.
  - `quartus/`: Quartus project, `soc_system.qsys`, top-level VHDL, pin
    assignments.
- `sw/`: application software running on the board's embedded Linux
  (C/C++), one folder per component.
  - `camera_driver_2/`: V4L2 kernel driver for the OV7670.
  - `hexip_daemon/`: displays the board's IP on the 7-segment displays.
  - `vision_core/`: captures, filters and streams the camera feed over
    RTSP.
- `repos/`: git submodules (U-Boot, Linux kernel, Buildroot, GHRD,
  `sopc2dts`, Altera FPGA top-level files).
- `sdcard/`: SD card boot partition contents and root filesystem
  overlay, used to assemble the flashable image.
- `docs/`: reference manuals and this project's own documentation.
  - `Altera/`: Intel/Altera manuals (Avalon interface specs, Embedded
    Peripherals IP User Guide, Cyclone V HPS Technical Reference
    Manual).
  - `FPGAlix/`: this project's own numbered documentation (this file
    included).
  - `Misc/`: miscellaneous reference material (bootloader generation
    flow).
  - `OV7670_CameraModule/`: OV7670 datasheet, implementation guide, and
    camera module schematic.
  - `Terasic_DE1-SoC/`: DE1-SoC schematic, GHRD archive, and Terasic's
    own user manuals.
- `WARNING.md`: camera hardware bug note (symlink to
  `hw/camera_adapter/WARNING.md`).
- `LICENSE`: MIT.

## License

MIT, see [LICENSE](../../LICENSE).
