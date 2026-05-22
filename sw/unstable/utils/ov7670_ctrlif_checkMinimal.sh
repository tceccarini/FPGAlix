#!/bin/sh
###############################################################################
#  ov7670_minimalCheck.sh — Verify registers written by ov7670_minimal.sh
#  Expected mode: VGA Bayer RGB 30fps @ XCLK=24MHz
###############################################################################

B=1      # I2C bus
A=0x21   # OV7670 address
D=0.002  # SCCB inter-operation delay (2ms)
FAIL=0

r() {
    i2cset -y $B $A $1
    sleep $D
    GOT=$(i2cget -y $B $A | tr 'a-f' 'A-F')
    EXP=$(echo "$2"        | tr 'a-f' 'A-F')
    sleep $D
    if [ "$GOT" = "$EXP" ]; then
        echo "  OK   $3  $1 = $GOT"
    else
        echo "  FAIL $3  $1 = $GOT (expected $EXP)  <<<<<"
        FAIL=$((FAIL + 1))
    fi
}

# Bayer mode selection
while true; do
    echo "Select Bayer output format to verify:"
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

echo "=== Register check: ov7670_minimal ($MODE) ==="

# Output format
r 0x12 $COM7_VAL COM7
r 0x0C 0x00      COM3
r 0x3E 0x00      COM14

# Scaling
r 0x70 0x3A      SCALING_XSC
r 0x71 0x35      SCALING_YSC
r 0x72 0x11      SCALING_DCWCTR
r 0x73 0xF0      SCALING_PCLK_DIV
r 0xA2 0x02      SCALING_PCLK_DELAY

# Clock
r 0x6B 0x4A      DBLV
r 0x11 0x01      CLKRC

# Auto exposure / gain / white balance
r 0x13 0xE7      COM8
r 0x3B 0x12      COM11

# Banding
r 0x9D 0x4C      BD50ST
r 0x9E 0x3F      BD60ST
r 0xA5 0x05      BD50MAX
r 0xAB 0x07      BD60MAX

echo ""
if [ $FAIL -eq 0 ]; then
    echo "=== ALL OK ==="
else
    echo "=== $FAIL ERROR(S) ==="
fi
