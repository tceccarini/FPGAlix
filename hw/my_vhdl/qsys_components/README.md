# FPGAlix Qsys Custom Components

Custom Avalon components for the FPGAlix project (Intel Cyclone V SoC, DE1-SoC board).

---

## ov7670_data_interface

Captures raw pixel data from an OV7670 camera and forwards it on an Avalon-ST source interface with SOP/EOP framing and backpressure support. Clocked on the camera pixel clock (`pclk`). The Avalon-MM slave runs on the same clock domain and must be accessed via an Avalon-MM Clock Crossing Bridge.

**Generic**

| Name | Type | Default | Description |
|---|---|---|---|
| `FRAME_SIZE` | natural | 307200 | Bytes per frame. Use `W×H` for RAW8, `W×H×2` for RGB565. |

**Interfaces**

| Interface | Type | Clock |
|---|---|---|
| `pclk` | Clock sink | — |
| `reset_n` | Reset sink | pclk |
| `mm` | Avalon-MM slave | pclk |
| `st_source` | Avalon-ST source | pclk |
| `camera_if_conduit` | Conduit (export) | — |

**Register map** (base address via MM Clock Crossing Bridge)

| Offset | Name | Access | Description |
|---|---|---|---|
| 0x0 | `ctrl` | R/W | Control register |
| 0x4 | `stat_dropped_cnt` | R | Dropped frame counter |

**ctrl register — bit map**

| Bit | Name | Description |
|---|---|---|
| 0 | `ctrl_enabled` | 1 = enable frame acquisition |
| 1 | `ctrl_clear_counters` | Write 1 to clear `stat_dropped_cnt` (self-clearing) |
| 2 | `ctrl_cam_pwdn` | Forwarded to `cam_pwdn` pin |
| [31:3] | — | Reserved |

**Conduit pins**

| Signal | Direction | Description |
|---|---|---|
| `cam_pwdn` | out | Camera power-down (follows `ctrl_cam_pwdn`) |
| `cam_href` | in | Horizontal reference (active line) |
| `cam_vsync` | in | Vertical sync (frame boundary) |
| `cam_data` | in | 8-bit pixel data bus |

> `cam_reset_n` has been removed. The physical camera reset pin is now driven directly by `pclk_reset_controller.camera_reset_n` (see below).

**FSM states**

| State | Description |
|---|---|
| `WAIT_SYNC` | Waits for falling edge of `cam_vsync` and `ctrl_enabled = 1`. Resets byte counter. |
| `ACQUIRE` | Transfers pixels while `cam_href = 1`. Raises SOP on byte 0, EOP on byte `FRAME_SIZE-1`. Frame termination is driven by `FRAME_SIZE`, **not** by `cam_vsync`. If `st_ready = 0` at any point, increments `stat_dropped_cnt` and transitions to `WAIT_READY`. |
| `WAIT_READY` | Downstream applied backpressure mid-frame. Holds until `st_ready = 1`, then sends a closing EOP (allowing the downstream to discard the incomplete packet) and returns to `WAIT_SYNC`. |

---

## pclk_reset_controller

Controls the reset of the pclk clock domain. Receives the camera pixel clock (`pclk_in`), forwards it as a Qsys clock source (`pclk_out`), and generates two reset signals:

- `pclk_reset_n` — synchronised to `pclk` via a two-FF synchroniser (for the FPGA acquisition logic)
- `camera_reset_n` — purely combinatorial from the `sys_clk` domain (for the physical camera reset pin)

The separation avoids the circular dependency where the camera reset pin would depend on `pclk`, which the camera itself generates.

**Interfaces**

| Interface | Type | Clock |
|---|---|---|
| `sys_clock` | Clock sink | — |
| `sys_reset_n` | Reset sink | sys_clock |
| `sys_mm` | Avalon-MM slave (R/W) | sys_clock |
| `pclk_in` | Clock sink | — |
| `pclk_out` | Clock source | pclk_in |
| `pclk_reset_n` | Reset source | pclk_in |
| `camera_reset_n_conduit` | Conduit (export) | sys_clock |

**Register map** (base address on main Avalon-MM bus)

| Offset | Name | Access | Description |
|---|---|---|---|
| 0x0 | `sw_release` | R/W | Bit 0 = 1 to release reset; 0 to assert it |

**Reset logic**

```
camera_reset_n = sys_reset_n AND sw_release          (combinatorial, sys_clk domain)
pclk_reset_n   = synchronise_to_pclk(camera_reset_n) (two-FF, pclk domain)
```

Assert is asynchronous (immediate). Deassert of `pclk_reset_n` is synchronised to `pclk`. Quartus attribute `SYNCHRONIZER_IDENTIFICATION FORCED` is set on the first FF.

**Conduit pins**

| Signal | Direction | Description |
|---|---|---|
| `camera_reset_n` | out | Physical camera reset pin — combinatorial, no pclk dependency |

**Typical software sequence**

1. Assert reset: write `0` to `sw_release`
2. Configure camera via I2C (camera is in reset, PCLK domain is held)
3. Release reset: write `1` to `sw_release`
4. `camera_reset_n` goes high immediately; `pclk_reset_n` follows after two pclk edges

---

## hex_display_controller

Displays a 20-bit integer (0–999 999) on the six 7-segment displays of the DE1-SoC (HEX0–HEX5, active-low, no decimal point). Leading zeros are suppressed (right-aligned). Values above 999 999 show `------`. A single bit controls the display on/off.

BCD conversion is performed by a combinatorial Double Dabble sub-entity (`bin_to_bcd_6digit`) — zero DSP blocks, purely LUT-based.

**Interfaces**

| Interface | Type |
|---|---|
| `clk` / `reset_n` | System clock and reset |
| Avalon-MM slave | Single register, R/W |
| `hex0_n`..`hex5_n` | 7-bit conduit outputs (active low) |

**Register map**

| Offset | Name | Access | Description |
|---|---|---|---|
| 0x0 | `ctrl` | R/W | Display control register |

**ctrl register — bit map**

| Bit | Name | Description |
|---|---|---|
| 31 | `enabled` | 1 = display on; 0 = all segments off |
| [30:20] | — | Reserved (ignored on write, reads back as written) |
| [19:0] | `value` | Integer to display (0–999 999). Values > 999 999 show `------` |

**7-segment encoding** (active low, `bit0`=segment a … `bit6`=segment g)

| Digit | Binary | Hex |
|---|---|---|
| 0 | 1000000 | 0x40 |
| 1 | 1111001 | 0x79 |
| 2 | 0100100 | 0x24 |
| 3 | 0110000 | 0x30 |
| 4 | 0011001 | 0x19 |
| 5 | 0010010 | 0x12 |
| 6 | 0000010 | 0x02 |
| 7 | 1111000 | 0x78 |
| 8 | 0000000 | 0x00 |
| 9 | 0010000 | 0x10 |
| blank | 1111111 | 0x7F |
| dash | 0111111 | 0x3F |

---

## simple_byte_pixel_counter_generator

Debug/test component. Generates a synthetic Avalon-ST video stream with 8-bit data. Each beat carries the low byte of a running pixel counter (0x00, 0x01, …, 0xFF, 0x00, …). Produces one complete frame of `WIDTH × HEIGHT` pixels with correct SOP/EOP framing. Respects backpressure (`sink_ready`).

**Generics**

| Name | Default | Description |
|---|---|---|
| `WIDTH` | 640 | Frame width in pixels |
| `HEIGHT` | 480 | Frame height in pixels |

**Use case**: inject a known pattern into the DMA pipeline to verify the ST→FIFO→mSGDMA chain without a physical camera.

---

## simple_qword_pixel_cnt_generator

Identical in structure to `simple_byte_pixel_counter_generator` but outputs **64-bit** beats. Each beat carries the full 64-bit pixel counter value. Useful for verifying that the downstream DMA handles wide Avalon-ST transfers correctly and for bandwidth measurements.

**Generics**

| Name | Default | Description |
|---|---|---|
| `WIDTH` | 640 | Frame width in pixels |
| `HEIGHT` | 480 | Frame height in pixels |
