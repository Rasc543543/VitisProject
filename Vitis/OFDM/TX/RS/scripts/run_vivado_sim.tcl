# ============================================================
# run_vivado_sim.tcl
# Usage:
#   Batch : vivado -mode batch -source scripts/run_vivado_sim.tcl -tclargs batch
#   GUI   : vivado -mode gui   -source scripts/run_vivado_sim.tcl -tclargs gui
# ============================================================

set mode "batch"
if { [llength $argv] > 0 } { set mode [lindex $argv 0] }

set script_dir [file dirname [file normalize [info script]]]
set root_dir   [file normalize "$script_dir/.."]

# ---- 從 create_project.tcl 讀取專案名稱與 part（不執行 HLS 指令）----
set cfg_fh   [open "$script_dir/create_project.tcl" r]
set cfg_text [read $cfg_fh]
close $cfg_fh

regexp {set project_name\s+"([^"]+)"} $cfg_text -> project_name
regexp {set part\s+"([^"]+)"}         $cfg_text -> part_name

set rtl_dir "$root_dir/$project_name/solution1/syn/verilog"
set sim_dir "$root_dir/vivado_sim"
set out_dir "$root_dir/output"

puts "\n=== Vivado Simulation : $mode ==="
puts "  Project : $project_name"
puts "  Part    : $part_name"
puts "  RTL dir : $rtl_dir"

# ---- 檢查 RTL 是否存在 ----
if { ![file isdirectory $rtl_dir] } {
    puts "\n\[ERROR\] RTL not found: $rtl_dir"
    puts "         先跑 HLS synthesis（RUN_SYN=1）"
    exit 1
}

set rtl_files [glob -nocomplain "$rtl_dir/*.v"]
if { [llength $rtl_files] == 0 } {
    puts "\n\[ERROR\] No .v files in $rtl_dir"
    exit 1
}

# ---- 建立 Vivado 專案 ----
create_project vivado_sim $sim_dir -part $part_name -force

# ---- 加入 HLS 生成的 RTL ----
add_files $rtl_files
foreach dat [glob -nocomplain "$rtl_dir/*.dat"] {
    add_files $dat
}

# ---- 加入 Testbench ----
add_files -fileset sim_1 "$root_dir/tb/tb_vivado.v"
set_property top        tb_vivado      [get_filesets sim_1]
set_property top_lib    xil_defaultlib [get_filesets sim_1]

update_compile_order -fileset sources_1
update_compile_order -fileset sim_1

# ---- 執行模擬 ----
if { $mode eq "batch" } {
    puts "\n>>> Batch simulation..."
    file mkdir $out_dir

    launch_simulation

    set vcd_path [file normalize "$out_dir/sim.vcd"]
    open_vcd  $vcd_path
    log_vcd   /tb_vivado/*
    restart
    run all
    close_vcd

    puts "<<< Simulation done"
    puts "=== VCD saved: $vcd_path ==="

} else {
    puts "\n>>> GUI mode — launching simulation..."
    launch_simulation
    puts "=== Vivado GUI ready. 手動加 signal 後按 Run ==="
}
