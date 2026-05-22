#!/bin/sh
###############################################################################
#  resetCtrl.sh — Assert / deassert pclk domain reset via pclk_reset_controller
#  Register: 0xFF2001C0  bit 0: sw_release
#    0 -> reset asserted   (pclk domain held in reset)
#    1 -> reset deasserted (pclk domain running)
###############################################################################

ADDR=0xFF2001C0

while true; do
    echo "Select action:"
    echo "  1) Assert reset   (sw_release = 0)"
    echo "  2) Deassert reset (sw_release = 1)"
    printf "Choice [1/2]: "
    read CHOICE
    case "$CHOICE" in
        1) VAL=0x0; ACTION="Assert reset";   break ;;
        2) VAL=0x1; ACTION="Deassert reset"; break ;;
        *) echo "Invalid choice, please enter 1 or 2." ;;
    esac
done

printf "Writing (%s)... " "$ACTION"
devmem $ADDR 32 $VAL
echo "ok"

sleep 1

printf "Checking... "
READ=$(devmem $ADDR 32)
READ_NORM=$(printf "%d" "$READ"   2>/dev/null)
VAL_NORM=$(printf  "%d" "$VAL"   2>/dev/null)
if [ "$READ_NORM" = "$VAL_NORM" ]; then
    echo "ok (read back $READ)"
else
    echo "FAIL (expected $VAL, got $READ)"
fi
