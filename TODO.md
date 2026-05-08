# TODO

## Documentation

- [x] Create a storage where anyone can download installers — files are on SharePoint (UniTS account required, cannot be made public):
  - Quartus 22.1 installer
  - ARM toolchain (`arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-linux-gnueabihf`)
  - Ubuntu 22.04 LTS image

  Link: https://units-my.sharepoint.com/:f:/g/personal/s133932_ds_units_it/IgCUoXfERelfT7MK33S5s3mCAS_ia6fI6KJ4DiAp4tTX6Gw?e=oKP6TL

- [ ] If feasible, provide a pre-configured VM (Ubuntu 22.04 + Quartus + toolchain already installed and ready to use)
- [x] Document the full list of `apt` packages required on Ubuntu 22.04 to build the project (kernel, U-Boot, Buildroot, mkimage, etc.)

  **32-bit libraries for Quartus 22.1** (Quartus is a 32-bit application):
  - `libglib2.0-0:i386`
  - `libfreetype6:i386`
  - `libsm6:i386`
  - `libxrender1:i386`
  - `libfontconfig1:i386`
  - `libxext6:i386`
  - `libpng16-16:i386`
  - `libx11-6:i386`
  - `libxau6:i386`
  - `libxdmcp6:i386`
  - `libxft2:i386`
  - `libxss1:i386`
  - `libc6:i386`
  - `libncurses6:i386`
  - `libstdc++6:i386`
  - `libz1:i386`
  - `lib32z1`
  - `lib32stdc++6`
  - `libncurses5` / `libncurses5:i386`
  - `libtinfo5` / `libtinfo5:i386`
  - `libxtst6:i386`
  - `libxi6:i386`
  - `libdbus-1-3:i386`
  - `libgtk2.0-0:i386`
  - `default-jdk` (required by Quartus GUI)
  - `build-essential`, `g++`, `make`, `wget`, `unzip`
  - `libfreetype6` (64-bit)

  ```bash
  sudo apt install -y \
    libglib2.0-0:i386 libfreetype6:i386 libsm6:i386 libxrender1:i386 \
    libfontconfig1:i386 libxext6:i386 libpng16-16:i386 libx11-6:i386 \
    libxau6:i386 libxdmcp6:i386 libxft2:i386 libxss1:i386 libc6:i386 \
    libncurses6:i386 libstdc++6:i386 libz1:i386 lib32z1 lib32stdc++6 \
    libncurses5 libncurses5:i386 libtinfo5 libtinfo5:i386 \
    libxtst6:i386 libxi6:i386 libdbus-1-3:i386 libgtk2.0-0:i386 \
    default-jdk build-essential g++ make wget unzip libfreetype6
  ```

  **U-Boot / kernel build dependencies:**
  - `bison` — parser generator required by the kernel build system
  - `flex` — lexer generator required by the kernel build system
  - `libssl-dev` — required to build the kernel (crypto subsystem headers)
  - `libncurses-dev` — required for `make menuconfig`
  - `u-boot-tools` — provides `mkimage` to compile the U-Boot boot script

  ```bash
  sudo apt install bison flex libssl-dev libncurses-dev u-boot-tools
  ```

  **Version control:**
  - `git`
  - `git-lfs`

  ```bash
  sudo apt install git git-lfs
  ```

  **Networking / misc tools:**
  - `curl`
  - `openssh-server`
  - `iperf3`

  ```bash
  sudo apt install curl openssh-server iperf3
  ```
- [ ] Document the following environment variables to add to `~/.bashrc` (required for Quartus and the ARM toolchain):

  ```bash
  export QUARTUS_ROOTDIR="/home/wheel/Programs/Altera/22.1/quartus/"
  export QSYS_ROOTDIR="${QUARTUS_ROOTDIR}/sopc_builder/bin"
  export PATH="$PATH:${QUARTUS_ROOTDIR}/bin"
  export PATH="$PATH:${QSYS_ROOTDIR}"
  export PATH="$PATH:/home/wheel/Programs/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-linux-gnueabihf/bin"
  ```

- [x] Configure USB Blaster on Ubuntu 22.04

  The USB Blaster is not accessible without a udev rule — Quartus will show "No JTAG hardware" without this step.

  Create the udev rule:

  ```bash
  sudo nano /etc/udev/rules.d/51-usbblaster.rules
  ```

  Paste the following:

  ```
  SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6001", MODE="0666", GROUP="plugdev"
  SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6002", MODE="0666", GROUP="plugdev"
  SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6003", MODE="0666", GROUP="plugdev"
  ```

  Reload udev and add the user to the `plugdev` group:

  ```bash
  sudo udevadm control --reload-rules
  sudo udevadm trigger
  sudo usermod -aG plugdev $USER
  ```

  Log out and back in for the group change to take effect, then replug the USB Blaster. Verify with:

  ```bash
  jtagconfig
  ```

  Should print something like `1) USB-Blaster [...]`.

- [ ] Review and finalize `README.md`

## Kernel

- [x] Enable LED Heartbeat Trigger in `make menuconfig`:
  `Device Drivers → LED Support → LED Trigger support → LED Heartbeat Trigger`

## System configuration

- [ ] Write and document a `chrony` configuration file

## Testing

- [ ] Verify Ethernet PHY stability: MAC/PHY parameters (clock delays, PHY address, `phy-mode`) in `socfpga_cyclone5_fpgalix.dts` may need tuning — known potential issue, see README footnote
- [ ] Verify that Ethernet and networking work correctly on the board:
  - DHCP lease obtained (`ip addr`)
  - Internet/LAN reachability (`ping`)
  - SSH access from host (`openssh`)
  - Bandwidth (`iperf3`)
