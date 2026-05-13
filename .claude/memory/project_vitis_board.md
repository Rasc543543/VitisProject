---
name: Vitis 上板應用程式碼進度
description: C:/Vitis/test_loop/app_loop 的裸機 AXI DMA 應用程式，完整 OFDM 鏈路 BER 測試
type: project
originSessionId: 703681cb-f98f-4250-b1b3-77949fcac683
---
# Vitis 上板專案：test_loop / test_solo

## 專案路徑
- 主應用 Workspace：`C:/Vitis/test_loop`
- Application：`app_loop`，源碼：`C:/Vitis/test_loop/app_loop/src/main.c`
- 隔離測試 Workspace：`C:/Vitis/test_solo`
- Application：`app_solo`，源碼：`C:/Vitis/test_solo/app_solo/src/main.c`
- 對應 XSA：`C:/Vivado/test_Brian`（主），`C:/Vivado/solo_test`（隔離）

## AXI DMA 配置（Simple Mode）
- MM2S TX：Stream Data Width = 8-bit
- S2MM RX：Stream Data Width = 8-bit
- TX_BUFFER_BASE：0x10000000，RX_BUFFER_BASE：0x11000000
- DMA_TIMEOUT：50,000,000 cycles

## 目前檔案狀態（2026-05-07）
- **main.c**：UDP_PAYLOAD=500，NUM_PKTS=3，TX_FRAG_MODE=1（8PSK），tx_frag AXI-Lite 控制（`start_tx_frag(mode)`，base=0xA0010000），pattern: `(i*37+13+pkt*0x5A)&0xFF`
- **tb_ip.v**：PKT_BYTES=188，NUM_PKTS=1，QPSK，output=`qpsk_1pkt_simulation.txt`
- **XSA 存檔**：`C:\VitisProject\xsa_archive\QPSK\bd_test_wrapper.xsa`（QPSK chain）

## 硬體驗證結果（2026-05-07）：Frag/Defrag 整合 ✅
| 設定 | 結果 |
|------|------|
| **QPSK（mode=0），UDP_PAYLOAD=500B，NUM_PKTS=3** | **BER=0/12000 bits，PASS** ← 已驗證 working config |
| fragments/pkt=3（各 188B），共 9 個 OFDM frame | 全部正確 |
| tx_frag→energy_dispersal→RS→Viterbi→OFDM→Viterbi→RS→descramble→rx_defragment | 端對端驗證完成 |
| 8PSK（mode=1），UDP_PAYLOAD=500B，BD 仍是 QPSK demap | RX DMA Timeout（rx_defragment 收到亂碼，永不輸出）|
| **8PSK（mode=1），UDP_PAYLOAD=500B，NUM_PKTS=3，BD 換 p8psk_demap** | **BER=0/12000 bits，PASS** ← 2026-05-07 驗證 |
| fragments/pkt=2（各 274B），共 6 個 OFDM frame | 全部正確，3 pkts 各 500B 獨立傳輸 |

## 硬體測試結果歷程
| 日期 | 版本 | PKT_BYTES | NUM_PKTS | WNS | BER | 結論 |
|------|------|-----------|----------|-----|-----|------|
| 2026-04-25 | 舊 Viterbi | 306 | 1 | -52.832 ns | 1191/2448 | ❌ timing violation |
| 2026-04-28 | 新 Viterbi（完整系統 8PSK）| 274 | 1 | +1.068 ns | 0/2192 | ✅ PASS |
| 2026-04-30 | 8PSK 多封包 | 274 | 10 | +0.778 ns | 0/21920 | ✅ PASS |
| 2026-04-30 | QPSK 單封包 | 188 | 1 | +0.518 ns | 0/1504 | ✅ PASS |
| 2026-04-30 | QPSK 多封包 | 188 | 10 | +0.518 ns | 0/15040 | ✅ PASS |

## Data Rate 量測（Sim，tlast-to-tlast，穩態 throughput）
| 模式 | 資料量 | ΔT | Data Rate |
|------|--------|----|-----------|
| 8PSK | 274 B (2192 bits) | 1,096.550 µs | ≈ 2.000 Mbps |
| QPSK | 188 B (1504 bits) | 1,001.040 µs | ≈ 1.502 Mbps |

## test_Brian Vivado 實作結果

### 8PSK（2026-04-30）
- WNS=+0.778 ns，WHS=+0.004 ns，WPWS=+3.500 ns，Failing Endpoints=0
- LUT: 61,239/425,280 (14.40%)，FF: 57,193/850,560 (6.72%)
- RAMB36: 27/1,080 (2.50%)，RAMB18: 15/2,160 (0.69%)，DSP48E2: 73
- CLB: 17,104/53,160 (32.17%)
- CSV：`C:\測試記錄\8PSK\timing_report.csv`、`C:\測試記錄\8PSK\utilization_report.csv`

### QPSK（2026-04-30）
- WNS=+0.518 ns，WHS=+0.009 ns，WPWS=+3.500 ns，Failing Endpoints=0
- LUT: 59,640/425,280 (14.02%)，FF: 57,575/850,560 (6.77%)
- RAMB36: 27/1,080 (2.50%)，RAMB18: 15/2,160 (0.69%)，DSP48E2: 73
- CLB: 16,954/53,160 (31.89%)
- CSV：`C:\測試記錄\QPSK\timing_report.csv`、`C:\測試記錄\QPSK\utilization_report.csv`

## Sim 與 HW 比對
- 8PSK 274B 逐 byte 完全一致（`C:\VitisProject\sim_hw_compare.txt`）

## Vitis 平台注意事項
- 新建 platform 時：standalone domain → BSP Settings → stdin/stdout 設為 `psu_uart_0`
- Clean Build Windows 錯誤（Makefile:42 `rm *.o`）：直接 Build Project 跳過 clean

**Why:** 記錄上板測試狀態與完整系統驗證歷程。
**How to apply:** 繼續上板作業時確認 PKT_BYTES，chain 正確，Build → 燒錄。
