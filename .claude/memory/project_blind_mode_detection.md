---
name: 盲模式偵測設計討論
description: QPSK/8PSK 自動識別架構討論，從問題起源到 DDR 平行雙鏈路方案
type: project
originSessionId: b9a6cf40-630b-4335-b69d-3a275d982961
---
## 核心問題
TX 端可動態切換 QPSK/8PSK，RX 端的 demapper 是固定版本，RX 不知道當前 frame 是哪個模式。

## 關鍵知識
- **symbol_mapper 自動適應**：依輸入 bits 數自動切換，TX 鏈路不需額外控制
- **Demapper 是模式鎖點**：qpsk_demap / p8psk_demap 固定，模式不匹配 → BER=50%，rx_defragment timeout
- **AXI Stream Broadcaster 不適用**：錯誤模式 chain 卡住 → back-pressure → 拖死正確模式 chain → 死鎖

## 實驗驗證（2026-05-07）
8PSK mode 送進 QPSK chain → DMA timeout，BER=5999/12000≈50%，符合理論最大值

## 目前規劃方向：DDR 讀兩次（平行）
```
ADC DMA → DDR[A]
            ├── DMA1 MM2S → 完整 QPSK RX chain → DMA1 S2MM → result_qpsk
            └── DMA2 MM2S → 完整 8PSK RX chain → DMA2 S2MM → result_8psk
哪條 DMA 不 timeout → 那個模式正確
```
- 優點：無 back-pressure，平行處理
- 缺點：硬體資源 ×2，連續串流需 double buffer

## rx_defragment single-symbol bug（已修）
- 舊版：mode 判斷條件 `total_sym_cnt > 1 && sym_idx < total_sym_cnt - 1`，single-symbol 8PSK 永遠跳過 → `is_mode_274` 不更新 → payload 讀錯
- 新版（`C:\OFDM\RX\rx_defragmentation2`）：修正重點 3，last/only symbol 改用 `bytes_to_read = PAYLOAD_274`，靠 TLAST 煞車
- **硬體驗證（2026-05-08）**：8PSK 200B payload（num_syms=1），3/3 PASS，BER=0 ✅

## 設計文檔
完整記錄在 `C:\VitisProject\design_notes\blind_mode_detection.md`

**Why:** 系統需要在不知道 TX 模式的情況下自動識別 QPSK/8PSK
**How to apply:** 繼續討論此架構時，先讀設計文檔確認當前方向
