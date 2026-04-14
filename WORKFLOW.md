# Vitis HLS 開發流程文檔

## 環境需求

| 工具 | 版本 / 路徑 | 安裝方式 |
|------|------------|---------|
| Xilinx Vitis HLS | 2022.2 → `C:/Xilinx/Vitis_HLS/2022.2/` | Xilinx Unified Installer |
| Xilinx Vivado | 2022.2 → `C:/Xilinx/Vivado/2022.2/` | 同上（與 Vitis HLS 一起裝） |
| Python | 3.x | `winget install Python.Python.3.12` |
| VS Code | 最新版 | https://code.visualstudio.com |
| Git Bash | 任意版本 | https://git-scm.com |

> **重要**：工作目錄路徑不能含空格。  
> 本專案根目錄：`C:\VitisProject`（原本 `Vitis Project` 因空格導致 HLS 編譯失敗，已改名）

---

## 專案結構

```
C:\VitisProject\
└── Vitis\
    ├── _template\              ← 每次新專案從這裡複製
    │   ├── .vscode\
    │   │   └── tasks.json      ← VS Code 任務設定
    │   ├── scripts\
    │   │   ├── create_project.tcl   ← 建立 HLS 專案
    │   │   ├── run_all.tcl          ← csim + syn + cosim + export
    │   │   ├── parse_report.py      ← 解析 synthesis report
    │   │   └── run_vivado_sim.tcl   ← 建立 Vivado 模擬專案
    │   ├── src\
    │   │   ├── top.cpp         ← HLS 演算法實作
    │   │   └── top.h           ← 型別定義、pragma 參數
    │   └── tb\
    │       ├── data\
    │       │   ├── input.dat   ← csim 輸入測試資料
    │       │   └── golden.dat  ← csim 黃金參考輸出
    │       ├── tb_top.cpp      ← HLS C Simulation testbench
    │       └── tb_vivado.v     ← Vivado RTL simulation testbench
    │
    └── <your_project>\         ← 從 _template 複製後的實際專案
```

---

## 新建專案步驟

### 1. 複製模板
```
複製整個 _template\ 資料夾，重新命名為你的專案名稱（不能有空格）
```

### 2. 修改 `scripts/create_project.tcl`
```tcl
set project_name  "your_project"   # 改成你的專案名稱
set top_func      "your_func"      # 改成 top function 名稱
set clock_period  "10"             # ns，視需求調整
```

> **注意**：`add_files` 和 `add_files -tb` 預設指向 `src/top.cpp` 和 `tb/tb_top.cpp`。  
> 若你的檔案不在這兩個位置（例如放在專案根目錄），**必須同時修改路徑和 `-I` include 路徑**：
> ```tcl
> # 範例：檔案放在根目錄
> add_files "$root_dir/ref_rx_packed.cpp" \
>     -cflags "-I$root_dir"
> 
> add_files -tb "$root_dir/tb_ref_rx_packed.cpp" \
>     -cflags "-I$root_dir" \
>     -csimflags "-I$root_dir"
> ```
> `run_all.tcl` 不需要改，它會自動 source `create_project.tcl`。

### 3. 修改 `src/top.h`
- 更新型別定義、`#define` 參數（DATA_WIDTH、N_SAMPLES 等）

### 4. 修改 `src/top.cpp`
- 實作演算法
- 設定 HLS pragmas（INTERFACE、PIPELINE、UNROLL 等）
- 所有真實專案使用 AXI-Stream 介面：
  ```cpp
  #pragma HLS INTERFACE ap_ctrl_none port=return
  #pragma HLS INTERFACE axis port=input_stream
  #pragma HLS INTERFACE axis port=output_stream
  ```

### 5. 修改 `tb/tb_top.cpp`
- 呼叫正確的 top function 名稱
- 填入測試資料與驗證邏輯

### 6. 修改 `.vscode/tasks.json`
- 找到 **HLS: Parse Report** 那個 task
- 把 `"hls_project"` 改成實際的 `project_name`

### 7. 修改 `tb/tb_vivado.v`
```verilog
// [修改 1] DUT module name
top dut ( ...          // "top" 換成你的 top function 名稱

// [修改 2] Port prefix（與 HLS stream 變數名一致）
.input_stream_TDATA    // 若 HLS 裡叫 "s_in"，就改成 .s_in_TDATA

// [修改 3] 測試資料
parameter TEST_LEN = 8;
test_data[0] = 8'hAA;  // 填入你的實際測試資料
```

---

## VS Code 開發任務

按 `Ctrl+Shift+P` → **Tasks: Run Task** 選擇：

| Task | 用途 | 執行時間 |
|------|------|---------|
| **HLS: Create Project** | 只建立 HLS 專案（不跑模擬） | 快 |
| **HLS: C Simulation only** | 只跑 csim，驗證 C 層級邏輯 | 快（幾十秒） |
| **HLS: Run All** | csim → synthesis → cosim → export | 慢（幾分鐘） |
| **HLS: Parse Report** | 顯示 latency / LUT / FF / DSP / BRAM | 瞬間 |
| **Vivado: Sim (Batch) → VCD** | 全自動跑 RTL sim，輸出 `output/sim.vcd` | 中等 |
| **Vivado: Sim (GUI)** | 開 Vivado GUI，手動加 signal 看波形 | 中等 |

---

## 開發循環

### 快速迭代（改演算法）
```
修改 src/top.cpp
    ↓
HLS: C Simulation only   → 看 [PASS] / [FAIL]
    ↓ PASS
HLS: Run All             → 確認 cosim PASS + 看資源報告
    ↓
HLS: Parse Report        → 確認 latency / LUT / FF
```

### RTL 波形驗證
```
HLS: Run All 完成後
    ↓
Vivado: Sim (Batch) → VCD    ← 全自動，用 WaveTrace / GTKWave 看
  或
Vivado: Sim (GUI)             ← 開 Vivado，手動加 signal 互動看波形
```

---

## 關鍵設定說明

### HLS 介面設定

| Pragma | 說明 |
|--------|------|
| `ap_ctrl_none` | 無 ap_start/done 控制介面，資料到就處理 |
| `axis` | AXI-Stream，適合串流資料處理 |
| `m_axi` | 記憶體映射，適合大量 DDR 存取（template 預設，實際改用 axis） |
| `PIPELINE II=1` | 每 cycle 處理一筆，最高吞吐量 |

### run_all.tcl 控制旗標

| 變數 | 預設值 | 說明 |
|------|-------|------|
| `RUN_CSIM` | 1 | C Simulation |
| `RUN_SYN` | 1 | C Synthesis |
| `RUN_COSIM` | 1 | RTL Co-Simulation（自動比對 RTL vs C 結果） |
| `RUN_EXPORT` | 1 | Export IP Catalog |
| `CLEAN_BUILD` | 1 | 每次 csim 都清乾淨重新編譯 |

支援 `-tclargs` 快速切換模式：
```
-tclargs csim_only   → 只跑 csim
-tclargs syn_only    → 只跑 synthesis
```

### csim 的 working directory 問題

csim 實際執行位置：`<proj>/<proj_name>/solution1/csim/build/`

所以 `tb_top.cpp` 讀取測試資料的路徑必須往上 4 層：
```cpp
#define INPUT_FILE  "../../../../tb/data/input.dat"   // 正確
#define INPUT_FILE  "../../tb/data/input.dat"          // 錯誤！
```

### Parse Report 注意事項

`tasks.json` 裡的 project name 參數必須跟 `create_project.tcl` 的 `project_name` 完全一致，否則找不到 report。

---

## 輸出檔案位置

| 檔案 | 路徑 | 說明 |
|------|------|------|
| HLS Log | `vitis_hls.log` | 完整執行 log |
| Synthesis Report | `<proj>/solution1/syn/report/*_csynth.rpt` | 資源與 timing 估算 |
| Report CSV | `output/report_summary.csv` | parse_report.py 輸出 |
| IP Export | `output/export.zip` | 給 Vivado Block Design 用的 IP |
| RTL 原始碼 | `<proj>/solution1/syn/verilog/*.v` | HLS 生成的 Verilog |
| Vivado 專案 | `vivado_sim/` | Vivado simulation 專案 |
| VCD 波形 | `output/sim.vcd` | Batch sim 輸出，用 WaveTrace/GTKWave 開 |

---

## 常見錯誤

| 錯誤訊息 | 原因 | 解法 |
|---------|------|------|
| `Cannot open ../../tb/data/input.dat` | csim 路徑層數錯誤 | 改成 `../../../../tb/data/` |
| `No synthesis reports found under: hls_project` | tasks.json project name 未改 | 改成實際 project_name |
| `Python was not found` | Python 未安裝 | `winget install Python.Python.3.12` |
| RTL 路徑空格導致 TCL 錯誤 | 路徑含空格 | 資料夾名稱不能有空格 |
| `Top function not found: 'xxx'` | `add_files` 指向錯誤的 .cpp，HLS 找不到 top function | 修改 `create_project.tcl` 的 `add_files` 路徑 |
| Run Task 沒有 HLS 選項出現 | VS Code workspace root 不是該專案資料夾 | File → Open Folder 選正確的子專案資料夾 |
