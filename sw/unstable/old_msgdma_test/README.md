# msgdma_test

Kernel module for testing the mSGDMA pipeline: `counter_source → mSGDMA → kmalloc buffer`.

On load, the module requests a DMA channel, transfers 512 bytes from the FPGA counter source into a kernel buffer, and prints the first 8 beats (64-bit each) to the kernel log. Each beat is verified against the expected counter value (0, 1, 2, ...).

## Prerequisites

Set the `CROSS_COMPILE` environment variable before building. For this project's toolchain:

```bash
export CROSS_COMPILE=arm-none-linux-gnueabihf-
```

The kernel source tree at `repos/linux-socfpga` must already be built as described in the main `README.md`.

## Build

```bash
make
```

This produces `msgdma_test.ko`.

## Usage

Copy the module to the board (e.g. via SCP):

```bash
scp msgdma_test.ko root@<board-ip>:/tmp/
```

On the board, load the module:

```bash
insmod /tmp/msgdma_test.ko
```

Check the output:

```bash
dmesg | tail -20
```

Expected output (with `INITIAL_VALUE=0`, `INCREMENT=1`):

```
msgdma_test: channel ...
msgdma_test: OK — first 8 beats (64-bit):
  [0] 0x0000000000000000
  [1] 0x0000000000000001
  [2] 0x0000000000000002
  ...
  [7] 0x0000000000000007
```

Any line marked `*** MISMATCH ***` indicates a data integrity or ordering error.

To unload:

```bash
rmmod msgdma_test
```
