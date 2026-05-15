# simple_qword_pixel_cnt_generator_hw.tcl

#
# simple_qword_pixel_cnt_generator "Simple Qword Pixel-Counter Generator" v1.1
# Tiziano Ceccarini 2026.05.14
#

package require -exact qsys 16.1


#
# module simple_qword_pixel_cnt_generator
#
set_module_property DESCRIPTION ""
set_module_property NAME simple_qword_pixel_cnt_generator
set_module_property VERSION 1.1
set_module_property INTERNAL false
set_module_property OPAQUE_ADDRESS_MAP true
set_module_property GROUP My/Debug
set_module_property AUTHOR "Tiziano Ceccarini"
set_module_property DISPLAY_NAME "Simple Qword Pixel-Counter Generator"
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true
set_module_property EDITABLE true
set_module_property REPORT_TO_TALKBACK false
set_module_property ALLOW_GREYBOX_GENERATION false
set_module_property REPORT_HIERARCHY false
set_module_property VALIDATION_CALLBACK validate_frame_size


#
# file sets
#
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL simple_qword_pixel_cnt_generator
set_fileset_property QUARTUS_SYNTH ENABLE_RELATIVE_INCLUDE_PATHS false
set_fileset_property QUARTUS_SYNTH ENABLE_FILE_OVERWRITE_MODE false
add_fileset_file simple_qword_pixel_cnt_generator.vhd VHDL PATH simple_qword_pixel_cnt_generator.vhd TOP_LEVEL_FILE


#
# parameters
#
add_parameter WIDTH INTEGER 640 ""
set_parameter_property WIDTH DEFAULT_VALUE 640
set_parameter_property WIDTH DISPLAY_NAME WIDTH
set_parameter_property WIDTH TYPE INTEGER
set_parameter_property WIDTH UNITS None
set_parameter_property WIDTH ALLOWED_RANGES 8:2147483647
set_parameter_property WIDTH DESCRIPTION ""
set_parameter_property WIDTH HDL_PARAMETER true
add_parameter HEIGHT INTEGER 480 ""
set_parameter_property HEIGHT DEFAULT_VALUE 480
set_parameter_property HEIGHT DISPLAY_NAME HEIGHT
set_parameter_property HEIGHT TYPE INTEGER
set_parameter_property HEIGHT UNITS None
set_parameter_property HEIGHT ALLOWED_RANGES 1:2147483647
set_parameter_property HEIGHT DESCRIPTION ""
set_parameter_property HEIGHT HDL_PARAMETER true


#
# connection point st_source
#
add_interface st_source avalon_streaming start
set_interface_property st_source associatedClock clock
set_interface_property st_source associatedReset reset
set_interface_property st_source dataBitsPerSymbol 8
set_interface_property st_source errorDescriptor ""
set_interface_property st_source firstSymbolInHighOrderBits true
set_interface_property st_source maxChannel 0
set_interface_property st_source readyLatency 0
set_interface_property st_source symbolsPerBeat 8
set_interface_property st_source ENABLED true
set_interface_property st_source EXPORT_OF ""
set_interface_property st_source PORT_NAME_MAP ""
set_interface_property st_source CMSIS_SVD_VARIABLES ""
set_interface_property st_source SVD_ADDRESS_GROUP ""

add_interface_port st_source data data Output 64
add_interface_port st_source startofpacket startofpacket Output 1
add_interface_port st_source endofpacket endofpacket Output 1
add_interface_port st_source valid valid Output 1
add_interface_port st_source sink_ready ready Input 1


#
# connection point clock
#
add_interface clock clock end
set_interface_property clock clockRate 0
set_interface_property clock ENABLED true
set_interface_property clock EXPORT_OF ""
set_interface_property clock PORT_NAME_MAP ""
set_interface_property clock CMSIS_SVD_VARIABLES ""
set_interface_property clock SVD_ADDRESS_GROUP ""

add_interface_port clock clock clk Input 1


#
# connection point reset
#
add_interface reset reset end
set_interface_property reset associatedClock clock
set_interface_property reset synchronousEdges DEASSERT
set_interface_property reset ENABLED true
set_interface_property reset EXPORT_OF ""
set_interface_property reset PORT_NAME_MAP ""
set_interface_property reset CMSIS_SVD_VARIABLES ""
set_interface_property reset SVD_ADDRESS_GROUP ""

add_interface_port reset reset_n reset_n Input 1


#
# validation
#
proc validate_frame_size {} {
    set w [get_parameter_value WIDTH]
    set h [get_parameter_value HEIGHT]
    set total [expr {$w * $h}]
    if {$total > 4294967295} {
        send_message error "WIDTH * HEIGHT ($total) exceeds 32-bit unsigned range (max 4294967295)"
    }
}
