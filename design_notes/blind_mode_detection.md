# 盲模式偵測（Blind Mode Detection）系統設計記錄

**日期**：2026-05-07  
**狀態**：架構規劃中，方向待確認

---

## 一、背景與問題起源

### 1.1 OFDM 系統雙模式設計

本系統支援兩種調變模式：

| 模式 | tx_frag frame 大小 | payload/frame | RS 編碼 | 星座點 |
|------|-------------------|--------------|---------|--------|
| QPSK | 188B | 179B | RS(204,188) | 2 bits/symbol |
| 8PSK | 274B | 265B | 2×RS(153,137) | 3 bits/symbol |

TX 鏈路透過 `tx_frag` IP 的 `mode` register 決定輸出的 frame 格式。

### 1.2 TX 鏈路的自動適應特性

**關鍵發現**：`symbol_mapper_hls_0` 會自動依輸入 bits 數量切換模式：

- 輸入 **4896 bits（612B）** → 自動切 **8PSK**（3 bits/symbol × 1632 tones）
- 輸入 **3264 bits（408B）** → 自動切 **QPSK**（2 bits/symbol × 1632 tones）

TX 鏈路不需要額外 mode 控制，只需設定 `tx_frag` 的 mode register 即可，後續全部自動適應。

### 1.3 RX 鏈路的限制

**RX 端的模式限制點是 Demapper**，非自動適應：

- `qpsk_demap_packed_0`：固定 QPSK 解調，只能處理 QPSK 符號
- `p8psk_demap_packed_0`：固定 8PSK 解調，只能處理 8PSK 符號

**原則**：`tx_frag mode` 必須與 BD 裡的 demap IP 對應，否則系統完全失效。

---

## 二、模式不匹配實驗驗證

### 2.1 實驗設定

- BD 使用 `qpsk_demap_packed_0`（QPSK RX chain）
- `tx_frag mode = 1`（8PSK，274B/frame）
- UDP payload = 500B，NUM_PKTS = 3

### 2.2 實驗結果

```
[TIMEOUT] pkt0 RX DMA Timeout!
pkt[0] bit_errors=2000  FAIL (RX TIMEOUT - RxBuf=0x00)
pkt[1] bit_errors=1998  FAIL (RX TIMEOUT - RxBuf=0x00)
pkt[2] bit_errors=2001  FAIL (RX TIMEOUT - RxBuf=0x00)

Total errors: 5999 / 12000 bits → BER ≈ 50%
```

### 2.3 分析

| 現象 | 原因 |
|------|------|
| DMA Timeout | rx_defragment 收到亂碼，找不到有效 0x47 sync byte，永不輸出 |
| RxBuffer 全 0x00 | rx_defragment 沒輸出，DMA S2MM 未收到任何資料 |
| BER ≈ 50% | TX pattern（非零）vs RxBuffer（全零），約一半 bits 不同，符合理論最大值 |

**結論**：Demapper 不匹配 → LLR 完全錯誤 → Viterbi/RS 無法解碼 → rx_defragment 永遠卡住 → BER = 50%（理論最大值）。

---

## 三、系統設計問題

### 3.1 實際系統架構

```
TX 鏈路：
  CPU → DMA MM2S → tx_frag → energy_dispersal → RS enc → Viterbi enc
      → tx_interleaver64 → symbol_mapper → tx_interleaver_float
      → reference_signal → IFFT → DAC

RX 鏈路：
  ADC → FFT → ref_rx → rx_deinterleaver_flo → Demap
      → rx_deinterleaver64 → Viterbi dec → rx_deinterleaver_dyn
      → RS dec → energy_descramble → rx_defragment → DMA S2MM → CPU
```

### 3.2 核心問題

TX 端可以動態切換模式（QPSK/8PSK），但 **RX 端不知道當前 frame 是哪個模式**。

若 RX 選錯 demap → 整個 frame 解碼失敗 → BER = 50%。

---

## 四、解法討論

### 方案一：AXI Stream Broadcaster（已排除）

**概念**：在 ADC 輸出後加入 Xilinx `axis_broadcaster`，同時送資料給 QPSK 和 8PSK 兩條 chain，比較哪條 BER 低。

**問題：Back-pressure 死鎖**

AXI Stream 握手機制：`TVALID & TREADY = 1` 才完成一個 beat 傳輸。

Broadcaster 規則：必須等**所有 output 都 TREADY=1** 才推進。

死鎖過程：
```
1. 錯誤模式 chain（如 QPSK chain 收到 8PSK 資料）
   → rx_defragment 收到亂碼，內部 buffer 填滿
   → TREADY 向上游傳遞 back-pressure，整條 chain 輸入端 TREADY=0

2. Broadcaster 看到 output_0（QPSK）TREADY=0
   → 停止推進，即使 output_1（8PSK）TREADY=1

3. 正確模式 chain（8PSK）被餓死
   → 兩條 chain 全部卡住 → 系統死鎖
```

**結論**：Broadcaster 不適用於其中一條 chain 可能無限期阻塞的情況。

---

### 方案二：在 Header 加 mode 欄位

**概念**：`tx_frag` header byte[8]（目前是重複的 pkt_idx）改為存放 mode（0=QPSK / 1=8PSK）。rx_defragment 解出 header 後選擇對應的 demap chain。

**優點**：
- 延遲最低（不需試兩次）
- 硬體資源 ×1（不需複製 chain）
- 適合即時連續串流

**缺點**：
- 需修改 `tx_frag` 和 `rx_defragment` IP 原始碼
- BD 需要加入 demap 切換邏輯（multiplexer）

---

### 方案三：DDR 讀兩次（序列）

**概念**：
1. ADC 資料存進 DDR（address A）
2. DMA 讀 address A → QPSK chain → 結果 + BER
3. DMA 再讀 address A → 8PSK chain → 結果 + BER
4. 比較 BER → 低者即為正確模式

**優點**：不需改 IP，無 back-pressure 問題  
**缺點**：延遲 = 2× 單條 chain 處理時間，硬體資源 ×1

---

### 方案四：DDR 讀兩次（平行）← 目前規劃方向

**概念**：
```
ADC DMA S2MM → DDR (address A)
                    │
         ┌──────────┴──────────┐
         │                     │
   DMA1 MM2S              DMA2 MM2S
   （讀 address A）        （讀 address A）
         │                     │
   完整 QPSK RX chain    完整 8PSK RX chain
   （FFT→ref_rx→           （FFT→ref_rx→
    qpsk_demap→...→          p8psk_demap→...→
    rx_defragment）           rx_defragment）
         │                     │
   DMA1 S2MM              DMA2 S2MM
   result_qpsk             result_8psk
```

DMA1 和 DMA2 獨立讀同一份 DDR 資料，各自的 back-pressure **不會互相影響**。

**軟體判斷邏輯**：
```c
// 同時啟動 DMA1（QPSK chain）和 DMA2（8PSK chain）
// 等待結果：
// - 哪條 DMA 先完成（不 timeout）→ 那個模式是正確的
// - 也可進一步比較 BER 作為確認
```

**優點**：
- 無 back-pressure 死鎖
- 平行處理，延遲接近單條 chain
- 不需修改任何現有 IP

**缺點**：
- 硬體資源 ×2（需完整複製一條 RX chain）
- 需要 2 個額外 DMA instance（DMA1, DMA2）
- 連續串流需要 double buffer（ADC 寫 A 時，處理 B；ADC 寫 B 時，處理 A）

---

## 五、方案比較總覽

| 方案 | 延遲 | 硬體資源 | 需改 IP | back-pressure | 適合連續串流 |
|------|------|---------|---------|--------------|------------|
| AXI Broadcaster | ×1 | ×2 | 否 | ❌ 死鎖 | ❌ |
| Header 加 mode | ×1 | ×1 | ✅ 需改 | ✅ 無 | ✅ |
| DDR 讀兩次（序列）| ×2 | ×1 | 否 | ✅ 無 | ⚠️ 勉強 |
| DDR 讀兩次（平行）| ≈×1 | ×2 | 否 | ✅ 無 | ⚠️ 需 double buffer |

---

## 六、目前狀態

- **已驗證**：QPSK chain 完整鏈路（tx_frag→OFDM→rx_defragment），BER=0
- **已驗證**：模式不匹配實驗，BER=50%，timeout 行為符合預期
- **待決定**：最終採用哪個盲模式偵測方案
- **待實作**：選定方案後的 BD 修改與 main.c 更新

---

## 七、相關檔案

| 檔案 | 說明 |
|------|------|
| `C:\Vivado\test_Brian\` | 主 Vivado 專案（BD: bd_test） |
| `C:\Vitis\test_loop\app_loop\src\main.c` | 當前 Vitis 應用程式 |
| `C:\OFDM\TX\tx_fragmentation\tx_frag.cpp` | tx_frag IP 原始碼（新版，每 symbol TLAST）|
| `C:\OFDM\RX\rx_defragmentation\rx_defragment.cpp` | rx_defragment IP 原始碼 |
| `C:\VitisProject\Vitis\OFDM\` | 所有 HLS IP 原始碼 |
