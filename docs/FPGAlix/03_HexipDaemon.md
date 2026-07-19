# 03. hexip_daemon

## 1. Introduction

`sw/hexip_daemon` is a small daemon that displays the board's IPv4 address on the DE1-SoC's 7-segment hex displays, through the `hex_display_controller` Qsys component, reached at physical address `0xFF200140` over the LW HPS-to-FPGA bridge.

Every 3 seconds, the IPv4 address of a given network interface (`eth0` by default, overridable as the first command-line argument) is re-read with `getifaddrs()`. Once an address is found, its four octets are displayed in sequence, one second each, separated by a brief blank and followed by a trailing dash, and the cycle repeats. Until an address is found (interface down, no DHCP lease yet), the display blinks a single dash instead, once a second, so that the daemon's own state remains visible even without an address to show.

The display's control register is written to directly, via `mmap()` on `/dev/mem`:

- bit 31 enables the display
- bits `[19:0]` hold the decimal value shown
- any value above `999999` forces a dash
- `SIGTERM`/`SIGINT` are handled by turning the display off and releasing the mapping before the process exits

## 2. Building

```bash
cd sw/hexip_daemon
export CROSS_COMPILE=arm-none-linux-gnueabihf-
make
```

This produces `bin/hexip_daemon`.

## 3. Deploying as a daemon

`sdcard/rootfs_overlay/` already contains what is needed, as two symlinks pointing at this component's own files:

- `opt/FPGAlix/bin/hexip_daemon` → `sw/hexip_daemon/bin/hexip_daemon`, the binary built in [Section 2](#2-building)
- `etc/init.d/S60hexip_daemon` → `sw/hexip_daemon/init.d/S60hexip_daemon`, a BusyBox-style init script (`S60` denoting priority 60 at startup), which starts the daemon on `eth0`

Since these are symlinks, building the daemon is enough for `rootfs_overlay/` to hold its current binary; no separate install step exists. Deploying it means copying both files onto the SD card's root filesystem partition (the second partition, ext4) and rebooting the board:

```bash
sudo mount /dev/sdX2 /mnt
sudo mkdir -p /mnt/opt/FPGAlix/bin
sudo cp -L sdcard/rootfs_overlay/opt/FPGAlix/bin/hexip_daemon /mnt/opt/FPGAlix/bin/
sudo cp -L sdcard/rootfs_overlay/etc/init.d/S60hexip_daemon /mnt/etc/init.d/
sudo umount /mnt
```

Replace `/dev/sdX2` with the SD card's actual root filesystem partition. On the next boot, the `S60` script starts the daemon automatically.
