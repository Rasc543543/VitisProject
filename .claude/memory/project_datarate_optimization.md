---
name: Data Rate 優化計畫
description: QPSK 1.5 Mbps → 2 Mbps 目標，Viterbi decoder 為主要瓶頸（66% frame time）
type: project
originSessionId: b9a6cf40-630b-4335-b69d-3a275d982961
---
## 目標
- QPSK 目前：1.502 Mbps（1001 µs/frame，188B QPSK）
- 8PSK 目前：2.000 Mbps（已達標）
- 目標：QPSK 也達到 ≥ 2 Mbps → frame time 需從 1001 µs 降至 ≤ 752 µs（需減少 ~250 µs）

## 瓶頸分析（2026-05-07）

| IP | Cycles (max) | 時間 @100 MHz | 佔比 |
|----|------------|--------------|------|
| **Viterbi_decode_0** | 65,633 | **~657 µs** | **66%** |
| RS decoder (rs_decoder_rx_0) | SYNDROME: 2451~3267 | ~25~33 µs | ~3% |
| RS encoder (rs_encoder_tx_0) | QPSK_DATA: 190 | ~1.9 µs | <1% |
| 其他（FFT/IFFT/interleaver）| — | ~310 µs | ~31% |

## Viterbi Decoder 關鍵數字
- 路徑：`C:\OFDM\Viterbi_decode`（Vitis HLS 2024.2，他人開發）
- DECODE_LOOP Interval：min=97, max=65,633 cycles
- HLS 估計 critical path：82.486 ns（悲觀估計，硬體實測 WNS=+1.068 ns 通過）
- 資源：LUT 71,155（17%），FF 61,238（7%）— 已很大
- 100 MHz 合成

## RS Decoder 關鍵數字（2026-05-08 完整分析）
- 路徑：`C:\VitisProject\Vitis\OFDM\RX\RS\rs_decoder_rx`（100 MHz）
- 各階段（QPSK RS(204,188)，無錯誤情境）：
  | 階段 | Min cycles | Max cycles |
  |------|-----------|-----------|
  | COLLECT_LOOP | ~206 | ~206 |
  | SYNDROME_LOOP | 2,451 | 3,267 |
  | BM_LOOP | 608 | 1,504 |
  | OMEGA_LOOP | 112 | 624 |
  | CHIEN_LOOP | ~3,825 | ~30,600 |
  | OUTPUT_LOOP | 142 | 381 |
- **總計（無錯誤）：~7,344 cycles = ~73 µs**
- **總計（8 errors）：~36,582 cycles = ~366 µs**
- CHIEN_LOOP 是 RS 內部瓶頸（255 iter × 15~120 cycles，無 pipeline）
- 佔整體 frame time 約 7%（遠小於 Viterbi 的 66%）
- 結論：RS decoder 不是主要瓶頸，但實際比之前記錄的 25~33 µs 大（之前只算了 SYNDROME）

## RS Encoder 關鍵數字
- 報告路徑：`C:\OFDM\reed_solomon_22\solution1\syn\report\rs_encoder_tx_csynth.rpt`
- QPSK_DATA：190 cycles，QPSK_PARITY：18 cycles → 合計 ~208 cycles = 2.08 µs @100 MHz
- 結論：非瓶頸

## 全 IP Synthesis Report 整理（2026-05-08）

所有報告路徑皆可在 `C:\OFDM\` 或 `C:\VitisProject\Vitis\OFDM\` 下找到。

| IP | 時鐘 | Cycles | 時間 | 備註 |
|----|------|--------|------|------|
| **ref_rx_packed** (VitisProject 版) | 100 MHz | 45,464~66,680 | **455~667 µs** | Loop_CFO_Scan 佔 428 µs，疑似只跑一次 |
| **Viterbi_decode** | 100 MHz | ~65,633 | **~657 µs** | 主瓶頸，DECODE_LOOP trip count 決定 |
| qpsk_demap_packed | 100 MHz | ~9,826 | **~98 µs** | recv_data+calc_snr+process_output |
| tx_interleaver64 | 100 MHz | 6,536~9,800 | **65~98 µs** | fill+drain buffer |
| rx_deinterleaver64 | 100 MHz | 6,539~9,803 | **65~98 µs** | fill+drain buffer |
| rs_decoder_rx | 100 MHz | ~7,344 | **~73 µs** | 無錯誤情境（見RS Decoder詳細分析） |
| tx_interleaver_dyn | ~100 MHz | ~620 | **~6.2 µs** | clock 設定異常（10 ms 單位），實際小 |
| rx_deinterleaver_dyn | 100 MHz | ~623 | **~6.2 µs** | |
| reference_signal_tx | 250 MHz | 2,052 | **~8.2 µs** | |
| symbol_mapper | 250 MHz | 1,671 | **~6.7 µs** | |
| energy_dispersal / descramble | 250 MHz | streaming | **<2 µs** | |
| FFT / IFFT / viterbi_enc | RTL | 不明 | — | 無 HLS report |

## ref_rx_packed CFO_Scan 分析（2026-05-10 確認）
- **IP 狀態：已完成驗證（csim PASS、cosim PASS、Vivado loopback BER=0）**
- DC pilot bug 已修正（DC bypass，BER=0）
- **Loop_CFO_Scan**：141 iter × 304 cycles = 42,864 cycles = **428 µs**（佔 ref_rx 的 94%）
- **確認**：`best_cfo` 是 local variable（非 static），每幀都重跑完整掃描 → 每幀浪費 428 µs
- CFO 本質是硬體振盪器頻率差，幀間不變 → 重掃結果相同，是純粹的冗餘計算
- 目前 ZCU208 loopback：TX/RX 同一 FPGA，CFO = 0，best_cfo 永遠相同

## CFO Cache 優化方案（已設計，待實作）
- **目標**：省去第 2 幀起每幀 428 µs 的重複掃描
- **預期效果**：QPSK frame time 1001 → ~573 µs（實際需上板確認，Viterbi 657 µs 可能是新瓶頸）
- **採用方案：Tracking Mode（衛星 Doppler 適用）**
- 完整設計文件：`C:\VitisProject\design_notes\cfo_cache_design.md`
- PPT 比較圖：`C:\VitisProject\design_notes\cfo_versions_compare.html`

### 四版比較（2026-05-11 確認定案）

| 版本 | 穩定代價 | Doppler | 衛星適用 | 結論 |
|------|---------|---------|---------|------|
| 原始 | 428 µs | 428 µs | ❌ | 現行 |
| Simple Cache | ~0 µs | 錯誤 | ❌ | 不實作 |
| Neighbor Check | ~9 µs | 428 µs（重掃）| ❌ | 不實作 |
| **Tracking Mode** | ~9 µs | ~9 µs | ✅ | **採用** |

Doppler 特性：漸進漂移（LEO 每幀最多移 1 subcarrier），Tracking Mode 永遠追得上。

```cpp
// Tracking Mode 核心邏輯（第 2+ 幀只看 3 個點）
static int  best_cfo    = 0;
static bool initialized = false;

if (!initialized) {
    /* 第一幀：完整 141 次掃描 */
    initialized = true;
} else {
    half e_left  = measure_energy(buf_i, buf_q, best_cfo - 1);
    half e_cur   = measure_energy(buf_i, buf_q, best_cfo);
    half e_right = measure_energy(buf_i, buf_q, best_cfo + 1);
    if      (e_left > e_cur && e_left > e_right)  best_cfo -= 1;
    else if (e_right > e_cur && e_right > e_left) best_cfo += 1;
}
int base = LEFT_GUARD + best_cfo;
```

### SAL 能量計算（同步實作）
- 能量計算改絕對值：`vi*vi + vq*vq` → `|vi| + |vq|`
- argmax 結果不變（L1/L2 norm 最大值位置相同）
- 移除 38,493 次浮點乘法，減少 DSP48
- 注意：`vi` 是 `half`（浮點），SAL 省的是乘法，加法 DSP48 仍在
- 若改成對 int16_t 原始值做 SAL，效益更大（待評估）

## CFO 相關學術資源（2026-05-10 已完成 NotebookLM 論文探查）

NotebookLM notebook：「Zadoff-Chu Synchronization for Short-Burst OFDM Signals」（21 篇論文）
完整分析文件：`C:\VitisProject\design_notes\cfo_papers_deep_dive.html`

### 五篇精選論文及其可用技術

| 論文 | 可直接用的技術 | 當前優先度 |
|------|--------------|---------|
| **Classen CFO Synchronization**（review） | CFO cache 的學術名稱（capture+tracking two-mode），可引用 | 立即 |
| **FFT-Based ICFO（802.15.4g）** | FFT cross-correlator 取代 141 次 exhaustive scan，速度 28.4×，但需 LTF training symbol | 中期（需改 frame 結構）|
| **SAL Algorithm（鄰近 subcarrier 相似性）** | energy 計算改 `abs(Re)+abs(Im)` 取代平方，省 DSP；LUT -90%, FF -94% | 立即（單行改動）|
| **Decision Feedback Estimator** | data+pilot 聯合消除 ICI error floor（SNR>14dB 時用）| RF 部署後 |
| **Easy Hardware LS（802.11a）** | LS fractional CFO shift-add 近似，DSP -75%，但只處理 residual fractional CFO | RF 部署後 |

### 可立即實作的 SAL 改動
```cpp
// Loop_CFO_Scan 能量計算：平方 → 絕對值（argmax 結果完全相同）
// 舊：energy += sq_i * sq_i + sq_q * sq_q;
// 新：
energy += (sq_i < 0 ? -sq_i : sq_i) + (sq_q < 0 ? -sq_q : sq_q);
// 效益：省掉 38,493 次乘法，HLS 不推 DSP48
```

## Viterbi Decoder 原始碼狀態
- 原始碼在 `D:/FPGA/jameslin/viterbi_decode/Viterbi.cpp`（他人機器，非本機）
- 本機只有 preprocessed + compiled 產物（`C:\OFDM\RX\viterbi_decode\`）
- DECODE_LOOP 已有 `#pragma HLS PIPELINE II=1`，瓶頸是 trip count（loop 次數）
- trip count ≈ 48 + actual_symbol_count + 48，受 interleaver 深度決定

## CFO Tracking Mode 設計（2026-05-12 完成 csim）

### 狀態機
- **Unlocked**：第一幀全掃 141 點，鎖定後進入 Locked
- **Locked**：只掃 ±5（11 點），boundary_hit（best 在邊界）→ 下一幀重掃
- `static cfo_state`、`static cfo_locked` 跨幀保存狀態

### csim 結果（對比 baseline）
- 5 dB 以上：AvgErr 完全一致，best_cfo 相同
- 0 dB：新版 best_cfo=70 ✅（舊版 49/63 ❌），locked 縮小搜尋範圍反而更穩
- 實作路徑：`C:\VitisProject\Vitis\OFDM\RX\Refer\ref_rx_packed.cpp`
- Baseline 數據存檔：`review/baseline_snr_test.md`

### 預期效益
- Locked 時：42,864 cycles → ~3,344 cycles（11/141 × 42,864）
- Frame time：~1001 µs → ~605 µs（待 synthesis + 上板確認）

### Pilot/Data 能量比分析
- Pilot 振幅 4/3，Data QPSK 0.707，比值 1.78×
- Lost 門檻若用 energy_drop：需 > 0.56，設 0.65 勉強但不可靠
- **結論：只用 boundary_hit，不用 energy_drop**（channel fading 會誤觸發）

## CFO Tracking Mode 完整驗證結果（2026-05-12）

| 階段 | 結果 |
|------|------|
| csim（SNR sweep 14 cases）| PASS ✅ |
| synthesis timing | PASS ✅（8.419 ns < 10 ns）|
| cosim（2 幀）| PASS ✅ |
| IP export | ✅ `C:\VitisProject\Vitis\OFDM\RX\Refer\output\export.zip` |

**Synthesis Loop 對照：**
- Loop_CFO_Scan（Unlocked）：42,864 cycles（不變）
- Loop_CFO_Track（Locked）：22 ~ 3,344 cycles（**12.8× 加速**）
- 總 latency：2,622 ~ 66,680 cycles

設計文檔：`C:\VitisProject\design_notes\cfo_tracking_mode.md`

## Vivado Simulation 驗證結果（2026-05-13，5 幀 QPSK）

| 幀 | Frame Time | 模式 |
|----|-----------|------|
| 1 | ~1001 µs | Unlocked（全掃 141 點）|
| 2–5 | **~611 µs** | Locked（±5，11 點）|

- BER = 0/7520 bits，全部 PASS ✅
- Locked 穩態 Data Rate：188B × 8 / 611 µs = **2.46 Mbps**（超過 2 Mbps 目標 ✅）
- 預測 605 µs vs 實測 611 µs，誤差 < 1%
- 首幀 1001 µs 正常（Unlocked 全掃，與舊版相同）
- Simulation 路徑：`C:\Vivado\test_loopback`，TB = `tb_ip.v`（NUM_PKTS=5，QPSK 188B）

## 硬體實測結果（2026-05-13）

### 8PSK（265B payload × 3 pkts）
| 封包 | Frame Time | Data Rate | 模式 |
|------|-----------|-----------|------|
| pkt[0] | 1107 µs | 1.91 Mbps | Unlocked |
| pkt[1-2] | **717 µs** | **2.95 Mbps** | Locked |
- BER = 0/6360 bits ✅，平均吞吐量 2.50 Mbps

### QPSK（179B payload × 3 pkts）
| 封包 | Frame Time | Data Rate | 模式 |
|------|-----------|-----------|------|
| pkt[0] | 1009 µs | 1.41 Mbps | Unlocked |
| pkt[1-2] | **620 µs** | **2.30 Mbps** | Locked |
- BER = 0/4296 bits ✅，平均吞吐量 1.91 Mbps

### Simulation vs Hardware 對比
| 模式 | Sim Frame Time | HW Frame Time | 誤差 |
|------|---------------|--------------|------|
| QPSK Locked | ~611 µs | **620 µs** | ~1.5% |
| 8PSK Locked | ~706.55 µs | **717 µs** | ~1.5% |

**結論：兩個模式 Locked 穩態均超過 2 Mbps 目標 ✅**

## 下一步（2026-05-13 最終更新）
1. ✅ ~~換 IP 進 Vivado 重跑 bitstream~~（已完成）
2. ✅ ~~上板實測 8PSK~~：Locked 2.95 Mbps，BER=0 ✅
3. ✅ ~~上板實測 QPSK~~：Locked 2.30 Mbps，BER=0 ✅（620 µs/frame）
4. ✅ ~~討論文件~~：`design_notes/cfo_tracking_discussion.html`（含流程圖、假設、擔憂、完整結果）

## 待驗證項目（開會後確認優先度）
- ⚠ argmax-at-edge 假設：csim 注入 CFO > ±5，驗證 drift 是否落在視窗邊緣
- ⚠ Doppler 計算：需取得載波頻率 + subcarrier spacing → 驗算 CFO_TRACK_W=5 是否足夠
- Viterbi 優化：需取得 jameslin 原始碼（他人機器），或縮短 interleaver 深度

**Why:** QPSK Data Rate 需提升以達到系統規格，衛星通訊需能追蹤 Doppler
**How to apply:** CFO Tracking Mode 已全部完成（csim/cosim/Vivado sim/HW 全 PASS），兩模式均超 2 Mbps 目標
