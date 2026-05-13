# CFO 同步論文深度分析：五篇精選比較

整理日期：2026-05-10  
對應專案：`ref_rx_packed.cpp` CFO 優化  
資料來源：NotebookLM notebook「Zadoff-Chu Synchronization for Short-Burst OFDM Signals」（21 篇中精選 5 篇）

---

## 快速比較總覽

| # | 論文 | 核心方法 | CFO 類型 | 需要 Training Symbol? | FPGA 友善度 | 對你的優先度 |
|---|------|---------|---------|----------------------|------------|------------|
| 1 | **Classen CFO Synchronization**（review） | Capture + Tracking 雙模 | Integer + Fractional | 否 | 概念層 | ★★★ 最高（架構依據） |
| 2 | **FFT-Based ICFO**（802.15.4g） | FFT cross-correlator 取代逐一掃描 | Integer 只 | 是（LTF） | Reg -70%, Logic -39% | ★★ 中（需改 frame 結構） |
| 3 | **SAL Algorithm**（鄰近 subcarrier 相似性） | 絕對值加法取代平方 | Integer 只 | 是（preamble） | LUT -90%, FF -94% | ★★★ 高（energy 計算可直接移植） |
| 4 | **Decision Feedback Estimator** | data+pilot 聯合消除 ICI error floor | Residual Fractional | 否 | 無 FPGA 數據 | ★ 低（BER=0 暫不需要） |
| 5 | **Easy Hardware LS**（802.11a） | LS → shift-add 近似 | Residual Fractional 只 | 否 | DSP -75% | ★ 低（暫無 fractional 需求） |

---

## 論文 1：Classen CFO Synchronization

**論文全名：** Carrier Frequency Offset-Based OFDM Synchronization Technology（Review Paper）

### 核心架構：Capture + Tracking 雙模

**Capture Mode（大掃描）**
- 估測 integer CFO + fractional CFO 同時補償
- 補償量 = integer + fractional 之和
- 對應你的：啟動時 141-offset 掃描

**Tracking Mode（trace mode）**
- 只估測並補償 fine CFO
- 使用 pilot 的相位旋轉量計算
- 對應你的：neighbor energy check

### Tracking Mode 數學公式

```
ε̂_f = (1 / 2π·T_sub·D) · arg{ Σ(j=1 to L-1) Y_{l+D}[p, ε̂_acq] · Y_l*[p, ε̂_acq] · X_{l+D}*[p] · X_l[p] }

其中：
  Y      = 接收到的頻域信號
  X      = 已知 pilot 符號
  p      = pilot subcarrier 索引
  ε̂_acq = capture phase 取得的粗 CFO 估測值
  D      = 符號間距（相鄰 pilot 的相位差）
```

### Pilot 結構要求

每個 OFDM symbol 都要嵌入 pilot，才能做連續追蹤（你的 273 pilots/frame 符合此要求）。

### 缺失資訊（此為 review paper 的限制）

- 切換觸發條件未指定：未說明何時從 tracking mode 回到 capture mode
- 計算複雜度未與 exhaustive search 比較
- Pilot 數量未給出具體數字

### 對你的價值

**學術引用依據：** 你的 CFO cache 設計在學術上對應「Classen capture+tracking two-mode」，可引用此論文作為方法論基礎。

---

## 論文 2：FFT-Based ICFO（An Implementation of a Low Complexity Integer CFO Estimator for OFDM）

**目標標準：** IEEE 802.15.4g MR-OFDM  
**實作平台：** Altera Cyclone V FPGA

### 硬體架構流程

```
接收 time-domain LTF
        ↓ ×
conj(本地 LTF 參考)
        ↓
     FFT (radix-2 CORDIC butterfly)
        ↓
  Peak Searcher (CORDIC vectoring mode + comparator)
        ↓
  ICFO = peak index
```

### 速度對比（FFT size = 128）

| 方法 | 時脈數（cycles） | 相對速度 |
|------|----------------|---------|
| Serial exhaustive search | 16,384 | 1× (baseline) |
| Parallel exhaustive search | 4,096 | 4× |
| **FFT-based（此論文）** | **576** | **28.4× 快（vs serial）** |

公式：log₂(N) × N/2 + N = 7×64 + 128 = 576 cycles（N=128）

### FPGA 資源節省

| 指標 | 節省量 |
|------|--------|
| Registers | -70% |
| Combinational Logic | -39% |
| Latency vs serial search | 28.4× 快 |

### 限制與假設

- **需要 perfect timing sync**：必須先完成 symbol timing synchronization
- **需要 training symbol（LTF）**：使用 Long Training Field（值域 {+1, -1, 0}），不是 scattered pilot
- 若你的 frame 沒有專用 training symbol，此架構不能直接套用

### 對你的價值

你目前的 141-offset 掃描用的是 scattered pilots（273 個/frame），不是 LTF，**架構不能直接套用**。若未來重新設計 frame 結構加入 preamble，此方案可做初始啟動掃描（28.4× 加速）。

---

## 論文 3：SAL Algorithm（A Novel Integer Frequency Offset Estimation Method for OFDM Based on Preamble）

### 核心數學原理：鄰近 Subcarrier 相似性

```
/* 傳統做法（SAH）：最小化差值能量 */
Ĝ = argmin_g Σ_k |Z(k) - Z(k-1)|²
  其中 Z(k) = Y(k) · X*(k-g)，Y=接收，X=preamble，g=候選 IFO

/* SAL（低複雜度版）：改用絕對值之和 */
Ĝ = argmin_g Σ_k ( |Re(M(k))| + |Im(M(k))| )
  其中 M(k) = Z(k) - Z(k-1)

/* 直覺解釋：
   g 正確時 → Z(k)≈1 → M(k)≈0 → |Re|+|Im| 最小
   g 錯誤時 → Z(k) 隨機複數 → M(k) 大 → |Re|+|Im| 大    */
```

### 硬體優化：兩個省乘法器技巧

**技巧一：Conjugate 乘法省略**  
因 preamble 值域限制在 `{1, j, -1, -j}`，`X*(k-g)` 的共軛乘法可化簡成 real/imaginary 部分的交換或取反，無需複數乘法器。

**技巧二：平方 → 絕對值**

| 傳統（SAH） | SAL 改法 |
|------------|---------|
| `energy += Re² + Im²` | `energy += \|Re\| + \|Im\|` |
| 需要 2 個 multiplier/subcarrier | 只需 abs() = sign bit 操作 |
| ~38,493 次平方乘法（141×273） | 0 個 multiplier |

### FPGA 資源節省（vs SAH）

| 指標 | 節省量 |
|------|--------|
| LUT | **-90%** |
| Flip Flop | **-94%** |

### Preamble 結構要求

- 只需 1 個已知 preamble symbol
- 值域限制：每個 subcarrier ∈ {1, j, -1, -j}（QPSK-like）
- 覆蓋整個 IFFT 大小 N 個 subcarrier

### 估測範圍與性能

- 最大估測範圍：[-N/2, N/2-1] 個 subcarrier offset
- 性能：比傳統 ML（CAF）估測失敗率更低；SAL 略低於 SAH 但仍優於 baseline

### 對你的直接可用部分

**可立即移植：** energy 計算技巧

```cpp
// 舊：需要 multiplier
energy += sq_i * sq_i + sq_q * sq_q;

// 新：只需絕對值（argmax 結果完全相同）
energy += (sq_i < 0 ? -sq_i : sq_i) + (sq_q < 0 ? -sq_q : sq_q);
```

> **注意：** Conjugate 乘法省略技巧不可用，因為你的 pilot 值不是 {1,j,-1,-j}，仍需真正的複數乘法。但平方那步可以省。

---

## 論文 4：Decision Feedback Estimator（Decision Feedback Frequency Offset Estimator for Data-Pilot Multiplexed OFDM）

### 兩步驟流程

**Step 1：Coarse + 決策回饋**
1. 用 scattered pilots 做粗 CFO 補償（Classen/Bai 等方法）
2. 執行通道估測
3. 解調 data symbols（決策回饋）
4. 重建完整 OFDM symbol：pilot + detected data

**Step 2：精細 CFO 估測**
1. 用**所有 subcarrier**（pilot + detected data）聯合估測 CFO
2. 迭代計算並補償
3. SNR > 14 dB 時 error floor 完全消失

### Error Floor 問題解釋

```
傳統 pilot-only tracking 問題：
  在高 SNR 時，殘餘 CFO 造成的 ICI 不可忽略
  但 pilot 太稀疏，無法精確估計 residual CFO
  結果：BER 在高 SNR 不再下降 → error floor

Decision Feedback 解法：
  先把 data symbols 解碼（高 SNR 時初步解碼準確）
  把解碼後的 data 當作已知符號
  用 ALL subcarriers 做精細 CFO 估計 → residual CFO 準確
```

### 關鍵數字

| 指標 | 數值 |
|------|------|
| Error floor 消失 SNR | > 14 dB |
| 論文使用 pilot density | 25%（8 pilots / 32 subcarriers） |

### 對你的價值

你目前 BER=0，ZCU208 loopback CFO≈0，不存在 ICI error floor 問題。**未來 RF 部署（USRP、不同 FPGA）出現 high-SNR BER floor 時使用。**

---

## 論文 5：Easy Hardware LS（Easy-Hardware-Implementation Algorithm of CFO and SFO Estimation in OFDM）

**目標標準：** IEEE 802.11a  
**對象：** Residual fractional CFO（integer CFO 需由別的模組先處理）

### LS 公式展開成 Shift-Add

```
/* 802.11a 的 4 個 pilot 位置：-21, -7, +7, +21 */

/* 原始 LS slope 公式（需要乘法器） */
λ = (-3θ(-21) - θ(-7) + θ(7) + 3θ(21)) / K    ← K 不是 2 的次方

/* 近似展開為 3 個 2 的次方分母之和 */
λ ≈ X/128 - X/2048 - X/4096
  其中 X = -3θ(-21) - θ(-7) + θ(7) + 3θ(21)

/* 硬體實作（全部用移位） */
X/128  → X >> 7
X/2048 → X >> 11
X/4096 → X >> 12
3θ = (θ << 1) + θ   （shift-add，不用 multiplier）
```

### FPGA 資源節省

| 實作方式 | DSP Blocks |
|---------|-----------|
| 標準 LS | 32 |
| **Shift-Add（此論文）** | **8（-75%）** |

### 重要限制

- **只處理 Fractional/Residual CFO**：Integer CFO 需由獨立模組先處理
- **專為 802.11a 設計**：4 個固定位置 pilot，你的 273 pilots 結構不同，公式需重新推導
- ZCU208 loopback CFO≈0：暫無 fractional CFO 需要追蹤

### 對你的價值

若未來加入 fractional CFO 補償（RF 部署後），此論文的 shift-add 思路可用於設計 FPGA-friendly LS estimator。**當前不需要實作。**

---

## 六維比較矩陣

| 比較維度 | Classen | FFT-Based ICFO | SAL Algorithm | Decision Feedback | Easy LS |
|---------|---------|---------------|--------------|------------------|---------|
| CFO 類型 | Integer + Fractional | Integer 只 | Integer 只 | Residual Fractional | Residual Fractional 只 |
| 需要 training symbol? | 否 | **是（LTF）** | **是（preamble）** | 否 | 否 |
| Pilot 格式限制 | 無 | {+1, -1, 0} | {1, j, -1, -j} | 無 | 802.11a 固定 4 位置 |
| FPGA 資源節省 | 未說明 | Reg -70%, Logic -39% | LUT -90%, FF -94% | 無數據 | DSP -75% |
| 速度提升 | 未比較 | 28.4× faster | 未量化 | 迭代，較慢 | 快（單次計算）|
| 可直接用於你的 code? | ✅ 概念引用 | ⚠ 需改 frame 結構 | ✅ energy 計算可移植 | ❌ 暫不需要 | ❌ 暫不需要 |

---

## 對應 ref_rx_packed.cpp 的行動計畫

| 優先度 | 動作 | 來源論文 | 預期效益 | 難度 |
|-------|------|---------|---------|------|
| **立即** | CFO Cache 架構實作：best_cfo 改 static，第一幀掃完後保留，之後只做 neighbor check | 論文 1（Classen） | 省 428 µs/frame → ~2.59 Mbps | 低（已設計好） |
| **立即** | Energy 計算改絕對值：`sq_i*sq_i + sq_q*sq_q` → `abs(sq_i) + abs(sq_q)` | 論文 3（SAL） | 省 38,493 次乘法，HLS 不推 DSP48 | 低（單行改動） |
| **中期** | 加入 preamble training symbol，啟用 FFT-based ICFO | 論文 2（FFT-Based） | 啟動掃描 28.4× 加速 | 高（需改 TX/RX frame 結構）|
| **RF 部署後** | Decision Feedback Tracking | 論文 4 | 消除 ICI error floor | 高 |
| **RF 部署後** | Fractional CFO LS shift-add tracking | 論文 5 | DSP -75% | 中 |

---

## 可立即實作的 Code 改動（Loop_CFO_Scan）

```cpp
// 位置：ref_rx_packed.cpp，Loop_CFO_Scan 內層

// ❌ 舊（需要 multiplier，HLS 推 DSP48）
energy += sq_i * sq_i + sq_q * sq_q;

// ✅ 新（只需 abs，argmax 結果完全相同）
energy += (sq_i < 0 ? -sq_i : sq_i) + (sq_q < 0 ? -sq_q : sq_q);
```

**為什麼 argmax 不受影響：**  
L1 norm（`|Re|+|Im|`）和 L2 norm（`Re²+Im²`）對同一組數據的 argmax 結果相同，因為兩者都是正單調函數，最大值位置不變。

---

*資料來源：NotebookLM notebook「Zadoff-Chu Synchronization for Short-Burst OFDM Signals」*  
*HTML 版本：`C:\VitisProject\design_notes\cfo_papers_deep_dive.html`*
