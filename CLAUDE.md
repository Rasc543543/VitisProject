# CLAUDE.md — VitisProject 專案背景

**協作原則：**
- 回應精簡，結論先說，不重述已知資訊，不在結尾加摘要列表
- Vivado / Vitis synthesis/simulation/implementation 由使用者自己動手，Claude 準備好指令後等使用者說「你跑」才執行
- 失敗時使用者說「csim fail」→ Claude 直接 grep 對應 log 找 ERROR，不問「你看到什麼錯誤」

---

## 開發環境

| 項目 | 設定 |
|------|------|
| 工作目錄 | `C:\VitisProject` |
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
│   └── OFDM\TX\ / RX\
├── design_notes\
└── .claude\memory\
```

新 HLS 專案從 `_template\` 複製後，修改 `scripts/create_project.tcl` 三處：`set project_name`、`set top_func`、`add_files` 路徑。

**VS Code workspace：** 每個子專案要獨立 `Open Folder`，不能開上層目錄。

---

## OFDM 系統架構

- FFT 大小：2048-point；Pilot：273（間距 7）；Data：1632；Guard：左 72 / 右 71
- 8PSK：274B/frame；QPSK：188B/frame

### TX 鏈路
```
CPU→tx_frag→energy_dispersal→RS enc→tx_interleaver_dynam
→Viterbi enc→tx_interleaver64_dyn→symbol_mapper
→tx_interleaver_float→reference_signal_tx→IFFT→[RF]
```

### RX 鏈路（BD: bd_8psk_loop）
```
[RF]→FFT→ref_rx_packed→rx_deinterleaver_flo
→demap_packed→rx_deinterleaver64→Viterbi dec
→rx_deinterleaver_dyn→RS dec→energy_descramble→rx_defragment→CPU
```

**模式切換：** BD 用 `qpsk_demap_packed_0` → tx_frag mode=0（188B）；BD 用 `p8psk_demap_packed_0` → tx_frag mode=1（274B）。不符則 DMA timeout。

---

## HLS Log 路徑（csim fail 時 grep 這裡）

| 階段 | 路徑 |
|------|------|
| csim | `<proj>/solution1/csim/report/<top>_csim.log` |
| synthesis | `<proj>/solution1/syn/report/<top>_csynth.rpt` |
| cosim | `<proj>/solution1/sim/report/<top>_cosim.rpt` |

csim testbench 讀檔路徑：`../../../../tb/data/input.dat`

---

## 記憶檔同步

動態記憶在 `.claude/memory/`（git 追蹤）。新機器執行 `.\setup_claude_memory.ps1` 複製到本機 Claude 路徑。
