# =============================================================================
# test3_dynamic_roi configurable HLS script
# Run command:
#   vitis-run --mode hls --tcl scripts/run_hls.tcl
#
# Default flow:
#   CSIM + CSYNTH only
#   NO COSIM
#   NO IP export
# =============================================================================

catch {exec sh -c "rm -f flex*.log *.jou *.log"}
puts "Start time: [clock format [clock seconds]]"

# -----------------------------------------------------------------------------
# Path settings
# -----------------------------------------------------------------------------
set script_dir   [file dirname [file normalize [info script]]]
set project_root [file normalize "$script_dir/.."]
set src_dir      "$project_root/src"
set inc_dir      "$project_root/include"
set tb_dir       "$project_root/tb"
set build_root   "$project_root/build"
set build_name   "hls_video_preprocess_wb_gamma_denoise"
set build_output "$build_root/$build_name"

file mkdir $build_root
file mkdir $build_output

puts "Project root : $project_root"
puts "Source dir   : $src_dir"
puts "Include dir  : $inc_dir"
puts "TB dir       : $tb_dir"
puts "Build output : $build_output"

# -----------------------------------------------------------------------------
# Project settings
# -----------------------------------------------------------------------------
open_project -reset "$build_output/video_preprocess_prj"
set_top video_preprocess

set common_cflags "-I $inc_dir -I $src_dir -D__SDSVHLS__ -std=c++17"

# -----------------------------------------------------------------------------
# Add design files
# -----------------------------------------------------------------------------
if {![file exists "$src_dir/top.cpp"]} {
    puts "ERROR: Missing required file: $src_dir/top.cpp"
    exit 1
}
add_files "$src_dir/top.cpp" -cflags "$common_cflags"

# Optional source files
if {[file exists "$src_dir/roi.cpp"]} {
    puts "Add optional source: $src_dir/roi.cpp"
    add_files "$src_dir/roi.cpp" -cflags "$common_cflags"
}

if {[file exists "$src_dir/isp.cpp"]} {
    puts "Add optional source: $src_dir/isp.cpp"
    add_files "$src_dir/isp.cpp" -cflags "$common_cflags"
}

if {[file exists "$src_dir/gamma_lut.cpp"]} {
    puts "Add optional source: $src_dir/gamma_lut.cpp"
    add_files "$src_dir/gamma_lut.cpp" -cflags "$common_cflags"
}

# Optional header files
if {[file exists "$inc_dir/top.h"]} {
    add_files "$inc_dir/top.h"
}
if {[file exists "$inc_dir/gamma_lut.h"]} {
    add_files "$inc_dir/gamma_lut.h"
}
if {[file exists "$inc_dir/roi.h"]} {
    add_files "$inc_dir/roi.h"
}
if {[file exists "$inc_dir/isp.h"]} {
    add_files "$inc_dir/isp.h"
}

# -----------------------------------------------------------------------------
# Add testbench
# -----------------------------------------------------------------------------
if {[file exists "$tb_dir/tb_top.cpp"]} {
    puts "Use testbench: $tb_dir/tb_top.cpp"
    add_files -tb "$tb_dir/tb_top.cpp" -cflags "$common_cflags"
} elseif {[file exists "$tb_dir/testbench.cpp"]} {
    puts "Use testbench: $tb_dir/testbench.cpp"
    add_files -tb "$tb_dir/testbench.cpp" -cflags "$common_cflags"
} else {
    puts "ERROR: No testbench found in $tb_dir"
    puts "Expected one of: tb_top.cpp or testbench.cpp"
    exit 1
}

# -----------------------------------------------------------------------------
# Solution settings
# -----------------------------------------------------------------------------
open_solution -reset solution1
set_part {xc7z020clg400-1}
create_clock -period 10.0 -name default

# -----------------------------------------------------------------------------
# Flow control switches
# -----------------------------------------------------------------------------
set RUN_CSIM   0
set RUN_CSYNTH 1
set RUN_COSIM  0
set RUN_EXPORT 1

puts "RUN_CSIM   = $RUN_CSIM"
puts "RUN_CSYNTH = $RUN_CSYNTH"
puts "RUN_COSIM  = $RUN_COSIM"
puts "RUN_EXPORT = $RUN_EXPORT"

# -----------------------------------------------------------------------------
# Run flow
# -----------------------------------------------------------------------------
if {$RUN_CSIM} {
    puts "========== CSIM =========="
    csim_design -clean
}

if {$RUN_CSYNTH} {
    puts "========== CSYNTH =========="
    csynth_design
}

if {$RUN_COSIM} {
    puts "========== COSIM =========="
    cosim_design
}

if {$RUN_EXPORT} {
    puts "========== EXPORT =========="
    export_design -format ip_catalog
}

puts "Done at: [clock format [clock seconds]]"
exit