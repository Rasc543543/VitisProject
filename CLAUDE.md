# CLAUDE.md — VitisProject 專案背景

> 這個檔案在每次 Claude Code session 開啟時自動讀取，提供跨機器的靜態背景知識。
> 動態記憶（設計進度、決策細節）存放在 `.claude/memory/`，需手動複製至本機 Claude 路徑。

---

## 使用者背景

- 碩一下通訊系統研究生，C/C++ 能力充足，有 Vitis HLS 使用經驗
- 這台電腦**專注於 Vitis HLS / Vivado 開發**，不做 RTL 練習
- 喜歡先理解「為什麼」再執行

**協作原則（重要）：**
- 回應精簡，結論先說，不重述已知資訊，不在結尾加摘要列表
- Vivado / Vitis 的 synthesis / simulation / implementation **由使用者自己動手**，Claude 準備好指令後等使用者說「你跑」才執行
- 失敗時使用者說「csim fail」→ Claude 直接 grep 對應 log 找 ERROR，不問「你看到什麼錯誤」

---

## 開發環境

| 項目 | 設定 |
|------|------|
| 工作目錄 | `C:\VitisProject`（路徑不含空格，Vitis HLS 要求）|
| Vitis HLS | 2022.2 → `C:/Xilinx/Vitis_HLS/2022.2/bin/vitis_hls.bat` |
| Vivado | 2022.2 → `C:/Xilinx/Vivado/2022.2/` |
| 目標 FPGA | xczu48dr-fsvg1517-2-e（ZCU208）|
| 時脈 | 10 ns（100 MHz），uncertainty 1.25 ns |

---

## 專案結構

```
C:\VitisProject\
├── Vitis\
│   ├── _template\          ← 每個新 HLS IP 從這裡複製
│   └── OFDM\
│       ├── TX\             ← TX 鏈路 HLS IP（RS, tx_frag...）
│       └── RX\             ← RX 鏈路 HLS IP（ref_rx_packed, RS dec...）
├── design_notes\           ← 設計討論文件（HTML/MD）
├── .claude\
│   └── memory\             ← 記憶檔備份（同步用，需手動複製至本機路徑）
└── CLAUDE.md               ← 本檔案
```

新 HLS 專案從 `_template\` 複製後，**必須修改** `scripts/create_project.tcl` 三處：
1. `set project_name` → 新專案名稱
2. `set top_func` → top function 名稱
3. `add_files` 路徑（若原始碼不在 `src/`）

**VS Code workspace 規則**：每個子專案要獨立 `Open Folder`，不能開上層目錄。

---

## OFDM 系統架構

### 系統規格
- FPGA：ZCU208，Loopback（TX→RX 同一板）
- FFT 大小：2048-point
- Pilot tones：273（間距 7），Guard：左 72 / 右 71
- Data tones：1632

### 資料量（一個 OFDM frame）

| 位置 | 8PSK | QPSK |
|------|------|------|
| CPU TX/RX | **274 B** | **188 B** |
| RS 編碼後 | 306 B | 204 B |
| Coded bits | 4896 bits | 3264 bits |
| OFDM 符號 | 1632 | 1632 |

### TX 鏈路
```
CPU (DMA) → tx_frag → energy_dispersal → RS enc → tx_interleaver_dynam
→ Viterbi enc (rate 1/2) → tx_interleaver64_dyn → symbol_mapper
→ tx_interleaver_float → reference_signal_tx → IFFT → [RF/Loopback]
```

### RX 鏈路（BD: bd_8psk_loop）
```
[RF/Loopback] → FFT → ref_rx_packed → rx_deinterleaver_flo
→ p8psk_demap_packed（或 qpsk_demap_packed）→ rx_deinterleaver64
→ Viterbi dec → rx_deinterleaver_dyn → RS dec → energy_descramble
→ rx_defragment → CPU (DMA)
```

**模式切換規則：**
- BD 用 `qpsk_demap_packed_0` → tx_frag mode=0（188B/frame，payload 179B）
- BD 用 `p8psk_demap_packed_0` → tx_frag mode=1（274B/frame，payload 265B）
- tx_frag 與 demap 不符 → RX chain 掛掉，DMA timeout

### RS 參數
- QPSK：RS(204,188)，T=8，51 zero padding
- 8PSK：2 × RS(153,137)，T=8，274B → 306B

---

## 關鍵 IP 說明

### ref_rx_packed（RX 通道等化主 IP）
- 路徑：`Vitis/OFDM/RX/Refer/ref_rx_packed.cpp`
- 輸入：FFT 輸出 fixed-point Q15；輸出：等化後 FP16 {Q[31:16], I[15:0]}
- CFO 掃描範圍：±70 subcarrier（141 個候選點）
- **已修正**：DC bin (pilot[136]) 跳過（Xilinx FFT IP DC 污染）
- **已實作**：CFO Tracking Mode（Locked 狀態只掃 ±5，11 點）

### CFO Tracking Mode 狀態（已完成驗證）
```
Unlocked：第一幀全掃 141 點，~428 µs，鎖定後進入 Locked
Locked  ：只掃 best_cfo ± 5（11 點），~33 µs max
boundary_hit：drift ≥ 5 → 下一幀解鎖重掃
```

**Hardware 量測結果（ZCU208 Loopback，2026-05-13）：**
| 模式 | Frame Time (Locked) | Data Rate (Locked) | BER |
|------|--------------------|--------------------|-----|
| QPSK | 620 µs | **2.30 Mbps** | 0 ✅ |
| 8PSK | 717 µs | **2.95 Mbps** | 0 ✅ |

**待驗證：** argmax-at-edge 假設（csim 注入 CFO > ±5）、Doppler 計算（需載波頻率 + subcarrier spacing）

### tx_frag / rx_defragment
- tx_frag：`C:\OFDM\TX\tx_fragmentation\tx_frag.cpp`，AXI-Lite mode 控制（base 0xA0010000）
- rx_defragment：`C:\OFDM\RX\rx_defragmentation2\rx_defragment.cpp`，ap_ctrl_none
- Header 9B（CPU 透明），payload 上限：QPSK 179B / 8PSK 265B

---

## Vivado 專案路徑

| 名稱 | 路徑 | 用途 |
|------|------|------|
| test_Brian | `C:\Vivado\test_Brian` | 主要硬體燒錄專案 |
| test_loopback | `C:\Vivado\test_loopback` | HLS IP 串接 Sim 驗證 |
| fft_loopback | `C:\Vivado\fft_loopback` | 全系統串接（含 RS/energy）|
| tx_loopback | `C:\Vivado\tx_loopback` | TX 端 IFFT 輸出 |
| rx_loopback_8psk | `C:\Vivado\rx_loopback_8psk` | RX 端 BD |

**TB 腳位規則（Vivado wrapper）：**
- 對應 `design_1_wrapper`（非 design_1）
- AXI-Stream 訊號加 `_0` suffix，全小寫：`input_stream_0_tdata`

---

## 上板應用程式
- 路徑：`C:\Vitis\test_loop\app_loop\src\main.c`
- 計時：`XTime_GetTime()` 包圍 start_tx_frag → RX DMA 完成（不含 UART printf）
- pkt[0] = Unlocked（第一幀全掃），pkt[1+] = Locked（追蹤模式）

---

## HLS Log 路徑（csim fail 時 grep 這裡）

| 階段 | 路徑 |
|------|------|
| csim | `<proj>/solution1/csim/report/<top>_csim.log` |
| synthesis | `<proj>/solution1/syn/report/<top>_csynth.rpt` |
| cosim | `<proj>/solution1/sim/report/<top>_cosim.rpt` |

csim testbench 讀檔路徑需上 4 層：`../../../../tb/data/input.dat`

---

## 設計文件（design_notes/）

| 檔案 | 內容 |
|------|------|
| `cfo_tracking_mode.md` | CFO Tracking Mode 完整設計記錄 |
| `cfo_tracking_discussion.html` | 設計假設、擔憂、完整量測結果（可直接瀏覽）|
| `cfo_tracking_flowchart.html` | 運作流程圖 |
| `cfo_tracking_results.html` | 量測結果總覽 |
| `blind_mode_detection.md` | QPSK/8PSK 自動識別設計討論 |

---

## 記憶檔同步說明

動態記憶（設計進度、feedback、決策）存放在 `.claude/memory/`（git 追蹤）。

**在新機器上啟用記憶的方式：**
```powershell
# 在專案根目錄執行（需依實際 username 修改）
.\setup_claude_memory.ps1
```
或手動複製：`.claude\memory\` → `C:\Users\<username>\.claude\projects\C--VitisProject\memory\`
