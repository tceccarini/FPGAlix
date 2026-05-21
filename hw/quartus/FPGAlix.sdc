# FPGAlix timing constraints

# System clock – 50 MHz from on-board oscillator
create_clock -name clk_50 -period 20.0 [get_ports CLOCK_50]

# OV7670 pixel clock – 24 MHz default (12 MHz in RAW mode)
# Enters the FPGA on GPIO_0[1] (PIN_Y17), non-dedicated clock pin
create_clock -name cam_pclk -period 41.667 [get_ports {GPIO_0[1]}]

# Treat pclk as asynchronous to the system clock
set_clock_groups -asynchronous -group {clk_50} -group {cam_pclk}

derive_pll_clocks
derive_clock_uncertainty
