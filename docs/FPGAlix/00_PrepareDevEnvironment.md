# 00. Prepare Your Dev Environment

## Introduction

The board boots a full Linux OS and is reachable over Ethernet, so no
JTAG-based source debugger is needed on the host to debug software:
`gdbserver`, installed on the board, is attached to remotely from the
host with `gdb`, over the network. No in-circuit debugging is involved.

JTAG is still needed for one thing this can't replace: debugging the FPGA
fabric itself, with SignalTap's in-system logic analyzer, from Quartus.
For that, a physical PC is necessary, not a VM. Forwarding the
USB-Blaster from host to guest through the hypervisor's own USB
redirection (VirtualBox/VMware "USB passthrough", Hyper-V's basic USB
support) is a long-standing, widely reported source of failures on
timing-sensitive JTAG traffic: corrupted transfers, chains that fail to
enumerate. Everything else in this chapter, writing VHDL, compiling the
Quartus project, building U-Boot, the kernel, and Buildroot, works fine
in a VM; only the SignalTap session needs the USB-Blaster to be truly
local.

This chapter covers the required `apt` packages, where each piece of
software lives on disk, the environment variables needed to reach them,
USB-Blaster and serial console access, and the editor used to write VHDL
and application code.

## 1. Host requirements

FPGAlix requires an x86_64 machine running Ubuntu 22.04 LTS, with Quartus
Prime (including Platform Designer) and an ARM GNU toolchain installed and
working.

## 2. Required software

### 2.1 apt packages

```bash
sudo apt install -y \
    build-essential bison flex libssl-dev libncurses-dev u-boot-tools \
    cpio unzip rsync bc patch \
    git git-lfs \
    curl wget openssh-server iperf3 picocom \
    pkg-config libopencv-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstrtspserver-1.0-dev \
    gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad \
    python3 python3-numpy python3-opencv \
    ffmpeg \
    default-jdk
```

- `build-essential`, `bison`, `flex`, `libssl-dev`, `libncurses-dev`,
  `u-boot-tools`: building `repos/u-boot-socfpga` and the Linux kernel,
  and compiling `sw/boot.script` with `mkimage`.
- `cpio`, `unzip`, `rsync`, `bc`, `patch`: mandatory host packages for
  Buildroot itself, not covered by the group above; cross-checked against
  `repos/buildroot/docs/manual/prerequisite.adoc` and
  `repos/buildroot/support/dependencies/dependencies.sh` rather than
  assumed, since most of Buildroot's other mandatory tools (`tar`, `sed`,
  `find`, `perl`, `gzip`, ...) already ship with a stock Ubuntu install.
- `git`, `git-lfs`: cloning this repository and its submodules. No file in
  the repository is currently tracked with LFS, but the tooling is
  installed for forward compatibility.
- `curl`, `wget`: fetching installers and, on the board itself, general
  network testing.
- `openssh-server`, `iperf3`: verifying SSH access and bandwidth to the
  board once it is on the network.
- `picocom`: terminal emulator for the board's serial console.
- `pkg-config`, `libopencv-dev`, `libgstreamer1.0-dev`,
  `libgstreamer-plugins-base1.0-dev`, `libgstrtspserver-1.0-dev`: needed
  to build `sw/vision_core` natively on the host (`make intel`), which
  lets its capture/filter/streaming pipeline be exercised without the
  board, before cross-compiling it (`make arm`); see
  [sw/vision_core/README.md](../../sw/vision_core/README.md).
- `gstreamer1.0-plugins-base`, `gstreamer1.0-plugins-good`,
  `gstreamer1.0-plugins-bad`: the dev packages above only provide headers
  for linking; actually running a natively-built `vision_core` needs the
  matching runtime plugins loaded at pipeline-construction time:
  `appsrc`/`videoconvert` (base), `rtpjpegpay`/`rtpvrawpay`/`rtph264pay`
  via `rtpmanager` (good), `openh264enc` (bad), matching the sub-plugin
  table in `sw/vision_core/README.md`.
- `python3`, `python3-numpy`, `python3-opencv`: required by
  `sw/camera_driver_2/utils/debug/view.py`, the raw-frame viewer used to
  debug the V4L2 driver.
- `default-jdk`: `repos/sopc2dts` is a plain Java tool, built with
  `javac`/`jar` via its own `Makefile`
  ([02. Board Bring-up, Section 6.3.1](02_BoardBringUp.md#631-using-sopc2dts-as-a-reference)),
  not with Quartus's bundled JRE
  ([Section 2.2](#22-quartus-needs-no-extra-libraries)), which is
  runtime-only and has no `javac`.
- `ffmpeg`: provides `ffplay`, used on the host to view `vision_core`'s
  RTSP output with minimal latency:
  ```bash
  ffplay -fflags nobuffer -flags low_delay -framedrop rtsp://<board-ip>:8554/stream
  ```
  VLC was tried first and dropped; `ffplay` with these flags is what
  actually got used going forward.

### 2.2 Quartus needs no extra libraries

Earlier notes for this project listed a long block of 32-bit (`i386`)
compatibility libraries as required by Quartus. That does not hold for
the Quartus Prime 22.1 install actually in use: every executable under
`quartus/linux64/` (`quartus`, `quartus_sh`, `quartus_pgm`, `quartus_cpf`)
and Platform Designer's own launcher
(`quartus/sopc_builder/bin/qsys-edit`) is a 64-bit `x86-64` ELF binary,
and `ldd` resolves every one of their non-Quartus-internal shared
libraries to the system's own 64-bit copies under
`/lib/x86_64-linux-gnu/`, never an `i386` one. Quartus also ships its own
private JRE at `quartus/linux64/jre64/`, so no `default-jdk` or other
system Java package is needed either.

The only 32-bit binaries anywhere under the Quartus install are inside
`questa_fse`, the bundled ModelSim/Questa simulator: its UVM DPI
libraries and its own private GCC 7.4 toolchain. This project does not
use Questa, so that corner does not matter here either.

### 2.3 Install the software, and where it lives

| Software | Version | Install path |
|---|---|---|
| Quartus Prime | 22.1 | `~/Programs/Altera/22.1/` |
| ARM GNU toolchain | `arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-linux-gnueabihf` | `~/Programs/Arm/toolchain/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-linux-gnueabihf/` |

The user's home folder is sufficient for both; a system-wide installation
is not required. See [Section 3](#3-getting-the-installers) for where to
download each of these.

### 2.4 Environment variables

Add to `~/.bashrc`, matching the install paths from
[Section 2.3](#23-install-the-software-and-where-it-lives):

```bash
export QUARTUS_ROOTDIR="$HOME/Programs/Altera/22.1/quartus/"
export QSYS_ROOTDIR="${QUARTUS_ROOTDIR}/sopc_builder/bin"
export PATH="${QUARTUS_ROOTDIR}/bin:$PATH"
export PATH="$HOME/Programs/Altera/22.1/quartus/sopc_builder/bin/:$PATH"
export PATH="$HOME/Programs/Arm/toolchain/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-linux-gnueabihf/bin:$PATH"
export PATH="${QSYS_ROOTDIR}:$PATH"
```

Reload with `source ~/.bashrc` (or open a new shell) after editing.

Each line **prepends** to `PATH` rather than appending, so these installs
are always found first, ahead of anything already on `PATH`, whether or
not a same-named tool exists elsewhere on the system.

**Not** on `PATH`, and not meant to be: two other ARM toolchains exist in
this project and are always invoked by their full path from a `Makefile`,
never exported globally.

- Buildroot builds its own internal toolchain
  (`arm-buildroot-linux-gnueabihf-...`) as part of
  `repos/buildroot/output/host/bin/`. It is what `sw/vision_core`'s
  `make arm` target uses, and it must stay separate from the toolchain
  above: mixing the two (e.g. linking an object built with one against a
  library built with the other) produces glibc ABI mismatches.
- The host's own native `gcc`/`g++` is used for `sw/vision_core`'s
  `make intel` target ([Section 2.1](#21-apt-packages)), which is a
  completely separate, x86_64 build of the same program.

### 2.5 USB-Blaster and serial console access

Beyond the packages installed in [Section 2.1](#21-apt-packages), the
board requires access to two USB devices: the USB-Blaster, used for JTAG
access (SignalTap), and the UART-to-USB serial console. By default, udev
restricts both device nodes to root.

Create a udev rule granting the invoking user's group access to the
USB-Blaster:

```bash
sudo tee /etc/udev/rules.d/51-usbblaster.rules << 'EOF'
SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6001", MODE="0666", GROUP="plugdev"
SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6002", MODE="0666", GROUP="plugdev"
SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6003", MODE="0666", GROUP="plugdev"
EOF
sudo udevadm control --reload-rules
sudo udevadm trigger
sudo usermod -aG plugdev "$USER"
```

Without this rule, Quartus reports "No JTAG hardware" even with the
USB-Blaster plugged in. Verify it worked with:

```bash
jtagconfig
```

which should print something like `1) USB-Blaster [...]`.

The serial console needs no equivalent custom rule: the board's
UART-to-USB connector enumerates as `/dev/ttyUSB0`, and Ubuntu's own
default rule already assigns every `ttyUSB*`/`ttyACM*` device to the
`dialout` group. The user still needs to be added to it:

```bash
sudo usermod -aG dialout "$USER"
```

Log out and back in (or `newgrp plugdev`/`newgrp dialout`) for both group
changes to take effect.

### 2.6 Editor

Both the VHDL sources under `hw/` and the C/C++/Python sources under `sw/`
are written in Visual Studio Code, with the following extensions:

| Extension | Purpose |
|---|---|
| `teros-technology.teroshdl` (TerosHDL) | VHDL linting, snippets, project awareness |
| `rjyoung.vscode-modern-vhdl-support` | VHDL syntax highlighting and language support |
| `ms-vscode.cpptools` (+ `ms-vscode.cpptools-extension-pack`) | C/C++ IntelliSense for `camera_driver_2`, `hexip_daemon`, and `vision_core` |
| `ms-vscode.makefile-tools` | Makefile awareness (every `sw/` project builds via `make`) |
| `ms-python.python` (+ Pylance, debugpy) | editing/debugging `sw/camera_driver_2/utils/debug/view.py` |
| `tomoki1207.pdf` | viewing the datasheet/manual PDFs under `docs/` without leaving the editor |

VS Code is suggested, not required: it is only a text editor here, no
build or debug integration is used from it, so any editor works equally
well.

## 3. Getting the installers

See [download.md](download.md) for where to get Quartus Prime, the ARM
GNU toolchain, and the Ubuntu 22.04 LTS image.
