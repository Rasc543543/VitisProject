# 盲模式偵測設計討論 v2
# 日期：2026-05-19

## 背景與目標

TX 端可能隨機在 QPSK 和 8PSK 之間切換，**每幀都可能不同**。  
RX 端不知道當前調變方式，需要自動偵測並正確解碼。

**設計標準**：每幀獨立偵測，不依賴前一幀的狀態，才算合格的系統。

---

## 為什麼用 RS Decoder 做偵測

實測觀察：把 QPSK 資料送進 8PSK 鏈路 → 系統卡死（DMA timeout）。

根本原因追蹤：
```
錯誤模式資料 → Viterbi 輸出亂碼 → RS 無法糾正（錯誤數 > T=8）
→ RS decoder 執行 continue（不寫 output_stream）
→ rx_defragment 永遠等不到 tlast
→ DMA 永遠等不到資料 → timeout → 卡死
```

RS decoder（`rs_decoder_rx.cpp`，line 486）目前行為：
```cpp
if (uncorrectable)
    continue;  // 直接跳過，不輸出任何東西
```

**RS failure 在整條 RX 鏈路中最早出現**（~73 µs），比 DMA timeout 快幾個數量級，適合作為偵測信號。

---

## 關鍵前置問題：RS failure 輸出

目前 RS decoder **沒有 failure flag 輸出端口**。  
要用 RS 做偵測，需要選擇以下其中一個方向（目前純討論，未實作）：

### 方向 A：加 failure flag 輸出腳位
```cpp
void rs_decoder_rx(
    hls::stream<axis_word> &input_stream,
    hls::stream<axis_word> &output_stream,
    ap_uint<1> &decode_failed)  // ← 新增
```
CPU 透過 AXI-Lite 讀取，得知當幀是否解碼失敗。

### 方向 B：失敗時輸出假 tlast
```cpp
if (uncorrectable) {
    // 輸出 1 byte dummy + last=1，讓 DMA 正常完成
    output_stream.write(dummy_with_last);
    continue;
}
```
CPU 透過 flag 知道這筆 DMA 資料要丟棄。  
**優點**：DMA 永遠正常完成，不需要 reset，支援每幀切換。

> **推薦方向 B**，因為可完全避免 DMA 卡住問題，讓架構更乾淨。

---

## 方法 A：DDR 中轉 + 雙路平行讀取

### 架構

```
ADC → FFT → ref_rx_packed
                  │
            DMA_write
                  │
                 DDR（存一份等化後資料）
                  │
     CPU 觸發兩個 DMA 同時讀同一塊位址
                  │
        ┌─────────┴──────────┐
     DMA_Q (read)        DMA_8 (read)
        ▼                    ▼
   QPSK 完整鏈路         8PSK 完整鏈路
   demap→Viterbi→RS      demap→Viterbi→RS
        │                    │
   decode_failed=?       decode_failed=?
        └─────────┬──────────┘
                  ▼
        CPU 讀兩個 flag，flag=0 的那條資料有效
```

### 可行性
- DDR 支援多個 AXI master 同時讀，Zynq HP port 支援
- 兩個 DMA 讀同一塊 DDR 位址合法
- 兩條鏈路**同時解碼**，不是序列

### 優缺點
| 優點 | 缺點 |
|------|------|
| DDR 有資料備份，可重播 | 多一層 DDR 存取延遲（write + read）|
| 不受 backpressure 問題影響 | 需要管理「write 完成後才觸發兩個 read」|
| 兩條鏈路完全解耦 | 流程多一個同步點 |

---

## 方法 B：axis_broadcaster 直接分流

### 架構

```
ADC → FFT → ref_rx_packed
                  │
           axis_broadcaster    ← Xilinx 內建 IP，一進二出
                  │
        ┌─────────┴──────────┐
      FIFO_Q               FIFO_8   ← 各加一個小 FIFO，解耦兩條鏈路速度
        ▼                    ▼
   QPSK 完整鏈路         8PSK 完整鏈路
   demap→Viterbi→RS      demap→Viterbi→RS
        │                    │
   decode_failed=?       decode_failed=?
   DMA_Q                 DMA_8
        └─────────┬──────────┘
                  ▼
        CPU 讀兩個 flag，flag=0 的那條 DMA 資料有效
             flag=1 的那條 DMA 資料丟棄
```

### FIFO 的必要性
QPSK 鏈路和 8PSK 鏈路處理速度不同（RS 長度不同），  
若無 FIFO，broadcaster 會被慢的那條拖住（backpressure 傳回去影響 ref_rx_packed）。  
兩條鏈路各加一個小 FIFO → 獨立跑，互不影響。

### 優缺點
| 優點 | 缺點 |
|------|------|
| 純串流，無 DDR 中轉，延遲最低 | 需要額外 FIFO |
| 架構簡單，無同步點 | 無法重播（資料即時通過）|
| 每幀 ready 最快 | 需要 RS decoder 輸出假 tlast（方向 B）|

---

## 兩種方法比較

| 項目 | 方法 A（DDR 中轉）| 方法 B（broadcaster 直通）|
|------|------------------|--------------------------|
| 延遲 | 較高（DDR 往返）| 最低（純串流）|
| 複雜度 | 中（DMA 同步管理）| 低（標準串流 IP）|
| 重播能力 | ✅ 有 | ❌ 無 |
| 硬體資源 | DMA × 3 | broadcaster + FIFO × 2 |
| 每幀切換支援 | ✅（兩路同時讀）| ✅（需假 tlast）|
| DMA 卡住問題 | 需假 tlast 或 reset | 需假 tlast |

---

## 共同需求

無論選哪個方法，以下兩件事都要做：

1. **RS decoder 修改**（方向 B 建議）：失敗時輸出假 tlast，讓 DMA 正常完成
2. **硬體資源評估**：Viterbi × 2（LUT ~34%）+ 其他 IP，需 implementation 確認 timing

---

## 邊界情況處理

| 情況 | 處理方式 |
|------|---------|
| 兩個 RS 都 failure（SNR 太差）| CPU 丟棄本幀，等下一幀；可設重試計數 |
| 兩個 RS 都 success（SNR 極高僥倖）| 需第二層判斷（例如 CRC 或內容合理性）；極少發生 |
| 下一幀來時 DMA 還未完成 | 假 tlast 確保 DMA 已完成；此情況不發生 |
