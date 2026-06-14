# ⚠️ WARNING — PCB Hardware Bug

**Do NOT keep the camera (OV7670) in RESET or POWER-DOWN for extended periods.**

Doing so can cause the line buffer to overheat and may permanently damage it.
Always keep RESET / POWER-DOWN states as brief as possible (transients only), and never leave the camera parked in these
states for minutes, or longer.

---

## Summary

The data bus between the OV7670 camera module and the FPGA passes through an
`SN74ALVC16244A` line buffer. This specific part (the **ALVC** variant, *without*
the "H" suffix) does **not** include active bus-hold circuitry on its data inputs.

When the OV7670 is held in RESET or placed in POWER-DOWN, it stops actively
driving its output lines (D0–D7, PCLK, HREF, VSYNC). Those lines become
effectively floating / high-impedance. Because the buffer has no bus-hold to
latch a valid level, its inputs are then left floating while the buffer itself
is still powered.

This is the root of the problem.

---

## Why a floating input overheats the buffer

The `SN74ALVC16244A` has CMOS input stages. Each input stage has a
complementary pair of transistors (a P-channel pull-up and an N-channel
pull-down).

- When an input sits at a clean logic HIGH or LOW, only one transistor conducts
  and the stage draws almost no static current.
- When an input is left **floating**, it drifts toward the switching threshold
  (around Vcc/2). In that region **both** transistors conduct at the same time,
  creating a direct conduction path from Vcc to GND through the stage. This is
  called **shoot-through** (or crowbar) current. It does no useful work — it is
  converted entirely into heat.

Two factors make this worse here:

1. **Noise-induced oscillation.** A floating high-impedance input picks up board
   noise and parasitic coupling, causing it to wobble around the threshold. Each
   threshold crossing triggers another shoot-through pulse, and the stage may
   begin to oscillate, adding dynamic switching losses on top of the static
   shoot-through current.

2. **Many channels in parallel.** This is a 16-bit buffer. If several data lines
   float at once (which is exactly what happens when the camera stops driving the
   bus), the parasitic current is multiplied across all affected channels. This
   is how the device goes from "slightly warm" to "hot enough to risk damage."

Because RESET and POWER-DOWN are exactly the states in which the camera stops
driving the bus, these are the states that trigger the fault. POWER-DOWN is the
most dangerous case in practice, since the system may be intended to remain in
that low-power state for a long time — which means the buffer could overheat for
the entire duration of standby.

---

## Mitigation in firmware / system control (immediate, no rework)

Until one of the hardware fixes below is applied, the safest measure is to
**avoid leaving the camera in RESET or POWER-DOWN**:

- Sequence RESET and POWER-DOWN so that these states last only the minimum
  required time, then return to normal operation.
- Never park the camera in POWER-DOWN as a long-term idle/standby strategy with
  the current PCB.

The shoot-through still occurs during brief transients, but the buffer does not
have time to overheat in a few minutes. The thermal damage comes from
*sustained* floating states, not from short transitions.

---

## Permanent fix — two options

### Option 1 — Replace the buffer with a bus-hold variant

Replace the `SN74ALVC16244A` with the pin-compatible **bus-hold** version:

- **`SN74ALVCH16244A`** (note the **H**), or equivalently `SN74LVCH16244A`.

The "H" variant includes **active bus-hold** on the data inputs. Bus-hold is a
weak internal latch that holds the last valid logic level on an input when that
input is left undriven / high-impedance. This prevents the input from floating
to the threshold, eliminating the shoot-through current and the resulting
overheating — automatically, with no external components.

Advantages:
- Cleanest solution; no extra passives, no floating inputs ever.
- Same footprint and pinout (drop-in replacement on the existing PCB).

Notes:
- With a bus-hold part you must **not** add external pull-up/pull-down resistors
  on the data inputs — they fight the bus-hold circuit and waste power. Bus-hold
  replaces them, it does not coexist with them.

### Option 2 — Add pull-up / pull-down resistors on the data lines

If the existing `SN74ALVC16244A` is kept, add weak resistors to give every
buffer input a defined level when the camera stops driving the bus.

- Place a pull-up (to Vcc) **or** pull-down (to GND) on each of the affected
  buffer input lines: **D0–D7, PCLK, HREF, VSYNC**.
- Pull-**down** to GND is generally the safer default for a data bus that would
  otherwise float.
- Value: a few tens of kΩ (e.g. **47 kΩ–100 kΩ**). High enough not to disturb the
  PCLK timing or overload the camera's drivers; low enough to firmly define the
  level against board noise.
- These can be added as a rework on the existing board (0402/0603 resistors, or a
  bussed resistor array) — connect each line to GND (or Vcc) near the buffer
  inputs. No PCB respin is required.

Advantages:
- Uses the buffer already fitted; only adds passives.

Trade-offs:
- Adds a small constant current draw whenever the corresponding line is at the
  opposite level to the resistor's rail.
- More components / rework points than Option 1.

---

## Recommendation

If a board rework / reorder is acceptable, **Option 1 (swap to the `...H...`
bus-hold variant)** is the cleanest and most robust fix. If the current buffer
must stay, **Option 2 (pull-up/pull-down resistors)** resolves the issue with a
simple passive rework on the existing PCB.

In all cases, until a hardware fix is in place: **do not keep the camera in
RESET or POWER-DOWN for extended periods.**
