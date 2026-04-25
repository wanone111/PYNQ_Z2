# ============================================================================
# HLS Synthesis Script for Video Preprocess IP
# Target: PYNQ-Z2 (Zynq-7000, xc7z020clg400-1)
# ============================================================================

open_project -reset video_preprocess

# Set the top function
set_top video_preprocess

# Add source files
add_files src/top.cpp -cflags "-I include"

# Add testbench
add_files -tb tb/tb_top.cpp -cflags "-I include"

# Open solution
open_solution -reset "solution1"

# Set target device
set_part {xc7z020clg400-1}
create_clock -period 10 -name default

# HLS directives are embedded in source code via pragmas

# Run C synthesis
csynth_design

# Run C simulation (optional, can be skipped for speed)
# csim_design

# Export RTL (optional)
# export_design -format ip_catalog -output ./ip_output

puts "=== HLS Synthesis Complete ==="
puts "Check report in: video_preprocess/solution1/syn/report/csynth.rpt"
