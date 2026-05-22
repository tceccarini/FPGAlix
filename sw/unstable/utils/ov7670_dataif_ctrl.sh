#!/bin/sh
###############################################################################
#  ov7670_dataif_ctrl.sh — Control OV7670 Data Interface
#  Registers (LW HPS-to-FPGA, via MM Clock Crossing Bridge):
#    0xFF200400  ctrl     R/W  bit0=enable  bit1=clear_counters(self-clearing)  bit2=cam_pwdn
#    0xFF200404  dropped  R    stat_dropped_cnt
###############################################################################

ADDR_CTRL=0xFF200400
ADDR_DROP=0xFF200404

while true; do
    echo "Select action:"
    echo "  0) Disable frame acquisition"
    echo "  1) Enable frame acquisition"
    echo "  2) Monitor dropped frames (Ctrl+C to exit)"
    echo "  3) Clear dropped frames counter"
    printf "Choice [0/1/2/3]: "
    read CHOICE
    case "$CHOICE" in
        0|1|2|3) break ;;
        *) echo "Invalid choice, please enter 0, 1, 2 or 3." ;;
    esac
done

case "$CHOICE" in

    0)
        CTRL=$(devmem $ADDR_CTRL 32)
        NEW=$(( CTRL & 0xFFFFFFFE ))  # clear bit 0 (enable), preserve others
        printf "Disabling... "
        devmem $ADDR_CTRL 32 $NEW
        echo "ok"
        sleep 1
        printf "Checking... "
        READ=$(devmem $ADDR_CTRL 32)
        if [ $(( READ & 0x1 )) -eq 0 ]; then
            echo "ok (enable bit = 0)"
        else
            echo "FAIL (enable bit still set, ctrl = $READ)"
        fi
        ;;

    1)
        CTRL=$(devmem $ADDR_CTRL 32)
        NEW=$(( CTRL | 0x1 ))  # set bit 0 (enable), preserve others
        printf "Enabling... "
        devmem $ADDR_CTRL 32 $NEW
        echo "ok"
        sleep 1
        printf "Checking... "
        READ=$(devmem $ADDR_CTRL 32)
        if [ $(( READ & 0x1 )) -eq 1 ]; then
            echo "ok (enable bit = 1)"
        else
            echo "FAIL (enable bit not set, ctrl = $READ)"
        fi
        ;;

    2)
        echo "Monitoring dropped frames every 2s (Ctrl+C to exit)..."
        while true; do
            DROP=$(devmem $ADDR_DROP 32)
            printf "  stat_dropped_cnt = %d\n" "$(( DROP ))"
            sleep 2
        done
        ;;

    3)
        CTRL=$(devmem $ADDR_CTRL 32)
        NEW=$(( CTRL | 0x2 ))  # set bit 1 (clear_counters); HW auto-clears it after 1 cycle
        printf "Clearing dropped frames counter... "
        devmem $ADDR_CTRL 32 $NEW
        echo "ok"
        ;;

esac
