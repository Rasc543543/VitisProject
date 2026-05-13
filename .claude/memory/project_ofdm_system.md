---
name: OFDM 通訊系統架構知識
description: 8PSK/QPSK OFDM 通訊系統完整架構、IP 鏈路、資料格式與數量
type: project
originSessionId: f22d9495-941b-4005-8979-68562bfb925d
---
# OFDM 通訊系統架構（8PSK / QPSK 雙模式）

## 專案位置
- TX Vivado 專案：`C:\Vivado\tx_loopback`
- RX Vivado 專案：`C:\Vivado\rx_loopback_8psk`
- HLS IP 原始碼：`C:\VitisProject\Vitis\OFDM\`

## TX 鏈路
```
Input (274B/8PSK 或 188B/QPSK)
→ energy_dispersal_0       XOR PRBS (prbs_table_400.h, 400 bytes 表)
→ rs_encoder_tx_0          RS 編碼（見下方 RS 參數）
→ tx_interleaver_dynam_0   動態交錯 (8-bit)
→ axi_viterbi_encode_w_0   卷積編碼 rate 1/2
→ tx_interleaver64_dyn_0   64-based 交錯 (8-bit)
→ symbol_mapper_hls_0      bits → 8PSK/QPSK 符號 (32-bit FP16 IQ)
                           【自動模式偵測】依輸入 byte 數判斷：
                           4896 bits (612B) → 8PSK（3 bits/sym × 1632）
                           3264 bits (408B) → QPSK（2 bits/sym × 1632）
                           不需要外部 mode 控制，TX 鏈路完全自動適應
→ tx_interleaver_float_0   Float IQ 交錯 (32-bit)
→ reference_signal_tx_0    插入 273 個 Pilots + Guard → 2048 tones
→ smart_ifft_0             IFFT → 時域 2048×32bit
→ Output
```

## RX 鏈路（Block Design: bd_8psk_loop）
```
Input (2048×32bit, 時域 IQ)
→ smart_FFT_0              FFT → 2048 頻域 bin (32-bit fixed, Q[31:16] I[15:0])
→ ref_rx_packed_0          通道等化 → 1632×32bit (FP16 IQ, {Q_fp16, I_fp16})
→ rx_deinterleaver_flo_0   Float IQ 解交錯, 矩陣 51×32 (1632→1632)
→ p8psk_demap_packed_0     8PSK Soft Demap → 4896×8bit LLR
→ rx_deinterleaver64_d_0   64-based 解交錯 (4896→4896)
→ Viterbi_decode_0         rate 1/2 解碼 (4896→306B/8PSK 或 204B/QPSK)
→ rx_deinterleaver_dyn_0   動態解交錯 (306→306)
→ rs_decoder_rx_0          RS 解碼 (306→274B/8PSK 或 204→188B/QPSK)
→ energy_descramble_0      XOR PRBS → 274B/8PSK 或 188B/QPSK 輸出
```

## 各模式資料量對照表

| 位置 | 8PSK | QPSK |
|------|------|------|
| TX/RX 原始資料 | **274 bytes** | **188 bytes** |
| RS 編碼後 | 306 bytes | 204 bytes |
| 卷積碼後 (coded bits) | 4896 bits | 3264 bits |
| OFDM 符號數 | 1632 (3bits/sym) | 1632 (2bits/sym) |
| FFT 大小 | 2048 | 2048 |
| Viterbi 輸出 | **306 bytes** | **204 bytes** |
| RS 解碼後輸出 | **274 bytes** | **188 bytes** |

## RS 參數

### 8PSK 模式
- TX encoder: input 274B → 兩段各 137B + 16B parity = 2 × RS(153,137) → 306B
- RX decoder: input 306B → 2 chunks, N=153, K=137, T=8 → 274B

### QPSK 模式
- TX encoder: input 188B → RS(204,188), T=8, 51 zero padding → 204B
- RX decoder: input 204B → N=204, K=188 → 188B

## OFDM 幀結構（2048-point FFT）
- `FFT_LENGTH` = 2048
- `DATA_TONES` = 1632
- `PILOT_TONES` = 273（間距 7，±70 CFO 掃描）
- `LEFT_GUARD` = 72, `RIGHT_GUARD` = 71
- Pilot 格式：PILOT_ROM_INV[273]（±0.75 序列）

## ref_rx_packed 等化邏輯
- 輸入 fixed-point Q15 (除以 2048 還原)
- CFO 掃描 ±70，找最佳 Pilot alignment
- 線性插值等化（相鄰 Pilot 間 6 個 data tone）
- 輸出 FP16 {Q[31:16], I[15:0]}
- **已修正問題**：PILOT_ROM_INV 原本 122/273 個符號錯誤 → 相鄰 pilot 估出的 H 符號相反 → 插值穿越 0 → 大幅放大
- **修正方式**：從 TX PILOT_ROM (ref_tx_defs.h) 直接推導：PILOT_ROM_INV[k] = 0.75 × sign(PILOT_ROM[k])
- **已修正**：內部通道估計用 float（非 half）避免 FP16 精度不足

## 模式選擇關鍵原則

- **TX 鏈路自動適應**：symbol_mapper 依輸入 bits 數量自動切換 QPSK/8PSK，不需要額外控制
- **RX demap 是真正的模式限制**：demap IP 是固定版本（p8psk_demap_packed 或 qpsk_demap_packed），無法自動切換
- **tx_frag mode 必須與 BD 的 demap IP 對應**：
  - BD 用 `qpsk_demap_packed_0` → tx_frag mode=0（188B/frame）
  - BD 用 `p8psk_demap_packed_0` → tx_frag mode=1（274B/frame）
- 若 tx_frag mode 與 demap 不符：TX 端正常，RX demap 解錯

## 8PSK Demapper LLR 定義
- `llr_b0` = `(|I| - |Q|) × SQRT2_INV` → 決定 b0 (角度象限)，**輸出順序第 1**
- `llr_b1` = `I` → 決定 b1 (I 正負)，**輸出順序第 2**
- `llr_b2` = `Q` → 決定 b2 (Q 正負)，**輸出順序第 3**
- 量化：[-7, +7] → ap_int<8>；正=0，負=1
- `FRAME_SYMBOLS` = 1632，每符號輸出 3 bytes → 4896 bytes
- **Symbol Mapper 位元順序**：`symbol_index = {b2, b1, b0}`（b2 MSB，b0 LSB）
- **具體範例**：TX bytes `00 01 01`（LSB=0,1,1）→ symbol_index=6 → I=-0.9239, Q=-0.3827 → LLR: `0x07, 0xF9, 0xF9`
- **8PSK 星座表（index→I,Q）**：
  - 0=+0.9239/+0.3827, 1=+0.3827/+0.9239, 2=-0.9239/+0.3827, 3=-0.3827/+0.9239
  - 4=+0.9239/-0.3827, 5=+0.3827/-0.9239, 6=-0.9239/-0.3827, 7=-0.3827/-0.9239

## energy_descramble
- PRBS 表存在 ROM (prbs_table_400.h)，每封包歸零重算 index
- 輸出 = input XOR prbs_rom[byte_idx]
- 最大封包長度 400 bytes

## TB 關鍵設定 (tb_8psk_loopback.sv)
- 讀取 TX 模擬輸出：`C:/Vivado/tx_loopback/.../tx_ifft_2048_payload.txt`
- 每 frame 送 2048 個 32-bit samples
- 內部信號正確路徑：`DUT.bd_8psk_loop_i.ref_rx_packed_0.inst.output_stream_TVALID`

## 開發進度（截至 2026-04-22，最後更新同日）
- ✅ ref_rx_packed.cpp：ZF 等化完整（Y×H*/|H|²）、通道估計 float、PILOT_ROM_INV 修正
- ✅ ref_rx 開發記錄：`Vitis/OFDM/RX/Refer/review/dev_log.md`（含優化提案：未來可移除 /|H|² 省 DSP）
- ✅ 全系統串接驗證（C:/Vivado/fft_loopback）：8PSK 274B loopback 完全正確
- ✅ git 初始化：C:/VitisProject，合成產物排除，最新 commit 633eafd
- ✅ TX/RS：.vscode/tasks.json 已補齊，src/rs_encoder_tx.h 已有 stdint.h，csim 進行中
- ✅ QPSK 188B：已驗證（2026-05-06），BER=0，全模式通過
- ✅ Vitis 上板應用程式碼：`C:/Vitis/test_loop/app_loop/src/main.c`（已加 LLR dump 輸出）
- ✅ **DC bin (pilot[136]) 問題已修復（2026-04-22）**：
  - 根因：Xilinx FFT IP DC bin 在 IFFT→FFT 污染（直連 I=+2730，IFFT+FFT 後 I=+1403 Q=-1328）
  - **修復方式（僅改 RX，TX 不動）**：
    - `ref_rx_packed.cpp` CFO scan：`if (p == 136) continue;`（跳過 DC bin）
    - `ref_rx_packed.cpp` Loop_Process_Packed：i==136 skip，i==135 改用 pilot[137] 為右錨點，14-tone span 線性插值，k=7 跳過 DC 位置不輸出
  - **驗證結果**：Sim BER = 0/4896，HW BER = 0/4896（完全驗證）
  - newLLR.txt（新 HW，IFFT+FFT + fix）與 LLR.txt（直連 HW）完全一致，DC bin 問題徹底解決

## 全系統 Vivado 專案
- 路徑：`C:/Vivado/fft_loopback`
- BD 名稱：`bd_loopback`（wrapper: `bd_loopback_wrapper`）
- TB：`fft_loopback.srcs/sim_1/new/tb_fft_loopback.v`
- 模擬輸出：`fft_loopback.sim/sim_1/behav/xsim/rx_full_system_output.txt`
- 測試：送 3 封包（274B 8PSK × 2、188B QPSK × 1）

## OFDM PHY + Viterbi + Interleaver64 Loopback（不含 RS / energy dispersal）
- 路徑：`C:/Vivado/test_loopback`
- BD 名稱：`bd_ip`（wrapper: `bd_ip_wrapper`）
- TB：`test_loopback.srcs/sim_1/new/tb_ip.v`
- **鏈路（2026-04-24 更新）**：
  `input(8-bit) → axi_viterbi_encode_w_0 → tx_interleaver64_dyn_0 → symbol_mapper_hls_0 → tx_interleaver_float_0 → reference_signal_tx_0 → smart_ifft_0 → smart_FFT_0 → ref_rx_packed_0 → rx_deinterleaver_flo_0 → p8psk_demap_packed_0 → rx_deinterleaver64_d_0 → Viterbi_decode_0 → output(8-bit)`
- **wrapper 腳位（2026-04-24）**：
  - Input：`input_stream_0_tdata/tvalid/tready/tlast`（無 tkeep/tstrb）
  - Output：`outStream_0_tdata/tkeep/tstrb/tvalid/tready/tlast`（前綴 outStream，非 output_stream）
- TB 送 **306 bytes**（8 bits 全有效，對應 8PSK 一個 RS frame），輸出 306 decoded bytes
- Viterbi rate 1/2：306→612 coded bytes → 1632 symbols × 3 bits
- BER check：`check_ber()` task，tx_buf vs rx_buf bit-level 比對
- 等待時間：`#5000000`（Viterbi latency 較大）
- 舊版（PHY only，4896 bytes LSB-only）已廢棄，已被此版取代
- ✅ 2026-04-22：舊版 PHY loopback 完全正確（0/4896 mismatch）
- ✅ **2026-04-24：Viterbi + interleaver64 loopback sim 驗證完成，BER = 0/2448**
  - LLR convention 確認：TX bit=0 → LLR=+7(0x07)；TX bit=1 → LLR=-7(0xF9)
- **TB 新增（2026-04-23）**：`tb_ip.v` 加入 ref_rx 輸入/輸出 CSV 監控
  - `ref_rx_input.csv`：FFT 輸出 fixed-point（index/I_dec/Q_dec）
  - `ref_rx_output.csv`：等化後 FP16（index/I_float/Q_float，fp16_to_real Verilog function）

## test_Brian 專案（硬體燒錄 XSA 主專案）
- 路徑：`C:/Vivado/test_Brian`
- BD（2026-04-24 更新）：Viterbi + interleaver64 完整鏈路
  - `DMA MM2S → axi_viterbi_encode_w_0 → tx_interleaver64_dyn_1 → symbol_mapper_hls_0 → tx_interleaver_float_0 → reference_signal_tx_0 → smart_ifft_0 → smart_FFT_0 → ref_rx_packed_0 → rx_deinterleaver_flo_0 → p8psk_demap_packed_0 → rx_deinterleaver64_d_0 → Viterbi_decode_0 → DMA S2MM`
- DMA 設定：MM2S=8-bit TX，S2MM=8-bit RX（Viterbi decode 輸出 8-bit）
- main.c：`C:/Vitis/test_loop/app_loop/src/main.c`
  - TX_LEN=306（full 8-bit，pattern: `(i*37+13)&0xFF`）
  - RX_LEN=306（Viterbi decoded bytes）
  - BER 比對：TX vs RX bit-level，print first 16 bytes sanity check
- 舊功能（ref_rx FP16 IQ 輸出，4896 LSB-only）已廢棄
- ✅ 2026-04-23 舊版：星座點 ±0.92/±0.38 符合 8PSK 理論值（歷史記錄）
- ✅ **2026-04-25 Timing 責任釐清完成**：
  - 無 Viterbi/interleaver64 → WNS = **+2.663 ns**（Failing Endpoints = 0），100 MHz timing OK
  - 加舊 Viterbi/interleaver64 → WNS = **-52.832 ns**（Failing Endpoints = 3352），timing 爆掉
  - Report 存檔：`C:/Vivado/test_Brian/timing_WITHOUT_viterbi_WNS_pos2633.rpt`
  - 結論：舊 Viterbi IP 有 timing 問題
- ✅ **2026-04-28：新版 Viterbi_decode IP（C:\OFDM\Viterbi_decode）timing 完全通過**
  - solo_test（enc→mapper→demap→dec）WNS = **+0.405 ns**，Failing Endpoints = 0，100 MHz OK
  - **test_Brian**（完整鏈路）換入新 IP 後：WNS = **+1.068 ns**，WHS = +0.009 ns，WPWS = +3.500 ns，Failing Endpoints = 0，100 MHz OK
  - 硬體測試（test_Brian 含新 IP）：BER = 0/2448，PASS
  - 新 IP 由他人更新，來源 C:\OFDM\Viterbi_decode（Vitis HLS 2024.2 合成，K=7，64 state）
  - ✅ test_Brian timing 問題已解決，完整鏈路 100 MHz clean

## ✅ 全模式驗證完成（2026-05-06 確認）

| 模式 | 單封包 Sim | 單封包 HW | 多封包 Sim | 多封包 HW |
|------|-----------|-----------|-----------|-----------|
| 8PSK (274B) | ✅ | ✅ | ✅ | ✅ |
| QPSK (188B) | ✅ | ✅ | ✅ | ✅ |

- 所有測試 BER = 0，全部 PASS
- main.c 最後設定：NUM_PKTS=10，PKT_BYTES=274（8PSK）
- tb_ip.v 最後設定：PKT_BYTES=188，NUM_PKTS=1（QPSK）
- ✅ **2026-04-28：test_Brian 加入 RS enc/dec + interleaver_dynam/deinterleaver_dyn**
  - 完整鏈路：RS enc → tx_interleaver_dynam → Viterbi enc → OFDM → Viterbi dec → rx_deinterleaver_dyn → RS dec
  - TX/RX = **274 bytes**（8PSK raw data）
  - 硬體驗證：BER = 0/2192，PASS
  - main.c：PKT_BYTES=274，全部 274 bytes 輸出
  - test_loopback TB：PKT_BYTES=274，wrapper 腳位更新（output_stream_0_* + tkeep/tstrb）
- ✅ **2026-04-28：加入 energy_dispersal + energy_descramble，完整系統驗證完畢**
  - 完整鏈路：energy_dispersal → RS enc → tx_interleaver_dynam → Viterbi enc → OFDM → Viterbi dec → rx_deinterleaver_dyn → RS dec → energy_descramble
  - HW BER = 0/2192，Sim BER = 0/2192，硬體與 sim 完全一致
  - **8PSK OFDM PHY 全鏈路（所有 IP）驗證完成**

## ILA 工作流程（Zynq UltraScale+）
- ILA 需要 PS 時鐘啟動後才可偵測，Vitis Launch Hardware 會 reset PL 導致 bitstream 消失
- **正確流程**：Vivado HW Manager 燒 bitstream → XSCT 啟動 PS → HW Manager Refresh
- XSCT 指令（只 reset A53，不動 PL）：
  ```tcl
  connect
  targets -set -nocase -filter {name =~ "*A53*#0"}
  rst -processor
  dow {C:/Vitis/test_loop/app_loop/Debug/app_loop.elf}
  con
  ```
- probe 名稱：`bd_test_i/system_ila_0/inst/net_slot_0_axis_tvalid`（slot0=FFT輸出，slot1=ref_rx輸出）
- 觸發設定：`set_property TRIGGER_COMPARE_VALUE eq1'b1 [get_hw_probes ...tvalid -of_objects [get_hw_ilas hw_ila_1]]`
- **必須先 arm（run_hw_ila），再跑 main.c**
- 匯出：`write_hw_ila_data -force -csv_file {...} hw_ila_data_1`

## ✅ DC bin (pilot[136]) bug — 已完全解決（2026-04-22）
- 根因：Xilinx FFT IP DC bin 污染，H_est[136] 符號反轉，造成 5 個固定 bit error
- **修復方式（僅改 RX）**：CFO scan skip p=136；Loop_Process_Packed i==136 skip，i==135 跨接 pilot[137]，14-tone span，k=7 跳過不輸出
- **驗證結果：Sim BER=0/4896，HW BER=0/4896，完全解決**
- 分析文件（歷史記錄，勿重新提出）：`C:/VitisProject/Vitis/OFDM/RX/Refer/review/dc_pilot_bug_report.md`

**Why:** 完整理解系統後可快速 debug，避免每次重新分析架構。
**How to apply:** 遇到 RX 鏈路問題，先對照此表確認哪個 IP 的輸入輸出數量出問題。

## Fragmentation / Defragmentation IP（2026-05-07，Vivado BD 整合中）

### 規劃鏈路
- TX：`[UDP input] → tx_frag → energy_dispersal → RS enc → ...`
- RX：`... → energy_descramble → rx_defragment → [UDP output]`

### tx_frag（新版，`C:\OFDM\TX\tx_fragmentation\tx_frag.cpp`）
- csim 4/4 PASS（2026-05-07）
- **✅ TLAST 問題已解決**：每個 symbol 最後一個 byte 都拉高 TLAST
- 輸入：任意大小 UDP packet（最大 4096B），AXI-Stream 8-bit
- 輸出：N × 274B（mode=1/8PSK）或 N × 188B（mode=0/QPSK），每個 symbol 帶 9-byte header，各自 TLAST
- 有效 payload per symbol：265B（8PSK）或 179B（QPSK）
- Header 格式（9B）：`[0x47][pkt_idx][size_hi][size_lo][total_sym_cnt][sym_idx][len_lo][len_hi][pkt_idx]`
- 介面：`s_axilite port=return bundle=control`（ap_ctrl_hs，需寫 0x01 啟動）、`mode` 也是 s_axilite

### rx_defragment（`C:\OFDM\RX\rx_defragmentation2\rx_defragment.cpp`）
- csim 2/2 PASS，硬體驗證 BER=0 ✅，已部署進 test_Brian
- 輸入：N 個 symbol（各有獨立 TLAST），自動從 header 判斷模式
- 輸出：重組後的 UDP payload
- `ap_ctrl_none`，free-running，不需要 AXI-Lite 控制
- ✅ **single-symbol bug 已修（rx_defragmentation2）**：8PSK 200B payload（num_syms=1），3/3 PASS，BER=0（2026-05-08 硬體驗證）

### Frame Payload 上限由 RS 決定（不可更改）
| 模式 | Frame 總大小 | Header | 最大 Payload | 原因 |
|------|------------|--------|-------------|------|
| QPSK | 188B | 9B | **179B** | RS(204,188) 輸入塊大小 = 188B |
| 8PSK | 274B | 9B | **265B** | 2×RS(153,137) 輸入塊大小 = 274B |

### Header 對 CPU/DMA 完全透明
- tx_frag **自動加入** header（CPU 只送 raw UDP payload）
- rx_defragment **自動剝除** header（CPU 只收 raw UDP payload）
- UART 印出的 idx 000~499 全部是純 payload bytes，不含 header

### Padding 行為（最後一個 fragment）
- 若最後一個 fragment payload < 最大 payload，剩餘空間填 **0x00**（tx_frag source line 128 確認）
- rx_defragment 從 header [len_lo][len_hi] 讀取有效長度，不輸出 padding bytes
- 例：500B 8PSK → frag1 有 235B payload + 30B padding(0x00)，CPU 端只收到 235B

### Vivado BD 整合狀態（2026-05-07 完成）
- ✅ QPSK chain（qpsk_demap_packed_0）：BER=0/12000，3pkts×500B，PASS
- ✅ 8PSK chain（p8psk_demap_packed_0）：BER=0/12000，3pkts×500B，PASS
- ✅ main.c 已加入 tx_frag AXI-Lite 控制（TX_FRAG_MODE, base=0xA0010000）
- ✅ XSA 存檔：`C:\VitisProject\xsa_archive\QPSK\`（8PSK XSA 待存）
- 切換模式：只需換 .bit 檔（Program FPGA），elf 不用重 build（IP 位址相同）
