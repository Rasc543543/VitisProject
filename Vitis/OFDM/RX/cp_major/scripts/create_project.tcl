# ============================================================
# create_project.tcl
# Usage: vitis_hls -f scripts/create_project.tcl
# ============================================================

# ---- 每個專案修改這裡 ----
set project_name  "ofdm_cfo_sync_major"
set top_func      "ofdm_cfo_sync"
set part          "xczu48dr-fsvg1517-2-e"
set clock_period  "10"    ;# ns
set clock_uncertain "1.25" ;# ns

# ---- 路徑（不需要改）----
set script_dir [file dirname [file normalize [info script]]]
set root_dir   [file normalize "$script_dir/.."]

# ---- 建立專案 ----
open_project -reset $project_name

set_top $top_func

add_files "$root_dir/ofdm_cfo_sync.cc" \
    -cflags "-I$root_dir"

add_files -tb "$root_dir/testbench.cc" \
    -cflags "-I$root_dir" \
    -csimflags "-I$root_dir"

open_solution -reset "solution1"
set_part $part
create_clock -period $clock_period -name default
set_clock_uncertainty $clock_uncertain default

puts ""
puts "=== Project Created ==="
puts "  Name    : $project_name"
puts "  Top     : $top_func"
puts "  Part    : $part"
puts "  Clock   : ${clock_period} ns  (uncertainty: ${clock_uncertain} ns)"
puts "  Root dir: $root_dir"
puts "======================="
