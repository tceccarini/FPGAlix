#!/bin/sh
###############################################################################
#  ov7670_minimal.sh — VGA 640x480 Bayer RGB 30fps @ XCLK=24MHz
#  Minimal register set. Everything else stays at reset default.
#  AEC/AGC/AWB auto, 50/60Hz banding auto-detect.
###############################################################################

B=1      # I2C bus
A=0x21   # OV7670 address
D=0.002  # SCCB inter-operation delay (2ms)

w() { i2cset -y $B $A $1 $2; sleep $D; }
r() { i2cset -y $B $A $1; sleep $D; i2cget -y $B $A; sleep $D; }

echo "=== OV7670 Minimal Init ==="

# Bayer mode selection
while true; do
    echo "Select Bayer output format:"
    echo "  1) Raw Bayer RGB"
    echo "  2) Processed Bayer RGB"
    printf "Choice [1/2]: "
    read CHOICE
    case "$CHOICE" in
        1) COM7_VAL=0x01; MODE="Raw Bayer RGB";       break ;;
        2) COM7_VAL=0x05; MODE="Processed Bayer RGB"; break ;;
        *) echo "Invalid choice, please enter 1 or 2." ;;
    esac
done

# Sensor check
PID=$(r 0x0A)
[ "$PID" != "0x76" ] && echo "Sensor not found (PID=$PID)" && exit 1
echo "Sensor OK (PID=$PID)"

# Reset
w 0x12 0x80; sleep 0.05

# Output format: VGA Bayer RGB
w 0x12 $COM7_VAL  # COM7: VGA + Bayer mode
w 0x0C 0x00       # COM3: scaling off
w 0x3E 0x00       # COM14: no manual scaling
w 0x70 0x3A       # SCALING_XSC
w 0x71 0x35       # SCALING_YSC
w 0x72 0x11       # SCALING_DCWCTR
w 0x73 0xF0       # SCALING_PCLK_DIV
w 0xA2 0x02       # SCALING_PCLK_DELAY

# Clock: PLL x4, prescaler /2 -> 30fps
w 0x6B 0x4A       # DBLV:  PLL x4 + internal regulator
w 0x11 0x01       # CLKRC: divider 2

# AEC/AGC/AWB all auto + 50/60Hz banding auto-detect
w 0x13 0xE7       # COM8:  AGC+AEC+AWB ON, banding ON, fast AEC
w 0x3B 0x12       # COM11: 50/60Hz auto-detect, night mode OFF

# Banding step registers (used by auto-detect)
w 0x9D 0x4C       # BD50ST
w 0x9E 0x3F       # BD60ST
w 0xA5 0x05       # BD50MAX
w 0xAB 0x07       # BD60MAX

echo "=== Init complete ==="
echo "VGA $MODE, 30fps, AEC/AGC/AWB auto, 50/60Hz banding."
