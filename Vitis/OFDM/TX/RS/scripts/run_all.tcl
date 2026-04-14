# ============================================================
# run_all.tcl
# Usage: vitis_hls -f scripts/run_all.tcl
# 想跳過某步驟 → 把對應變數改成 0
# ============================================================

set RUN_CSIM    1
set RUN_SYN     1
set RUN_COSIM   0
set RUN_EXPORT  0
set CLEAN_BUILD 1

# ---- 處理 -tclargs 參數 ----
if { [llength $argv] > 0 } {
    set mode [lindex $argv 0]
    if { $mode eq "csim_only" } {
        set RUN_SYN    0
        set RUN_COSIM  0
        set RUN_EXPORT 0
        puts ">>> Mode: csim_only"
    } elseif { $mode eq "syn_only" } {
        set RUN_CSIM   0
        set RUN_COSIM  0
        set RUN_EXPORT 0
        puts ">>> Mode: syn_only"
    }
}

# ---- 載入專案設定 ----
set script_dir [file dirname [file normalize [info script]]]
set root_dir   [file normalize "$script_dir/.."]
source "$script_dir/create_project.tcl"

# ---- C Simulation ----
if { $RUN_CSIM } {
    puts "\n>>> C Simulation"
    if { $CLEAN_BUILD } {
        csim_design -O -clean
    } else {
        csim_design -O
    }
    puts "<<< csim done"
}

# ---- C Synthesis ----
if { $RUN_SYN } {
    puts "\n>>> C Synthesis"
    csynth_design
    puts "<<< synthesis done"
}

# ---- Co-Simulation ----
if { $RUN_COSIM } {
    puts "\n>>> Co-Simulation"
    cosim_design -O -rtl verilog
    puts "<<< cosim done"
}

# ---- Export IP ----
if { $RUN_EXPORT } {
    puts "\n>>> Export IP"
    set output_dir "$root_dir/output"
    file mkdir $output_dir
    export_design -format ip_catalog \
                  -output  $output_dir \
                  -description "RS Encoder TX IP" \
                  -version "1.0"
    puts "<<< IP exported to: $output_dir"
}

puts "\n=== All steps completed ==="
