# Misc — Board commands (HPS Linux)

Quick reference for interacting with the DE1-SoC board peripherals from the Linux command line.

Memory map (LW HPS-to-FPGA bridge @ `0xFF200000`):

| Peripheral | Address      |
|------------|--------------|
| LED PIO    | `0xFF200120` |
| Button PIO | `0xFF200110` |
| DIP-SW PIO | `0xFF200100` |

---

## 1. LEDs

The kernel `gpio-leds` driver exposes each LED under `/sys/class/leds/fpga:red:N/`.

```bash
# Turn LED 0 on
echo 1 > /sys/class/leds/fpga:red:0/brightness

# Turn LED 0 off
echo 0 > /sys/class/leds/fpga:red:0/brightness

# Turn all LEDs on
for i in $(seq 0 9); do echo 1 > /sys/class/leds/fpga:red:$i/brightness; done

# Turn all LEDs off
for i in $(seq 0 9); do echo 0 > /sys/class/leds/fpga:red:$i/brightness; done
```

> Note: LED 9 has `linux,default-trigger = "heartbeat"` in the DTS — it blinks automatically
> unless you disable the trigger first:
> ```bash
> echo none > /sys/class/leds/fpga:red:9/trigger
> echo 1    > /sys/class/leds/fpga:red:9/brightness
> ```

---

## 2. Switches (SW[9:0])

The kernel `gpio-keys` driver exposes buttons as input events. For direct register reading
use `devmem2` (install with `opkg` or build from source if not present):

```bash
# Read all 10 DIP switches at once (returns a bitmask, bit N = SW[N])
devmem2 0xFF200100 w
```

Alternatively, export individual GPIO lines via sysfs (find the correct GPIO base first):

```bash
# Find the gpio chip associated with dipsw_pio
grep -r dipsw /sys/bus/platform/drivers/altera_gpio/*/gpiochip*/label 2>/dev/null

# Then export and read (replace <BASE> with the chip base number)
echo <BASE> > /sys/class/gpio/export
cat /sys/class/gpio/gpio<BASE>/value
```

---

## 3. I2C

The HPS exposes two I2C buses (configured at 100 kHz in the DTS):

| Bus  | Device      |
|------|-------------|
| I2C0 | `/dev/i2c-0`|
| I2C1 | `/dev/i2c-1`|

```bash
# Scan a bus for connected devices
i2cdetect -y -r 0   # bus 0
i2cdetect -y -r 1   # bus 1

# Read a byte from register REG of device at address ADDR
i2cget -y 0 0xADDR 0xREG

# Write a byte to register REG of device at address ADDR
i2cset -y 0 0xADDR 0xREG 0xVALUE

# Dump all registers (256 bytes) of a device
i2cdump -y 0 0xADDR
```

---

## 4. Program the FPGA at runtime (FPGA Manager)

### Generate the .rbf bitstream from Quartus (on the host PC)

Run from the root of the repository:

```bash
quartus_cpf -c hw/quartus/output_files/FPGAlix.sof /tmp/fpga_bitstream.rbf
```

Then transfer it to the board (lands in root's home directory):

```bash
scp /tmp/fpga_bitstream.rbf root@<ip_board>:~
```

### Program the FPGA fabric (on the board)

*See also: bridge unbind/rebind note at the end of this section.*

Once the file is on the board:

```bash
# Backup the current bitstream
mv /lib/firmware/soc_system.rbf /lib/firmware/soc_system.rbf.old

# Move and rename the new bitstream to the expected name
mv ~/fpga_bitstream.rbf /lib/firmware/soc_system.rbf

# Program the FPGA fabric
echo 0 > /sys/class/fpga_manager/fpga0/flags
echo soc_system.rbf > /sys/class/fpga_manager/fpga0/firmware

# Verify programming succeeded — should print: operating
cat /sys/class/fpga_manager/fpga0/state
```

> **Note:** `/lib/firmware/soc_system.rbf` is on the rootfs (ext4) and persists across reboots.
> However, Linux does **not** reprogram the FPGA automatically at boot — that is done exclusively
> by U-Boot, which loads `soc_system.rbf` from the FAT32 boot partition (p1). The file in
> `/lib/firmware/` is only used when you manually trigger the FPGA Manager.

```bash
```

### HPS-to-FPGA bridge unbind/rebind

If Linux has drivers attached to the HPS-to-FPGA bridges, unbind them before reprogramming and rebind after, otherwise they may crash.

First, discover the bridge names from the live device tree:

```bash
ls /sys/bus/platform/drivers/altera-hps2fpga-bridge/
ls /sys/bus/platform/drivers/altera-fpga2hps-bridge/
```

Each entry listed (e.g. `ff400000.fpga_bridge`) is a bridge name to use in the commands below.
Then unbind all of them before reprogramming and rebind after:

```bash
# Unbind (repeat for each name found above)
echo <name> > /sys/bus/platform/drivers/altera-hps2fpga-bridge/unbind
echo <name> > /sys/bus/platform/drivers/altera-fpga2hps-bridge/unbind

# ... reprogram here ...

# Rebind
echo <name> > /sys/bus/platform/drivers/altera-hps2fpga-bridge/bind
echo <name> > /sys/bus/platform/drivers/altera-fpga2hps-bridge/bind
```

Alternatively, store the names in variables for easy copy-paste:

```bash
H2F_LW=$(ls /sys/bus/platform/drivers/altera-hps2fpga-bridge/ | grep fpga_bridge | head -1)
H2F=$(ls /sys/bus/platform/drivers/altera-hps2fpga-bridge/  | grep fpga_bridge | tail -1)
F2H=$(ls /sys/bus/platform/drivers/altera-fpga2hps-bridge/  | grep fpga_bridge | head -1)

# Unbind
echo $H2F_LW > /sys/bus/platform/drivers/altera-hps2fpga-bridge/unbind
echo $H2F    > /sys/bus/platform/drivers/altera-hps2fpga-bridge/unbind
echo $F2H    > /sys/bus/platform/drivers/altera-fpga2hps-bridge/unbind

# ... reprogram here ...

# Rebind
echo $H2F_LW > /sys/bus/platform/drivers/altera-hps2fpga-bridge/bind
echo $H2F    > /sys/bus/platform/drivers/altera-hps2fpga-bridge/bind
echo $F2H    > /sys/bus/platform/drivers/altera-fpga2hps-bridge/bind
```

If a bridge has no driver bound the command will fail silently — no harm done.

### Make the bitstream persistent across reboots

At boot, U-Boot loads `soc_system.rbf` from the FAT32 boot partition (`/dev/mmcblk0p1`).
To replace it, mount the partition, overwrite the file, then unmount:

```bash
mkdir -p /mnt/boot
mount /dev/mmcblk0p1 /mnt/boot
cp ~/fpga_bitstream.rbf /mnt/boot/soc_system.rbf
umount /mnt/boot
```

The new bitstream will be loaded automatically on the next reboot.
