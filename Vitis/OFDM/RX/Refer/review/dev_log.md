# ref_rx_packed 開發與驗證記錄

**IP 功能：** OFDM RX 通道等化器（Channel Equalizer）  
**路徑：** `C:/VitisProject/Vitis/OFDM/RX/Refer/`  
**最終狀態：** csim PASS、cosim PASS、Vivado loopback BER = 0

---

## 系統背景

本 IP 位於 OFDM RX 鏈路的第二個環節：

```
smart_FFT → [ref_rx_packed] → rx_deinterleaver_float → p8psk_demap
```

**輸入：** 2048-point FFT 輸出，硬體格式為 Q15 定點數（`int16 × 2048`），打包成 32-bit AXI-Stream（`[31:16]=Q, [15:0]=I`）  
**輸出：** 1632 個等化後的 Data Tone，格式為 FP16 IQ，打包成 32-bit AXI-Stream

**OFDM 幀結構：**
- FFT 長度：2048
- 左 Guard：72，右 Guard：71
- Pilot Tone：273 個（間距 7）
- Data Tone：1632 個（每兩個 Pilot 間 6 個 Data）
- CFO 掃描範圍：±70

---

## 修改歷程

### 問題 1：資料型態錯誤 — `u16_to_half` vs `fixed16_to_half`

**症狀：** 所有等化輸出數值異常，無法對齊星座點。

**根因：**  
硬體 FFT（smart_FFT）的輸出為 Q15 定點數，代表實際數值乘以 2048 後的整數。  
原本的 `u16_to_half()` 直接把 16-bit pattern reinterpret 成 FP16，這是**格式完全不同**的操作，無法正確還原數值。

**修正（`ref_rx_defs.h`）：**

新增 `fixed16_to_half()`：
```cpp
static inline half fixed16_to_half(ap_uint<16> d)
{
    int16_t fixed_val = (int16_t)d.to_uint();  // 保留正負號
    return (half)((float)fixed_val / 2048.0f); // 還原小數點
}
```

同時修正 `u16_to_half` / `half_to_u16` 改用 `memcpy` 避免編譯器指標崩潰：
```cpp
static inline half u16_to_half(ap_uint<16> d) {
    half h;
    uint16_t tmp = d.to_uint();
    memcpy(&h, &tmp, 2);
    return h;
}
```

---

### 問題 2：PILOT_ROM_INV 符號錯誤（122/273 個錯誤）

**症狀：** csim 輸出誤差仍大，部分 Data Tone 等化後符號完全相反。

**根因：**  
`PILOT_ROM_INV[273]` 是 TX 送出的 Pilot 序列的反向（用於通道估計）。  
原版陣列有 122/273 個值符號錯誤，導致相鄰 Pilot 估出的通道響應 H 符號相反，線性插值時路徑穿越 0，造成 Data Tone 附近的等化結果被大幅放大或反向。

**修正（`ref_rx_defs.h`）：**  
從 TX 的 `PILOT_ROM`（`ref_tx_defs.h`）重新推導：
```
PILOT_ROM_INV[k] = 0.75 × sign(PILOT_ROM[k])
```
修正後 0 mismatch，全部 273 個值與 TX 一致。

---

### 問題 3：通道估計精度不足（FP16 → float）

**症狀：** 修正 PILOT_ROM_INV 後大部分正確，但插值邊界（k=1, k=6）仍有輕微誤差。

**根因：**  
通道估計（`h1_real`, `h2_real`, 插值斜率 `m_real/m_imag`）原本使用 `half`（FP16）運算。  
FP16 精度（約 3 位有效十進位）在插值計算中累積誤差，放大到後續等化。

**修正（`ref_rx_packed.cpp`）：**  
通道估計全部改用 `float`（FP32）：
```cpp
float p_inv1 = (float)PILOT_ROM_INV[i];
float h1_real = (float)fixed16_to_half(buf_i[idx1]) * p_inv1;
float h1_imag = (float)fixed16_to_half(buf_q[idx1]) * p_inv1;
// ...插值斜率、插值計算均為 float
```
輸出前才轉回 `half`：
```cpp
out_pkt.data = pack_iq((half)out_i, (half)out_q);
```

---

### 問題 4：缺少增益補償（ZF 等化不完整）

**症狀：** 在有通道衰減的 TB 下（`ch_gain = 0.5`），輸出振幅縮小，Average Error ≈ 0.75。

**根因：**  
原版只做了 `Y × H*`（補相位），沒有除以 `|H|²`（補增益）。  
標準 Zero-Forcing 等化公式為：

```
out = Y × H* / |H|²
```

若只做分子，通道增益 0.5 導致 `|H|²= 0.25`，輸出被縮小到正確值的 25%。

**驗證（2026-04-14）：**  
- 移除 `/h_power` → csim Average Error = **0.750**（錯誤）
- 還原 `/h_power` → csim Average Error = **~0**（正確）

**修正（`ref_rx_packed.cpp`）：**
```cpp
float h_power = cur_h_r * cur_h_r + cur_h_i * cur_h_i;
if (h_power < 0.0001f) h_power = 1.0f;  // 除零保護

float out_i = num_i / h_power;
float out_q = num_q / h_power;
```

---

## 最終程式碼結構

```
ref_rx_packed()
├── Load_Packed       : 讀入 2048 點 FFT，解包 I/Q 存入 buf_i/buf_q
├── Loop_CFO_Scan     : 掃描 ±70 offset，找 Pilot 能量最大的位置 (best_cfo)
└── Loop_Process_Packed : 對每對相鄰 Pilot 做通道估計 + 線性插值 + ZF 等化
    └── Loop_Interp   : 對 k=1..6 的 Data Tone 輸出等化結果
```

**ZF 等化數學：**
```
H_est = received_pilot × PILOT_ROM_INV  (float)
out   = (Y × H_est*) / |H_est|²
```

---

## 測試結果

### csim TB 設定（`tb_ref_rx_packed.cpp`）

| 參數 | 值 |
|---|---|
| CFO offset | 70 |
| Channel gain | 0.5 |
| Channel phase | 45° |
| 資料格式 | QPSK（gold_i=±0.707, gold_q=0.707）|
| 輸入模擬 | FFT 定點數（×2048），含 CFO 偏移與通道旋轉 |

### 結果對照

| 版本 | Average Error | Max Error | 說明 |
|---|---|---|---|
| 無 `/h_power` | 0.750232 | 0.750221 | 只補相位，增益錯誤 |
| **有 `/h_power`（最終版）** | **~0** | **~0** | ZF 完整，正確 |

### Vivado Loopback 驗證

- 測試：RX loopback（TX 輸出直接接 RX）
- 結果：output == input，**BER = 0**
- 時間：2026-04-12

---

## 檔案清單

| 檔案 | 說明 |
|---|---|
| `ref_rx_packed.cpp` | 主程式（CFO 掃描、通道估計、ZF 等化） |
| `ref_rx_defs.h` | 參數定義、PILOT_ROM_INV[273]、helper functions |
| `tb_ref_rx_packed.cpp` | csim Testbench（含通道模擬、誤差分析） |
| `scripts/run_all.tcl` | csim / syn / cosim / export 自動化腳本 |
| `.vscode/tasks.json` | VS Code 一鍵執行 HLS task |
