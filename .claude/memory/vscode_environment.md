---
name: Vitis HLS 開發環境
description: 這台電腦的 VS Code + Vitis HLS 環境設定
type: project
---

**這台電腦專用：Vitis HLS 自動化開發**

- 工作目錄：C:\VitisProject（原本是 "Vitis Project"，因路徑含空格導致 HLS 編譯錯誤，改名為 VitisProject）
- Vitis HLS 路徑：C:/Xilinx/Vitis_HLS/2022.2/bin/vitis_hls.bat
- 目標 FPGA：xczu48dr-fsvg1517-2-e
- 時脈：10 ns（uncertainty 1.25 ns）

**專案結構：**
- `Vitis/_template/` — 標準模板（每個新專案從這裡複製）
- `Vitis/example_add1/` — 第一個範例專案

**模板包含：**
- `scripts/create_project.tcl` — 建立 HLS 專案
- `scripts/run_all.tcl` — csim → syn → export 一鍵執行（cosim 預設關閉）
- `scripts/parse_report.py` — 解析 csynth report，輸出表格＋CSV
- `src/top.cpp / top.h` — 主程式（ap_int, AXI m_axi + s_axilite 介面）
- `tb/tb_top.cpp` — testbench（讀 input.dat / golden.dat 比對）
- `.vscode/tasks.json` — VS Code 任務（Create Project / Run All / CSim / Parse Report）

**新專案從模板複製後，必須修改 `create_project.tcl` 以下三處：**
1. `set project_name` → 改成新專案名稱
2. `set top_func` → 改成實際 top function 名稱
3. `add_files` → 改成實際的 .cpp 路徑（預設指向 `src/top.cpp`，若檔案不在 src/ 要改）
4. `add_files -tb` → 改成實際的 TB .cpp 路徑（預設指向 `tb/tb_top.cpp`）
   - `-cflags` / `-csimflags` 的 `-I` include 路徑也要一起對齊

`run_all.tcl` 不需要改，它 source create_project.tcl 自動繼承設定。

**VS Code workspace 規則：每個子專案要獨立 Open Folder（不能開上層），`${workspaceFolder}` 才會正確。**

**Why:** 這台電腦只做 Vitis HLS 自動化，不做 RTL 練習或碩論寫作。
**How to apply:** 對話只需關注 HLS 流程，不需提及 RTL 或另一台電腦。
