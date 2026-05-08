# FPGAlix

> **Work in progress:** This documentation is still being defined and developed. Content may be incomplete or subject to change.

## Hardware build workflow

Before proceeding with any other step, the FPGA hardware design must be compiled:

1. Open the Quartus project: `hw/quartus/FPGAlix.qpf`
2. Open Platform Designer from Quartus (**Tools → Platform Designer**) and load `hw/quartus/soc_system.qsys`
3. Generate the HDL: **Generate → Generate HDL...**, then confirm
4. Close Platform Designer and compile the full design in Quartus: **Processing → Start Compilation**

The compilation produces `hw/quartus/output_files/FPGAlix.sof`, which is the starting point for all subsequent steps.

## Development environment

The setup used for this project:

| Component | Version / Details |
|-----------|------------------|
| OS | Ubuntu 22.04 LTS |
| Quartus | 22.1 |
| ARM toolchain | `arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-linux-gnueabihf` |

Both Quartus and the ARM toolchain are installed in `~/Programs`.

## Post-build step: convert SOF to RBF

After modifying and completing the hardware design workflow in Quartus + QSys (Platform Designer), run the following command from the root of this repository to convert the compiled FPGA bitstream into the raw binary format required for SD card boot:

```bash
quartus_cpf -c hw/quartus/output_files/FPGAlix.sof sdcard/boot_partition/soc_system.rbf
```

The `.sof` (SRAM Object File) is the compiled output produced by Quartus and contains the full FPGA configuration data. The `.rbf` (Raw Binary File) is a stripped-down binary version of that configuration, used to program the FPGA fabric (the programmable logic) at runtime. During boot, U-Boot uses the HPS FPGA Manager to load `soc_system.rbf` from the SD card boot partition and configure the FPGA logic accordingly.

## Post-build step: generate U-Boot BSP headers from HPS handoff

After the Quartus + QSys compilation, the HPS configuration is exported to a set of handoff files in `hw/quartus/hps_isw_handoff/soc_system_hps_0/`. These must be converted into C header files that U-Boot uses to initialize the HPS hardware on the DE1-SoC board.

Run the following command from the root of this repository:

```bash
python3 repos/u-boot-socfpga/arch/arm/mach-socfpga/cv_bsp_generator/cv_bsp_generator.py \
    -i hw/quartus/hps_isw_handoff/soc_system_hps_0/ \
    -o repos/u-boot-socfpga/board/terasic/de1-soc/qts/
```

The script reads the XML handoff data exported by Quartus and generates the following board-specific headers under `repos/u-boot-socfpga/board/terasic/de1-soc/qts/`:

| File | Purpose |
|------|---------|
| `pinmux_config.h` | HPS pin multiplexing (which peripheral is assigned to each I/O pad) |
| `pll_config.h` | PLL and clock settings for the HPS |
| `sdram_config.h` | DDR SDRAM controller initialization parameters |
| `iocsr_config.h` | I/O Configuration Shift Register values for the HPS I/O banks |

These files must be regenerated every time the HPS subsystem is modified in QSys (Platform Designer), so that U-Boot initializes the hardware with settings that exactly match the compiled design.

### Compiling U-Boot

Move into the U-Boot source directory and set the required environment variables:

```bash
cd repos/u-boot-socfpga
export ARCH=arm
export CROSS_COMPILE=arm-none-linux-gnueabihf-
```

Load the Cyclone V base configuration, then open the interactive menu to select the correct board target:

```bash
make socfpga_cyclone5_defconfig
make menuconfig
```

In the menuconfig interface navigate to:
**ARM architecture → Altera SOCFPGA board select → Terasic DE1-SoC (Cyclone V)**

When exiting menuconfig, confirm **Yes** when prompted to save — otherwise the board selection is lost and the build will use the wrong target.

Then build:

```bash
make -j$(nproc)
```

The build produces `repos/u-boot-socfpga/u-boot-with-spl.sfp`, a combined image containing the SPL (Secondary Program Loader) and U-Boot proper in the Altera SoCFPGA format, ready to be written to the SD card.

The file is also accessible via the symlink `sdcard/u-boot-with-spl.sfp`, which points to the build output so that all SD card artifacts are reachable from a single directory.

## Compiling the boot script

The file `sw/boot.script` is a human-readable U-Boot script that defines the boot sequence (loading the kernel, DTB, and RBF from the SD card). It must be compiled into a binary image that U-Boot can execute.

Move into the `sw/` directory and run `mkimage`:

```bash
cd sw/
mkimage -A arm -O linux -T script -C none -a 0 -e 0 -n "Boot" -d boot.script ../sdcard/boot_partition/u-boot.scr
```

The output `sdcard/boot_partition/u-boot.scr` is the compiled boot script, ready to be read by U-Boot at startup. This step must be repeated every time `sw/boot.script` is modified.

## Building the kernel

Move into the Linux source directory and set the required environment variables:

```bash
cd repos/linux-socfpga
export ARCH=arm
export CROSS_COMPILE=arm-none-linux-gnueabihf-
```

Load the SoCFPGA default configuration:

```bash
make socfpga_defconfig
```

Open the interactive configuration menu and enable the mSGDMA driver as a built-in:

```bash
make menuconfig
```

Navigate to **Device Drivers → DMA Engine support** and enable:

- **Altera / Intel mSGDMA Engine** → `*` (built-in)

Navigate to **Device Drivers → Input device support → Keyboards** and enable:

- **GPIO Buttons** → `*` (built-in)

Navigate to **Device Drivers → LED Support** and enable:

- **LED Class Support** → `*` (built-in)
- **LED Trigger support → LED Heartbeat Trigger** → `*` (built-in)

When exiting menuconfig, confirm **Yes** when prompted to save.

Build the compressed kernel image and prepare the tree for external kernel modules:

```bash
make -j$(nproc) zImage
make modules_prepare
```

### Device Tree Source

Before building the Device Tree blobs, a board-specific DTS file must exist at:

```
repos/linux-socfpga/arch/arm/boot/dts/socfpga_cyclone5_fpgalix.dts
```

It describes the hardware topology of the SoC and is maintained manually in `socfpga_cyclone5_fpgalix.dts`, which includes `socfpga_cyclone5.dtsi` (the standard SoCFPGA base provided by the kernel) and adds the board-specific nodes on top.

#### Using sopc2dts as a reference

`sopc2dts` cannot be used as a direct DTS source for the kernel — its output is a standalone file that does not include `socfpga_cyclone5.dtsi` and is therefore incompatible with the modern Linux SoCFPGA DTS infrastructure.

It is however useful as a **reference** when the HPS subsystem changes in Platform Designer: run it, inspect its output to extract updated addresses, interrupt numbers and compatible strings for the FPGA peripherals, then manually apply those changes to `socfpga_cyclone5_fpgalix.dts`.

Before first use, build it from source:

```bash
cd repos/sopc2dts
make all
```

Generate the reference output:

```bash
java -jar sopc2dts.jar --input ../../hw/quartus/soc_system.sopcinfo --output /tmp/socfpga_ref.dts
```

> **Note:** This project's Linux kernel fork (`linux-socfpga`) already includes a ready-made `socfpga_cyclone5_fpgalix.dts` matching the current hardware. No action is needed unless the HPS subsystem has been modified in QSys.

Build the Device Tree blobs[^1]:

```bash
make dtbs
```

The compiled DTB is accessible via the symlink `sdcard/boot_partition/socfpga.dtb`, which points to `repos/linux-socfpga/arch/arm/boot/dts/socfpga_cyclone5_fpgalix.dtb`.

## Compiling the Buildroot operating system

Move into the Buildroot directory and set the required environment variables:

```bash
cd repos/buildroot
export ARCH=arm
export CROSS_COMPILE=arm-none-linux-gnueabihf-
```

Open the configuration menu:

```bash
make menuconfig
```

Enable the following options, navigating the menu as indicated:

**Target options**
- Target Architecture → `ARM (little endian)`
- Target Architecture Variant → `cortex-A9`
- Enable NEON SIMD extension support → `Y`
- Enable VFP extension support → `Y`

**Toolchain**
- C library → `glibc`
- Enable C++ support → `Y`
- Build cross gdb for the host → `Y`

**System configuration**
- `/dev` management → `Dynamic using devtmpfs + eudev`
- Enable root login with password → `Y`
- Root password → *(set a password)*

**Target packages → Debugging, profiling and benchmark**
- `gdb` → `Y`, then enable `gdbserver`
- `strace` → `Y`

**Target packages → Hardware handling**
- Show packages that are also provided by busybox → `Y`
- `i2c-tools` → `Y`

**Target packages → Networking applications**
- `chrony` → `Y` *(NTP client)*
- `dhcpcd` → `Y` *(DHCP client)*
- `ethtool` → `Y` *(Ethernet diagnostics)*
- `ifupdown-scripts` → `Y` *(ifup/ifdown support)*
- `iperf3` → `Y` *(bandwidth testing)*
- `iproute2` → `Y` *(ip addr, ip route)*
- `openssh` → `Y` *(SSH server and client)*
- `sshfs` → `Y` *(mount remote filesystems over SSH)*
- `tcpdump` → `Y` *(network packet capture)*

**Target packages → System tools**
- `htop` → `Y`
- `kmod` → `Y`
- `util-linux` → `Y`
- `xz` → `Y`
- `zip` → `Y`

**Target packages → Text editors and viewers**
- `nano` → `Y`

When exiting menuconfig, confirm **Yes** when prompted to save.

Once configured, build the entire system:

```bash
make
```

Do not use `make -j$(nproc)` — Buildroot manages parallelism internally. The default setting `BR2_JLEVEL=0` already instructs Buildroot to use all available CPU cores during the entire build (toolchain, packages, and root filesystem). The root filesystem image will be generated at `repos/buildroot/output/images/rootfs.tar` and is accessible via the symlink `sdcard/rootfs.tar`, alongside all other SD card artifacts.

## Creating the SD card

> **WARNING:** This section involves low-level disk operations. A wrong device name or a mistyped command can silently overwrite a system disk, a data partition, or any other block device on your machine. Always double-check every device path (`/dev/sdX`, `/dev/loopN`) before pressing Enter. When in doubt, stop and verify.

All the files needed to boot the board are collected under the `sdcard/` directory. The recommended approach is to create a disk image file first, then write it to the physical SD card — but the same procedure can be applied **directly on the SD card** by replacing `sdcard/images/sdcard.img` with the SD card device (e.g. `/dev/sdX`), skipping the image creation step and the final `dd`/Etcher write.

To write the final image to the SD card you can use:
- **Linux:** `dd` (see last step below)
- **Windows/Mac:** [balenaEtcher](https://etcher.balena.io/) — free, open-source, just select the image and the target drive

### 1. Create the image file

From the root of the repository, move into the `sdcard/` directory:

```bash
cd sdcard/
mkdir -p images
```

Create an empty image sized to fit any "8 GB" SD card. SD card manufacturers use decimal units (1 GB = 1,000,000,000 bytes), so an "8 GB" card has ~7629 MiB of actual space. Using 7400 MiB leaves a safe margin:

```bash
dd if=/dev/zero of=images/sdcard.img bs=1M count=7400
```

### 2. Partition the image

```bash
fdisk images/sdcard.img
```

Inside `fdisk`, enter the following commands in order:

```
o              # new MBR partition table

n              # partition 1 — FAT32 boot (500 MiB)
p
1
(enter)
+500M
t              # set type to W95 FAT32 LBA
c

p              # print disk info — note "Sectors" and "Sector size"
```

Before creating partition 3, calculate its first sector:

```
first_sector_p3 = total_sectors - ceil(10 * 1024 * 1024 / sector_size)
```

Example with 512-byte sectors and a 7400 MiB image (total sectors = 15,155,200):

```
first_sector_p3 = 15,155,200 - ceil(10 * 1024 * 1024 / 512)
               = 15,155,200 - 20,480
               = 15,134,720
```

Now create partition 3 at that position:

```
n              # partition 3 — raw A2, U-Boot SPL (10 MiB)
p
3
15134720       # enter the calculated first sector
(enter)        # accept default last sector (end of disk) — fdisk should confirm ~10 MiB
t              # set type to A2 (Altera SoCFPGA bootloader)
3
a2

n              # partition 2 — ext4 rootfs (fills remaining space)
p
2
(enter)        # fdisk auto-selects first sector after p1
(enter)        # fdisk auto-selects last sector before p3

p              # verify the partition table before writing
w              # write and exit
```

For this example (512-byte sectors, 7400 MiB image), the expected output of `p` is:

```
Disk images/sdcard.img: 7,23 GiB, 7759462400 bytes, 15155200 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x2df93d98

Device             Boot    Start      End  Sectors  Size Id Type
images/sdcard.img1          2048  1026047  1024000  500M  c W95 FAT32 (LBA)
images/sdcard.img2       1026048 15134719 14108672  6,7G 83 Linux
images/sdcard.img3      15134720 15155199    20480   10M a2 unknown
```

### 3. Attach the image to a loop device

```bash
sudo losetup -fP images/sdcard.img
```

Find out which loop device was assigned:

```bash
losetup -l
```

Look for the entry with `sdcard.img` and note the device name (e.g. `/dev/loop2`). In all the commands below, replace `N` with the actual number.

### 4. Format the partitions

```bash
sudo mkfs.vfat -n BOOT /dev/loopNp1
sudo mkfs.ext4 -L rootfs /dev/loopNp2
# loopNp3 is left unformatted — U-Boot SPL is written raw
```

### 5. Mount and copy the files

```bash
sudo mkdir -p /tmp/sdcard/boot /tmp/sdcard/rootfs
sudo mount /dev/loopNp1 /tmp/sdcard/boot
sudo mount /dev/loopNp2 /tmp/sdcard/rootfs
```

Copy the boot partition files:

```bash
sudo cp -L boot_partition/* /tmp/sdcard/boot/
```

Extract the root filesystem:

```bash
sudo tar xvf rootfs.tar -C /tmp/sdcard/rootfs/
```

Write U-Boot (SPL + U-Boot proper) raw into the A2 partition:

```bash
sudo dd if=u-boot-with-spl.sfp of=/dev/loopNp3 bs=64k seek=0
```

### 6. Unmount, detach and clean up

```bash
sudo umount /tmp/sdcard/boot /tmp/sdcard/rootfs
sudo losetup -d /dev/loopN
sudo rm -rf /tmp/sdcard
```

### 7. Write the image to the SD card (Linux)

```bash
sudo dd if=images/sdcard.img of=/dev/sdX bs=4M status=progress
sync
```

Replace `/dev/sdX` with the actual SD card device. **Double-check the target device** before running — this command will overwrite it completely.

## Booting the board

### MSEL configuration (SW10)

Before inserting the SD card and powering on the board, configure the **SW10** DIP switch located on the underside of the DE1-SoC. This switch sets the MSEL[4:0] pins, which tell the FPGA how its configuration will be loaded.

> **Note:** SW10 uses **negative logic** — switch in the ON position = logic 0, switch in the OFF position = logic 1.

Set the switches as follows:

| Switch | Signal | Position | Logic |
|--------|--------|----------|-------|
| SW10.1 | MSEL0  | ON       | 0     |
| SW10.2 | MSEL1  | ON       | 0     |
| SW10.3 | MSEL2  | ON       | 0     |
| SW10.4 | MSEL3  | ON       | 0     |
| SW10.5 | MSEL4  | ON       | 0     |
| SW10.6 | —      | OFF      | 1     |

With MSEL[4:0] = `00000`, the FPGA is configured by the HPS via the FPGA Manager: during boot, U-Boot loads `soc_system.rbf` from the SD card boot partition and programs the FPGA fabric before handing off to the Linux kernel.

### Inserting the SD card and connecting the serial console

1. Insert the SD card into the slot on the underside of the board.
2. Connect a mini USB cable between the **UART-to-USB** connector on the board (labelled on the PCB) and your PC.

The board exposes a serial console at **115200 baud, 8N1, no flow control**. Use a terminal emulator to connect:

- **Windows:** [TeraTerm](https://teratermproject.github.io/) is a good option — select the COM port that appears in Device Manager under *Ports (COM & LPT)* when you plug in the cable.
- **Linux:** `picocom` is a lightweight choice — install with `sudo apt install picocom`, then find the device with:

  ```bash
  dmesg | grep tty
  ```

  The board typically appears as `/dev/ttyUSB0`. Connect with:

  ```bash
  picocom -b 115200 /dev/ttyUSB0
  ```

  > **Note:** If access is denied, either run with `sudo` or adjust the permissions on `/dev/ttyUSB0`.

3. Power on the board — the boot log should appear in the terminal immediately.

---

[^1]: In case of Ethernet connection instability at runtime, verify the MAC/PHY parameters (clock delays, PHY address, `phy-mode`) in `socfpga_cyclone5_fpgalix.dts` and rebuild the DTB.

