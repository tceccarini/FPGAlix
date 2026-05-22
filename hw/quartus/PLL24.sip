set_global_assignment -entity "PLL24" -library "lib_PLL24" -name IP_TOOL_NAME "altera_pll"
set_global_assignment -entity "PLL24" -library "lib_PLL24" -name IP_TOOL_VERSION "22.1"
set_global_assignment -entity "PLL24" -library "lib_PLL24" -name IP_TOOL_ENV "mwpim"
set_global_assignment -library "lib_PLL24" -name SPD_FILE [file join $::quartus(sip_path) "PLL24.spd"]

set_global_assignment -library "lib_PLL24" -name MISC_FILE [file join $::quartus(sip_path) "PLL24_sim/PLL24.vho"]
