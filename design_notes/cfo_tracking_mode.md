# CFO Tracking Mode 設計文檔

**日期：** 2026-05-12  
**狀態：** csim / cosim / synthesis 全部 PASS，IP 已 export  
**路徑：** `C:\VitisProject\Vitis\OFDM\RX\Refer\ref_rx_packed.cpp`

---

## 背景問題

`Loop_CFO_Scan` 每幀固定掃描 141 個 offset，佔 ref_rx_packed 執行時間的 94%：

```
141 offsets × 139 pilots × II=1 = ~42,864 cycles = 428 µs/幀
```

CFO（載波頻率偏移）本質是硬體振盪器頻率差，幀與幀之間幾乎不變（LEO 衛星最多漂 1 subcarrier/幀），因此每幀重掃是純粹的冗餘計算。

---

## 能量計算原理

CFO 掃描的核心：對每個 offset 計算所有 pilot 位置的接收能量總和：

```
E(offset) = Σ_{p=0, p≠136}^{138} |Y[LEFT_GUARD + offset + p×7]|²
```

- offset 正確 → 對到 pilot subcarrier → 能量大
- offset 錯誤 → 對到 data subcarrier → 能量小

**Pilot vs Data 能量比：**

| 類型 | 振幅 | 每 tone 能量 |
|------|------|------------|
| Pilot | 4/3 ≈ 1.333 | **1.778** |
| QPSK Data | 0.707+j0.707 | **1.0** |
| 比值 | | **1.78×** |

---

## 狀態機設計

```
[Unlocked] ──全掃141點找到峰→ [Locked]
[Locked]   ──掃±5點，boundary_hit→ [Unlocked]
```

### 狀態變數（static，跨幀保存）

```cpp
static int  cfo_state  = 0;    // 上一幀找到的最佳 offset
static bool cfo_locked = false; // false=Unlocked, true=Locked
```

### Unlocked 行為（第一幀 or 重掃）

完整 141 點掃描，與原始實作相同。鎖定後 `cfo_locked = true`。

### Locked 行為（第二幀起）

只掃 `cfo_state ± CFO_TRACK_W`（CFO_TRACK_W = 5，共 11 點）：

```cpp
for (int d = -CFO_TRACK_W; d <= CFO_TRACK_W; d++) {
    int offset = cfo_state + d;
    if (offset < -CFO_RANGE || offset > CFO_RANGE) continue; // 邊界保護
    // 計算 pilot 能量...
}
```

### Lost 判斷（boundary_hit）

```cpp
int drift = new_best - cfo_state;
if (drift >= CFO_TRACK_W || drift <= -CFO_TRACK_W)
    cfo_locked = false;  // 下一幀重掃
```

**只用 boundary_hit，不用 energy_drop 的原因：**
- Pilot/Data 能量比僅 1.78×，energy_drop 門檻難以設定（需 > 0.56 才能觸發）
- Channel 正常衰落會讓能量波動，energy_drop 容易誤觸發
- boundary_hit 不依賴能量門檻，直接偵測漂移速度，對 Doppler 追蹤更可靠

---

## Unlocked 搜尋加速方案（未實作，供未來參考）

Unlocked 狀態仍需跑 141 點全掃（428 µs）。以下方案可進一步加速，但因 Unlocked 在正常通訊中極少發生（僅第一幀 + 偶發重鎖），目前暫不實作。

### 方案 A：Coarse-Fine 搜尋

```
粗掃：每 10 步掃一次 → 15 個點
細掃：最佳粗掃點附近 ±10 → 21 個點
總計：36 次評估 → ~4× 加速
```

優點：HLS 友好（兩個 for loop），不需要 unimodal 假設  
缺點：若峰值附近不平滑，粗掃可能找到錯誤的細掃起點

### 方案 B：Golden Section Search

約 10 次評估找到最大值，理論加速 ~14×  
**前提：** E(offset) 必須是單峰（unimodal）  
**風險：** 多路徑或低 SNR 時可能有假峰 → 找到錯誤 offset

### 方案 C：Strided Sliding Window（推薦）

利用 pilot 間距為 7 的數學特性：

```
E(k) = Σ_{p≠136} |Y[LEFT_GUARD + k + p×7]|²
E(k+7) = E(k) - S[LEFT_GUARD + k] + S[LEFT_GUARD + k + 973]
```

演算法：
1. 預計算 `S[j] = |Y[j]|²`（約 1024 次乘法，一次性）
2. 將 141 個 offset 按 `k mod 7` 分成 7 組，每組約 20 個點
3. 每組第一個點暴力計算（139 次加法），之後用滑動視窗（2 次加法）

```
總計：~1024 + 7×(139 + 19×2) ≈ 2,300 次運算
暴力：141×139 = 19,600 次運算
加速：約 8.5×，結果完全等價，不需要任何假設
```

**代價：** 需要 BRAM 存放 S[] 陣列（2048 個 float）  
**優先度：** Unlocked 極少發生，實際 frame time 收益有限，暫緩

---

## 效能結果

### Synthesis Report（Vitis HLS 2022.2）

| Loop | Cycles | 時間 @100MHz |
|------|--------|------------|
| Loop_CFO_Scan（Unlocked）| 42,864 | 428.6 µs |
| **Loop_CFO_Track（Locked）**| **22 ~ 3,344** | **0.22 ~ 33.4 µs** |
| 加速比 | **12.8×** | |

- Timing：8.419 ns（目標 10 ns）✅
- 資源：DSP 24，LUT 5,093（1%），幾乎不變

### 總 Frame Latency

| 狀態 | Cycles | 時間 |
|------|--------|------|
| Unlocked（第一幀）| 45,464~66,674 | 455~667 µs |
| **Locked（一般幀）**| **~5,900~27,000** | **~59~270 µs** |

### csim SNR Sweep 結果

5 dB 以上：AvgErr 與原版完全一致（小數點後 6 位相同）  
0 dB：新版 best_cfo=70 ✅（原版被雜訊帶偏到 49/63），locked 縮小搜尋範圍反而更穩健

---

## 實作檔案清單

| 檔案 | 修改內容 |
|------|---------|
| `ref_rx_packed.cpp` | 加入 Tracking Mode 狀態機 |
| `tb_ref_rx_packed.cpp` | 加入 SNR sweep、8PSK 測試、noise generator |
| `review/baseline_snr_test.md` | 原版 baseline 數據存檔 |
| `output/export.zip` | 匯出的 HLS IP |

---

## 驗證結果（2026-05-13 全部完成）

**狀態：csim / cosim / synthesis / Vivado simulation / Hardware 全部 PASS ✅**

### Vivado Behavioral Simulation（test_loopback）

| 模式 | PKT_BYTES | NUM_PKTS | Frame Time（Locked） | Data Rate（Locked） |
|------|----------|---------|---------------------|---------------------|
| QPSK | 188B | 5 | ~611 µs | ~2.34 Mbps（179B payload） |
| 8PSK | 274B | 5 | ~706.55 µs | ~3.00 Mbps（265B payload） |

- BER = 0，全部 PASS
- Simulation 路徑：`C:\Vivado\test_loopback`，TB = `tb_ip.v`

### Hardware 實測（ZCU208 Loopback）

| 模式 | Payload | Frame Time（Unlocked） | Frame Time（Locked） | Data Rate（Locked） | BER |
|------|---------|----------------------|---------------------|---------------------|-----|
| QPSK | 179B | 1009 µs | **620 µs** | **2.30 Mbps** | 0 ✅ |
| 8PSK | 265B | 1107 µs | **717 µs** | **2.95 Mbps** | 0 ✅ |

- main.c 路徑：`C:\Vitis\test_loop\app_loop\src\main.c`
- 計時範圍：start_tx_frag → RX DMA 完成（不含 UART printf）

### Simulation vs Hardware 對比

| 模式 | Sim Frame Time（Locked） | HW Frame Time（Locked） | 誤差 |
|------|------------------------|------------------------|------|
| QPSK | ~611 µs | **620 µs** | ~1.5% |
| 8PSK | ~706.55 µs | **717 µs** | ~1.5% |

**兩個模式 Locked 穩態均超過 2 Mbps 目標 ✅**

### 舊版 vs 新版對比（QPSK）

| 版本 | Frame Time | Data Rate |
|------|-----------|-----------|
| 原版（每幀全掃） | ~1001 µs | 1.50 Mbps |
| **新版 Unlocked（第一幀）** | ~1009 µs | 1.41 Mbps |
| **新版 Locked（第二幀起）** | **~620 µs** | **2.30 Mbps** |

---

## Doppler 追蹤能力驗證（待確認）

目前設計假設「LEO 每幀最多漂移 1 subcarrier」，需以下數據驗證 ±5 視窗是否足夠：

| 參數 | 取得方式 | 數值 |
|------|---------|------|
| 1. 載波頻率（GHz） | RF 前端規格 / Vivado RF Data Converter IP | 待確認 |
| 2. LEO 最大徑向速度（km/s） | 物理常數，軌道速度 ≈ 7.5 km/s | ≈ 7.5 km/s |
| 3. OFDM subcarrier spacing（Hz） | `fs / FFT_size`，查 Vivado BD FFT IP | 待確認 |
| 4. Frame duration（ms） | 已量得：QPSK 620 µs，8PSK 717 µs（Locked） | ✅ 已知 |

**驗證公式：**
```
最大 Doppler shift  = f_carrier × v_radial_max / c
每幀 Doppler 漂移  = 最大 Doppler shift 的變化量 / 每秒幀數
幀間漂移 subcarrier = 每幀 Doppler 漂移 / subcarrier_spacing
```
若幀間漂移 ≤ 5 subcarrier，則 CFO_TRACK_W = 5 的設定安全。
