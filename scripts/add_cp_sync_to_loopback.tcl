# add_cp_sync_to_loopback.tcl
# 在 test_loopback 的 bd_ip BD 中插入 cp_inserter + ofdm_cfo_sync
# 位置: smart_ifft_0/m_axis → cp_inserter_0 → ofdm_cfo_sync_0 → smart_FFT_0/s_axis
# 使用方式: Tools > Run Tcl Script（在 test_loopback 專案已開啟的狀態下執行）

open_bd_design [get_files bd_ip.bd]

# ── 新增兩個 IP 實例 ──────────────────────────────────────────────
create_bd_cell -type ip -vlnv xilinx.com:hls:cp_inserter:1.0    cp_inserter_0
create_bd_cell -type ip -vlnv xilinx.com:hls:ofdm_cfo_sync:1.0  ofdm_cfo_sync_0

# ── 連接 Clock / Reset ─────────────────────────────────────────────
connect_bd_net [get_bd_ports ap_clk_0]   [get_bd_pins cp_inserter_0/ap_clk]
connect_bd_net [get_bd_ports ap_clk_0]   [get_bd_pins ofdm_cfo_sync_0/ap_clk]
connect_bd_net [get_bd_ports ap_rst_n_0] [get_bd_pins cp_inserter_0/ap_rst_n]
connect_bd_net [get_bd_ports ap_rst_n_0] [get_bd_pins ofdm_cfo_sync_0/ap_rst_n]

# ── 刪除舊連接（IFFT 直接接 FFT）──────────────────────────────────
delete_bd_objs [get_bd_intf_nets smart_ifft_0_m_axis]

# ── 建立新連接鏈路 ────────────────────────────────────────────────
connect_bd_intf_net [get_bd_intf_pins smart_ifft_0/m_axis]       \
                    [get_bd_intf_pins cp_inserter_0/input_stream]

connect_bd_intf_net [get_bd_intf_pins cp_inserter_0/output_stream] \
                    [get_bd_intf_pins ofdm_cfo_sync_0/input_stream]

connect_bd_intf_net [get_bd_intf_pins ofdm_cfo_sync_0/output_stream] \
                    [get_bd_intf_pins smart_FFT_0/s_axis]

# ── 驗證並儲存 ────────────────────────────────────────────────────
validate_bd_design
save_bd_design
regenerate_bd_layout

puts "Done: cp_inserter_0 + ofdm_cfo_sync_0 inserted between IFFT and FFT"
