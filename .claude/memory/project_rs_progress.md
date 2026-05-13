---
name: RS Decoder RX 開發進度
description: RS Decoder (OFDM/RX/RS) 的當前開發狀態與待解決問題
type: project
originSessionId: 47690edc-2cde-4b74-a4cc-d5b12fea4ac5
---
**專案路徑**：`C:/VitisProject/Vitis/OFDM/RX/RS/`

## 驗證狀態：全部 PASS ✅

| 階段 | 結果 |
|---|---|
| Vitis csim 7/7 | PASS（2026-04-08）|
| Vitis cosim | PASS（2026-04-08）|
| Vivado sim | PASS，最後 byte = 0xBB（2026-04-09）|

---

## 檔案清單

| 檔案 | 說明 |
|---|---|
| `rs_decoder_rx.cpp / .h` | RS 解碼器主程式（Syndrome→BM→Chien→Forney）|
| `tb_rs_decoder.cpp` | Vitis csim TB，7 個測試案例 |
| `tb_golden_patterns.h` | TX csim 產出的 golden pattern（QPSK/8PSK）|
| `tb/tb_vivado.v` | Vivado sim TB，對應 design_1_wrapper |
| `review/code_review.md` | Code review 紀錄 |
| `scripts/create_project.tcl` | 建立 HLS 專案 |
| `scripts/run_all.tcl` | 執行 csim/syn/cosim/export |

---

## 設計決策

- **Uncorrectable 封包**：輸出 raw data（不丟棄），讓 downstream 計 BER
- **GF_EXP[255]=0x00**：正確，該位置從不被存取
- **BM_LOOP**：latency 608~1504 cycles，not pipelined（loop-carried dep，正常）

---

## Golden Pattern

來源：TX RS csim（`C:/VitisProject/Vitis/OFDM/TX/RS/`）

- **QPSK**：input 188B → codeword 204B，parity = `4F 29 DC 45 0E 4C 03 5B BA E8 93 84 03 00 E0 04`
- **8PSK**：input 274B → codeword 306B
  - chunk A parity = `2F 4F C1 11 11 C0 F3 91 BF D9 6B 54 22 9F 8C A4`
  - chunk B parity = `2A BB 64 F2 E0 C5 0F 86 94 00 52 58 C5 2A D6 85`

---

## TX RS Encoder 狀態（`C:/VitisProject/Vitis/OFDM/TX/RS/`）

- `src/rs_encoder_tx.h`：已有 `#include <stdint.h>`（舊 log 的 error 是過時記錄）
- `.vscode/tasks.json`：2026-04-15 已補齊
- csim：2026-04-15 進行中（編譯慢，正常，約 1~3 分鐘）
- synthesis：待確認

**How to apply:** 每次繼續前從這裡確認最新狀態。
