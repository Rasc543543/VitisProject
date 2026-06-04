# add_cp_sync_to_brian.tcl
# 在 test_Brian 的 bd_test BD 中插入 cp_inserter + ofdm_cfo_sync
# 位置: smart_ifft_0/m_axis → cp_inserter_0 → ofdm_cfo_sync_0 → smart_FFT_0/s_axis
# 使用方式: Tools > Run Tcl Script（在 test_Brian 專案已開啟的狀態下執行）

open_bd_design [get_files bd_test.bd]

# ── 新增兩個 IP 實例 ──────────────────────────────────────────────
create_bd_cell -type ip -vlnv xilinx.com:hls:cp_inserter:1.0    cp_inserter_0
create_bd_cell -type ip -vlnv xilinx.com:hls:ofdm_cfo_sync:1.0  ofdm_cfo_sync_0

# ── 連接 Clock / Reset ─────────────────────────────────────────────
# test_Brian 的 clock/reset net 名稱需從 BD nets 確認
# 根據 bd_test.bd 的 nets section，clock 接的是 ap_clk_0，reset 是 ap_rst_n_0
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
