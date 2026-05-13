---
name: Viterbi 隔離測試計畫
description: solo_test Vivado 專案，enc→mapper→demap→dec，已完成驗證
type: project
originSessionId: 703681cb-f98f-4250-b1b3-77949fcac683
---
# Viterbi 隔離測試（✅ 2026-04-28 完成）

## 專案路徑
- Vivado：`C:\Vivado\solo_test`
- BD 名稱：`bd_solotest`（wrapper: `bd_solotest_wrapper`）
- Vitis Workspace：`C:\Vitis\test_solo`
- Application：`app_solo`，源碼：`C:\Vitis\test_solo\app_solo\src\main.c`

## 鏈路
```
DMA MM2S (8-bit, 306B)
  → axi_viterbi_encode_w_0
  → symbol_mapper_hls_0 (8PSK)
  → p8psk_demap_packed_0
  → Viterbi_decode_0
  → DMA S2MM (8-bit, 306B)
```

## 驗證結果
| 指標 | 結果 |
|------|------|
| Timing WNS | **+1.068 ns**（Vivado GUI），Failing Endpoints = 0 |
| HW BER | **0 / 2448 bits，PASS** |
| TX pattern | `(i*37+13)&0xFF`，306 bytes |

## 關鍵發現：新版 Viterbi_decode IP
- 來源：`C:\OFDM\Viterbi_decode`（由他人更新，Vitis HLS 2024.2，K=7 64-state）
- 舊版 WNS = -52.832 ns → 新版 WNS = +1.068 ns，改善約 54 ns
- **test_Brian 換入新 IP 後 timing 全部通過**

## Vitis 平台注意事項
- 新建 platform 時，standalone domain → BSP Settings → stdin/stdout 必須設為 `psu_uart_0`
- Clean Build 在 Windows 會報 `rm *.o` 錯誤（Makefile:42），直接 Build Project 即可

**Why:** 隔離測試證明 Viterbi 邏輯正確，timing 問題是舊 IP 實作問題，已透過新 IP 解決。
