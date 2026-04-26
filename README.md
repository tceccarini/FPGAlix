# FPGAlix

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

## Compiling U-Boot

Move into the U-Boot source directory and set the required environment variables:

```bash
cd repos/u-boot-socfpga
export ARCH=arm
export CROSS_COMPILE=arm-none-linux-gnueabihf-
```

Load the DE1-SoC default configuration and build:

```bash
make socfpga_cyclone5_defconfig
make -j$(nproc)
```

> ~~`make socfpga_de1_soc_defconfig`~~ — da verificare quale dei due è corretto per la DE1-SoC

The build produces `repos/u-boot-socfpga/u-boot-with-spl.sfp`, a combined image containing the SPL (Secondary Program Loader) and U-Boot proper in the Altera SoCFPGA format, ready to be written to the SD card.

The file is also accessible via the symlink `sdcard/u-boot-with-spl.sfp`, which points to the build output so that all SD card artifacts are reachable from a single directory.
