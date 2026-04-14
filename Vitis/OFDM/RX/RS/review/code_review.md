# RS Decoder RX — Manual Code Review
**Project**: rs_decoder_rx  
**Date**: 2026-04-08  
**Reviewer**: Claude (Step 3 of 開發流程)  
**Files**: rs_decoder_rx.cpp, rs_decoder_rx.h  

---

## 總結

| 等級 | 數量 |
|------|------|
| 🔴 CRITICAL | 1 |
| 🟡 WARNING  | 3 |
| 🔵 INFO     | 3 |

---

## 🔴 CRITICAL

### C1 — `uncorrectable` 判斷被註解掉（Line 400-404）

```cpp
// 🚨 終極除錯關鍵：強制輸出爛資料！
// if (uncorrectable) {
//     return;
// }
```

**問題**：這段 debug code 留在 synthesis path 裡。
當 RS 演算法判定錯誤超過 T=8 個無法糾正時，本應丟棄封包，現在卻把**未修正的損壞資料直接送出 output_stream**。

**影響**：Vivado sim 的輸出資料 `0xBB` 是錯誤 codeword 硬算出來的結果，**不能代表演算法正確**。

**修正**：
```cpp
if (uncorrectable) {
    return;  // 或輸出 error flag，視系統設計
}
```

---

## 🟡 WARNING

### W1 — `static data_buf` 在 `ap_ctrl_none` 模式下的行為（Line 108-109）

```cpp
static uint8_t data_buf[MAX_BUF_SIZE];
#pragma HLS ARRAY_PARTITION variable = data_buf cyclic factor = 4 dim = 1
```

**設計意圖**：正確。`static` 讓這塊 buffer 合成為 persistent BRAM/LUT-RAM，
並且 Forney 演算法在 Chien Search 中直接寫回修正值（Line 383），需要同一塊記憶體。

**潛在風險**：
- `ap_ctrl_none` = 無 ap_start/ap_done，函式持續執行
- 若封包長度不是 204/306（line 157 `return`），函式立即返回，
  **static buffer 殘留上一包資料**。下一包正常封包到來時從 `input_len=0` 覆蓋，結果正確。
- **但如果兩個 204-byte 封包連續到來（back-to-back），第二包在 OUTPUT_LOOP 還在跑時就開始 COLLECT_LOOP**，
  因為這兩個 loop 是順序執行，`ap_ctrl_none` 不允許重入，**不會同時跑**，所以目前架構安全。

**結論**：目前單一 IP core 單一 stream 架構下 OK。若未來改 DATAFLOW 或多核，需要 double buffer。

---

### W2 — GF_EXP\[255\] = 0x00（Line 86）✅ 已確認正確，非 bug

```cpp
// GF_EXP 陣列最後一個元素（index 255）：
0x2C, ..., 0x47, 0x8E, 0x00   ← GF_EXP[255]
```

**確認**：GF_EXP[255] = 0x00 是刻意設計。`gf_mul_local` 的 sum 永遠在 [0, 254]（兩個 log 值相加最大 254+254=508，模 255 後不會超過 254），`gf_pow` 的 idx 也在 [0, 254]，**GF_EXP[255] 從不被存取**。此欄位值不影響計算結果。

---

### W3 — Testbench 沒有錯誤注入測試（tb_rs_decoder.cpp）

目前的 testbench 送進去的 `test_data[i] = i` 是**非合法 RS codeword**，沒有 parity bytes，
所以 syndrome 一定非零，演算法會進入 BM/Chien 流程，但結果不可預期。

**必要的 testbench 補強**：
1. 送入一個合法的 RS(204,188) codeword（syndrome = 0）→ 期待 input = output
2. 在合法 codeword 注入 1~8 個錯誤 → 期待修正後 output = 原始資料
3. 注入 9 個錯誤 → 期待 uncorrectable（`return`，無 output）

**目前 Vivado sim 的輸出無法驗證演算法正確性，只能驗證 AXI-Stream 時序。**

---

## 🔵 INFO

### I1 — GF_EXP / GF_LOG 宣告在 function 內（非 static const）（Lines 70-106）

```cpp
const uint8_t GF_EXP[256] = { ... };
const uint8_t GF_LOG[256] = { ... };
```

**合成行為**：正確。HLS 會把這兩個 `const` 陣列識別為 ROM，搭配 `BIND_STORAGE rom_1p lutram` 正常合成。  
**模擬行為**：每次呼叫都會「初始化」這兩個陣列（stack 上分配 512 bytes），
對 C simulation 有額外開銷，但功能正確。  
**若改為 `static const`**：在模擬中只初始化一次，略微加速 csim，合成結果相同。

---

### I2 — BM_LOOP PIPELINE II=1 可能無法達成（Lines 231-285）

```cpp
BM_LOOP:
    for (int r = 1; r <= TWO_T; r++)
    {
#pragma HLS PIPELINE II = 1
```

BM 演算法每一輪 `r` 的計算依賴前一輪的 `sigma[]`、`b[]`、`L`（loop-carried dependency）。
HLS 可能因為 dependency 只能達到 II=2 或更高，**請確認 synthesis report 中的 BM_LOOP 實際 II**。
若 II 較高，總延遲會增加但不影響正確性。

---

### I3 — `ap_ctrl_none` + do-while COLLECT_LOOP（Lines 117-133）

```cpp
#pragma HLS INTERFACE ap_ctrl_none port = return
...
COLLECT_LOOP:
    do { ... } while (!tlast_seen && input_len < MAX_BUF_SIZE);
```

**設計正確**：`ap_ctrl_none` 搭配 do-while 讓 IP 持續等待 input stream，
整個函式等同一個永遠運行的狀態機，這是 AXI-Stream IP 的標準設計模式。

**注意**：`#ifndef __SYNTHESIS__` 的 `input_stream.empty()` early-return 僅在 csim 時有效，
synthesis 中不存在，RTL 會正確阻塞在 `input_stream.read()` 等待資料。

---

## 行動項目

| 優先 | 項目 | 位置 |
|------|------|------|
| 🔴 立即 | 移除 uncorrectable 的 debug 註解，恢復正確判斷 | rs_decoder_rx.cpp:400 |
| 🟡 短期 | 補寫含合法 RS codeword 的 testbench（含錯誤注入） | tb_rs_decoder.cpp |
| 🟡 短期 | GF_EXP[255] 修正為 0x01 | rs_decoder_rx.cpp:86 |
| 🔵 觀察 | 確認 synthesis report 中 BM_LOOP 實際 II | review/parse_report.txt |

---

## 自動檢查補充說明

`check_hls_code.py` 回報的 TYPE WARNING（hls::stream 但找不到 ap_axiu）是**誤報**：  
`ap_axiu<8,0,0,0>` 定義在 `rs_decoder_rx.h` 中（typedef axis_word），  
checker 只掃描 .cpp 所以找不到。**實際介面型別正確。**
