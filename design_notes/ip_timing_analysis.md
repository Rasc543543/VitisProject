# OFDM 系統 IP 時序分析報告

**建立日期**：2026-05-08  
**分析對象**：test_Brian 完整鏈路（QPSK 模式，188B/frame，100 MHz 主時脈）  
**量測基準**：QPSK frame time = 1001 µs（DMA TX 啟動 → DMA RX 完成）

---

## 一、系統鏈路

```
DMA TX
  → energy_dispersal_0
  → rs_encoder_tx_0
  → tx_interleaver_dynam_0
  → axi_viterbi_encode_w_0    ← RTL IP
  → tx_interleaver64_dyn_1
  → symbol_mapper_hls_0
  → tx_interleaver_float_0    ← RTL IP
  → reference_signal_tx_0
  → smart_ifft_0              ← RTL IP
  → smart_FFT_0               ← RTL IP
  → ref_rx_packed_0
  → rx_deinterleaver_flo_0    ← RTL IP
  → qpsk_demap_packed_0 / p8psk_demap_packed_0
  → rx_deinterleaver64_d_0
  → Viterbi_decode_0
  → rx_deinterleaver_dyn_0
  → rs_decoder_rx_0
  → energy_descramble_0
  → rx_defragment_0
DMA RX
```

---

## 二、全 IP Synthesis Report 整理

### 2.1 100 MHz HLS IP

| IP（BD 名稱）| HLS 報告路徑 | Cycles | 時間 @100 MHz | 報告日期 |
|-------------|-------------|--------|--------------|---------|
| **ref_rx_packed_0** | `C:\VitisProject\Vitis\OFDM\RX\Refer\ref_rx_fixed\solution1\syn\report\ref_rx_packed_csynth.rpt` | 45,464~66,680 | **455~667 µs** | 2026-04-22 |
| **Viterbi_decode_0** | `C:\OFDM\RX\viterbi_decode\viterbi_decode\hls\syn\report\Viterbi_decode_csynth.rpt` | ~65,633 (max) | **~657 µs** | 2026-02-04 |
| qpsk_demap_packed_0 | `C:\OFDM\RX\qpsk_demap\solution1\syn\report\qpsk_demap_packed_csynth.rpt` | ~9,826 | **~98 µs** | 2026-03-28 |
| tx_interleaver64_dyn_1 | `C:\OFDM\TX\tx_interleaver64_dynamic\tx_interleaver64_dynamic\hls\syn\report\tx_interleaver64_dynamic_csynth.rpt` | 6,536~9,800 | **65~98 µs** | 2026-03-15 |
| rx_deinterleaver64_d_0 | `C:\OFDM\RX\rx_deinterleaver64_dynamic\rx_deinterleaver64_dynamic\hls\syn\report\rx_deinterleaver64_dynamic_csynth.rpt` | 6,539~9,803 | **65~98 µs** | 2026-03-15 |
| rs_decoder_rx_0 | `C:\VitisProject\Vitis\OFDM\RX\RS\rs_decoder_rx\solution1\syn\report\rs_decoder_rx_csynth.rpt` | ~7,344 | **~73 µs** | 2026-04-08 |
| rx_deinterleaver_dyn_0 | `C:\OFDM\RX\rx_deinterleaver_dynamic\rx_deinterleaver_dynamic\hls\syn\report\rx_deinterleaver_dynamic_csynth.rpt` | 419~623 | **~6.2 µs** | 2026-03-15 |
| rs_encoder_tx_0 | `C:\OFDM\reed_solomon_22\solution1\syn\report\rs_encoder_tx_csynth.rpt` | ~208 | **~2.1 µs** | — |

### 2.2 250 MHz HLS IP

| IP（BD 名稱）| HLS 報告路徑 | Cycles | 時間 @250 MHz |
|-------------|-------------|--------|--------------|
| symbol_mapper_hls_0 | `C:\OFDM\TX\Mapper_32bit\Mapper_32bit\symbol_mapper_hls\hls\syn\report\symbol_mapper_hls_csynth.rpt` | 1,671 | **~6.7 µs** |
| reference_signal_tx_0 | `C:\OFDM\TX\Refer\packed\ref\reference_signal_tx_packed\hls\syn\report\reference_signal_tx_packed_csynth.rpt` | 2,052 | **~8.2 µs** |
| energy_dispersal_0 | `C:\OFDM\TX\energy_dispersal_1byte\energy_dispersal\energy_dispersal\hls\syn\report\energy_dispersal_csynth.rpt` | streaming | **<2 µs** |
| energy_descramble_0 | `C:\OFDM\RX\RX_energy_descramble_1byte\energy_descramble\energy_descramble\hls\syn\report\energy_descramble_csynth.rpt` | streaming | **<2 µs** |

> ⚠️ **tx_interleaver_dynam_0**：ClockPeriod 設定異常（10,000,000，疑似 ps 單位 = 10 ms），報告中 Latency absolute 顯示 "秒" 單位為錯誤。實際時間估算：420~620 cycles × 10 ns = **~4.2~6.2 µs**。

### 2.3 RTL IP（無 HLS report）

| IP（BD 名稱）| 路徑 | 備註 |
|-------------|------|------|
| axi_viterbi_encode_w_0 | `C:\OFDM\TX\viterbi` | TX Viterbi encoder，RTL |
| tx_interleaver_float_0 | `C:\OFDM\TX\tx_interleaver_floatIQ` | Float domain interleaver，RTL |
| rx_deinterleaver_flo_0 | `C:\OFDM\RX\rx_deinterleaver_floatIQ` | Float domain deinterleaver，RTL |
| smart_ifft_0 | `C:\OFDM\TX\smart_ifft` | IFFT，RTL |
| smart_FFT_0 | `C:\OFDM\RX\smart_fft` | FFT，RTL |

---

## 三、各 IP 內部結構詳解

### 3.1 ref_rx_packed（最大不確定因素）

| 子階段 | Cycles | 時間 |
|--------|--------|------|
| Load_Packed | 2,050 | 20.5 µs |
| Pipeline_2 | 6 | 0.06 µs |
| Loop_Metric | 287 | 2.87 µs |
| Loop_Interp | 41~62 | 0.41~0.62 µs |
| **Loop_CFO_Scan** | **42,864** | **428.64 µs（141 iter × 304 cycles）** |
| Loop_Process_Packed | 544~21,760 | 5.44~217.6 µs（272 iter × 2~80 cycles）|
| **總計** | **45,464~66,680** | **455~667 µs** |

**關鍵問題**：Loop_CFO_Scan 是否每幀都執行？

- 若每幀都跑：ref_rx + Viterbi = 455 + 657 = 1112 µs > 實測 1001 µs，**矛盾**
- 若只跑第一幀（static 變數 cache CFO 結果）：穩態 ref_rx ≈ **29~241 µs**，與實測吻合
- **待確認**：量測第一幀 vs 後續幀的時間差

### 3.2 Viterbi_decode（主瓶頸）

| 子階段 | Cycles | 時間 |
|--------|--------|------|
| READ_LOOP | ? (input-dependent) | — |
| INIT_DIST | 66 | 0.66 µs |
| **DECODE_LOOP** | **97~65,633** | **0.97~656 µs** |
| Output | 含在 DECODE_LOOP 中 | — |

- DECODE_LOOP 已有 `#pragma HLS PIPELINE II=1`，每 cycle 推進一步
- `total_steps = 48 + actual_symbol_count + 48`，actual_symbol_count 由上游 interleaver 深度決定（約 40 OFDM symbols ≈ 65,500 pairs）
- 原始碼位於 `D:/FPGA/jameslin/viterbi_decode/Viterbi.cpp`（**非本機**），無法直接修改
- HLS critical path：82.486 ns（HLS 悲觀估計），硬體實測 WNS = +1.068 ns 通過

**資源消耗**：LUT 71,155（17%），FF 61,238（7%）

### 3.3 qpsk_demap_packed（第三大）

| 子階段 | Cycles | 時間 |
|--------|--------|------|
| recv_data | 1,634 | 16.34 µs |
| **calc_snr** | **4,913** | **49.13 µs** |
| process_output | 3,279 | 32.79 µs |
| **總計** | **~9,826** | **~98 µs** |

calc_snr 佔 50%，可能有優化空間。

### 3.4 tx/rx_interleaver64（各 65~98 µs）

- 結構：FILL_LOOP（讀入）+ DRAIN_LOOP（輸出），不可重疊
- QPSK：3,265~3,266 cycles per loop = 32.65 µs × 2 = **65.3 µs**
- 8PSK：4,897~4,898 cycles per loop = 48.97 µs × 2 = **97.9 µs**
- TX 和 RX 各一個，合計 **130~196 µs**

### 3.5 RS Decoder（詳細分解）

| 子階段 | Min cycles | Max cycles |
|--------|-----------|-----------|
| COLLECT_LOOP | ~206 | ~206 |
| SYNDROME_LOOP | 2,451 | 3,267 |
| BM_LOOP | 608 | 1,504 |
| OMEGA_LOOP | 112 | 624 |
| CHIEN_LOOP | ~3,825 | ~30,600 |
| OUTPUT_LOOP | 142 | 381 |
| **總計** | **~7,344** | **~36,582** |

- 無錯誤時：~73 µs；8 errors 最壞：~366 µs
- CHIEN_LOOP 是 RS 內部瓶頸（255 iter，無 pipeline）

---

## 四、時間預算分析（QPSK，穩態）

| 類別 | 時間 | 說明 |
|------|------|------|
| Viterbi decode | ~657 µs | 66%，最大瓶頸 |
| ref_rx (穩態，估計) | ~29~241 µs | CFO_Scan 是否每幀待確認 |
| qpsk_demap | ~98 µs | ~10% |
| tx_interleaver64 | ~65 µs | QPSK 模式 |
| rx_deinterleaver64 | ~65 µs | QPSK 模式 |
| RS decoder | ~73 µs | 無錯誤 |
| 其他（mapper/ref_tx/FFT等）| ~20 µs | 估算 |
| **實測總計** | **~1,001 µs** | **DMA TX→RX** |

目標：≤ 752 µs（QPSK 2 Mbps），需減少 **249 µs**。

---

## 五、優化可行性評估

| 優化方向 | 可節省時間 | 難度 | 備註 |
|---------|---------|------|------|
| 縮短 interleaver 深度（影響 Viterbi trip count）| 估計 ~150~250 µs | 中 | 需改 TX/RX interleaver IP，可能影響 coding gain |
| 確認並優化 ref_rx CFO_Scan | 0~428 µs | 低~高 | 先確認是否每幀都跑 |
| qpsk_demap calc_snr 優化 | ~49 µs | 中 | 需讀 demap 原始碼 |
| Viterbi 原始碼優化（需取得） | 大 | 高 | 需請 jameslin 提供 |

---

## 六、參考：IP Clock 設定

詳見 `C:\VitisProject\ip_clock_settings.md`
