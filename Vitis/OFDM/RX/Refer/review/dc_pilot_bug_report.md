# DC Subcarrier Pilot Bug Report
# OFDM PHY Loopback — 5 個系統性 Bit Error 調查報告

**日期：** 2026-04-21  
**系統：** test_loopback PHY（無 channel coding）  
**路徑：** `C:/Vivado/test_loopback`

---

## 1. 問題發現

### 測試背景
- 鏈路：`input(8-bit LSB) → symbol_mapper → tx_interleaver_float → reference_signal_tx → smart_IFFT → smart_FFT → ref_rx_packed → rx_deinterleaver_flo → p8psk_demap → output(LLR 8-bit)`
- 輸入：4896 bytes，每 byte 僅用 LSB（0 或 1），對應 8PSK 3-bit/symbol × 1632 symbols
- 預期：完美 loopback，LLR 正負號與輸入 bit 完全對應

### 發現過程
1. **Vivado simulation**（`tb_ip.v`）：送 4896 bytes，收 4896 bytes LLR
2. **FPGA 上板驗證**（`C:/Vitis/test_loop/app_loop/src/main.c`）：使用完全相同的輸入資料（`sim_tx_data.h`），與 sim golden 比對
3. 結論：**HW vs Sim = 0/4896 LLR byte mismatch**（完全一致）
4. 但 bit error rate：**5/4896 固定錯誤**，兩者相同

### 錯誤位置（LLR output index）
```
48, 146, 242, 4749, 4845
```

---

## 2. 問題追蹤

### Step 1：LLR index → 8PSK symbol

每個 symbol 輸出 3 個 LLR byte（b0, b1, b2）：

| LLR index | Symbol | LLR bit | 定義 |
|-----------|--------|---------|------|
| 48        | 16     | b0      | `(|I|−|Q|)×SQRT2_INV` |
| 146       | 48     | b2      | `Q` |
| 242       | 80     | b2      | `Q` |
| 4749      | 1583   | b0      | `(|I|−|Q|)×SQRT2_INV` |
| 4845      | 1615   | b0      | `(|I|−|Q|)×SQRT2_INV` |

### Step 2：rx_deinterleaver_flo 反推

`rx_deinterleaver_flo` 為 51×32 矩陣解交錯，公式：
```
deinterleaver_output[i] = deinterleaver_input[(i%32)*51 + i//32]
```

反推 `deinterleaver_input` 位置（= `ref_rx_packed` 輸出的 data tone index）：

| demap symbol | ref_rx_packed output (data tone) |
|-------------|----------------------------------|
| 16          | 816                              |
| 48          | 817                              |
| 80          | 818                              |
| 1583        | 814                              |
| 1615        | 815                              |

**→ 錯誤集中在 ref_rx_packed 輸出的 data tone 814, 815, 816, 817, 818**

### Step 3：data tone → FFT bin

Active band 從 DC-centered position 72 開始（`LEFT_GUARD=72`）：
```
active_idx = a → DC-centered position = 72 + a
pilot at: a % 7 == 0
data_tone_number = a - a//7 - (1 if a%7!=0 else 0) ... (see formula)
```

| data tone | active_idx | DC-centered bin | interval i | k |
|-----------|-----------|-----------------|-----------|---|
| 814       | 950       | 1022            | 135       | 5 |
| 815       | 951       | 1023            | 135       | 6 |
| 816       | 953       | 1025            | 136       | 1 |
| 817       | 954       | 1026            | 136       | 2 |
| 818       | 955       | 1027            | 136       | 3 |

### Step 4：共同依賴 → pilot[136]

`ref_rx_packed` 的 `Loop_Process_Packed` 以相鄰 pilot pair 做線性插值：
- interval i=135：pilot[135](bin 1017) ↔ pilot[136](bin **1024**)，data tone 814,815 在 k=5,6
- interval i=136：pilot[136](bin **1024**) ↔ pilot[137](bin 1031)，data tone 816,817,818 在 k=1,2,3

**→ 5 個錯誤全部依賴 pilot[136] 的通道估計值**

---

## 3. 根本原因分析

### pilot[136] 的特殊位置

```
pilot[136]:
  active_idx = 7 × 136 = 952
  DC-centered position = 72 + 952 = 1024
  = FFT_LENGTH / 2 = 2048 / 2
```

DC-centered position 1024 = **fftshift 邊界 = IFFT/FFT 的 bin 0（DC 分量）**

### fftshift 的轉換關係

TX 鏈路：
```
reference_signal_tx 寫 S[1024] = pilot[136]（DC-centered）
→ axis_fft_shift（pre-IFFT）: S[1024] → IFFT input[0]（DC bin）
→ Xilinx IFFT IP（bin 0 = DC 分量）
```

RX 鏈路：
```
Xilinx FFT IP 輸出 Y[0]（DC bin）
→ axis_fft_shift（post-FFT）: Y[0] → buf[1024]
→ ref_rx_packed 讀 buf_i[1024] 做 pilot[136] 通道估計
```

### 數學分析

FFT IP 設定（`fft_config_driver.sv`）：
- IFFT scaling schedule：`12'hAA9`（= /2048，1/N 正規化）
- FFT scaling schedule：`12'h000`（= 不縮放）

Round-trip 推導：
```
IFFT: x[n] = (1/2048) × Σ X[k] × exp(j2πkn/2048)
FFT:  Y[k] = Σ x[n] × exp(-j2πkn/2048) = X[k]
→ Y[k] = IFFT_input[k] （理論上完全還原）
→ buf[1024] = S[1024] = Int16(PILOT_ROM[136] × 2048) = 2730
→ H_est = fixed16_to_half(2730) × PILOT_ROM_INV[136] = 1.333 × 0.75 = 1.000 ✓
```

**數學上 H_est[136] 應為 1.0，但實際觀察到 5 個 bit error。**

### 假說：DC bin Sign Flip

若 Xilinx FFT IP 的定點運算在 bin 0（DC）產生 sign flip（H_est ≈ -1.0），可解釋錯誤分布：

插值公式（k=1..6，H1=pilot[i], H2=pilot[i+1]）：
```
H_interp = H1×(7-k)/7 + H2×k/7
```

若 H_est[136] = -1（其餘 = +1）：

| interval | bin  | k | H_interp          | sign | error? |
|----------|------|---|-------------------|------|--------|
| i=135    | 1018 | 1 | 6/7 + (-1)×1/7 = +5/7 | + | ✗ |
| i=135    | 1019 | 2 | 5/7 + (-1)×2/7 = +3/7 | + | ✗ |
| i=135    | 1020 | 3 | 4/7 + (-1)×3/7 = +1/7 | + | ✗ |
| i=135    | 1021 | 4 | 3/7 + (-1)×4/7 = -1/7 | − | ✓（可能因 TX bit=0 不出錯）|
| i=135    | 1022 | 5 | 2/7 + (-1)×5/7 = -3/7 | − | **✓ error data tone 814** |
| i=135    | 1023 | 6 | 1/7 + (-1)×6/7 = -5/7 | − | **✓ error data tone 815** |
| i=136    | 1025 | 1 | (-1)×6/7 + 1/7 = -5/7 | − | **✓ error data tone 816** |
| i=136    | 1026 | 2 | (-1)×5/7 + 2/7 = -3/7 | − | **✓ error data tone 817** |
| i=136    | 1027 | 3 | (-1)×4/7 + 3/7 = -1/7 | − | **✓ error data tone 818** |
| i=136    | 1028 | 4 | (-1)×3/7 + 4/7 = +1/7 | + | ✗ |
| i=136    | 1029 | 5 | (-1)×2/7 + 5/7 = +3/7 | + | ✗ |
| i=136    | 1030 | 6 | (-1)×1/7 + 6/7 = +5/7 | + | ✗ |

→ 完美匹配觀測到的 5 個錯誤位置（bin 1022,1023,1025,1026,1027）

---

## 4. 驗證實驗（尚未執行）

### 實驗 A：快速驗證 sign flip
修改 `ref_rx_defs.h` 第 90 行：
```cpp
// 原始（index 130..139）：
-0.75f, +0.75f, +0.75f, -0.75f, +0.75f, -0.75f, +0.75f, +0.75f, -0.75f, +0.75f,
//                                                 ↑ index 136
// 改成：
-0.75f, +0.75f, +0.75f, -0.75f, +0.75f, -0.75f, -0.75f, +0.75f, -0.75f, +0.75f,
//                                                 ↑ 改為 -0.75f
```
重跑 Vivado simulation，若 5 個 error 消失 → 確認 sign flip 假說。

### 實驗 B：量測 buf_i[1024] 實際值
在 HLS C simulation 加入 print，或在 Vivado 波形中抓 ref_rx_packed 的 buf_i[1024] 值。
預期：若假說正確，應看到 buf_i[1024] = -2730（而非 +2730）。

---

## 5. 根本修正方案

### 方案一（治標）：補償 PILOT_ROM_INV[136]

若實驗 A 確認：直接將 PILOT_ROM_INV[136] 永久改為 -0.75f。

缺點：治標不治本，未解決 DC bin 的物理問題。

### 方案二（治本）：排除 DC Subcarrier

在 OFDM 中，DC subcarrier 不應承載資訊（業界標準做法）。

**TX 修改（`reference_signal_tx_packed.cpp`）：**
```cpp
if (pos == FFT_LENGTH / 2) {
    output[pos] = 0;  // DC 強制為 null
    continue;
}
```

**RX 修改（`ref_rx_packed.cpp`，`Loop_Process_Packed`）：**
當 i=135 和 i=136 時，合併成跨 DC 的大 interval：
- pilot[135]（bin 1017）↔ pilot[137]（bin 1031）線性插值，跨 13 個 tone
- 完全不讀 buf_i[1024]

連帶影響：
- DATA_TONES：1632 → 1631（少 1 個 data slot）
- 封包長度、RS 編碼、交錯器都需對應調整
- 屬於系統性修改，工程量較大

---

## 6. 建議行動順序

1. **立即**：執行實驗 A，確認 sign flip 假說（5 分鐘）
2. **短期**：若確認，以方案一暫時修復，讓系統可運作
3. **長期**：評估是否需要方案二（如果要接 RF SoC 實際射頻，DC 排除是必要的）

---

## 附：相關檔案清單

| 檔案 | 說明 |
|------|------|
| `C:/VitisProject/Vitis/OFDM/RX/Refer/ref_rx_defs.h` | PILOT_ROM_INV 陣列（index 136 = +0.75f） |
| `C:/VitisProject/Vitis/OFDM/RX/Refer/ref_rx_packed.cpp` | Loop_Process_Packed，i=135,136 interval |
| `C:/VitisProject/Vitis/OFDM/TX/Refer/reference_signal_tx_packed.cpp` | pilot 寫入位置 |
| `C:/VitisProject/Vitis/OFDM/TX/Refer/ref_tx_defs.h` | PILOT_ROM（PILOT_ROM[136] = +1.333f） |
| `C:/Vivado/test_loopback/.../fft_config_driver.sv` | FFT scaling 設定 |
| `C:/Vivado/test_loopback/.../axis_fft_shift.sv` | fftshift FSM（已確認無 off-by-one）|
| `C:/Vivado/test_loopback/test_loopback.srcs/sim_1/new/tb_ip.v` | TB，送 4896 bytes |
| `C:/Vitis/test_loop/app_loop/src/main.c` | FPGA 驗證程式 |
