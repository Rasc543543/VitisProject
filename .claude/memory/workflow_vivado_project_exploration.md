---
name: Vivado 專案認識流程
description: 拿到一個新 Vivado 專案時，系統性認識架構的標準作業流程
type: feedback
originSessionId: f22d9495-941b-4005-8979-68562bfb925d
---
# Vivado 新專案認識標準流程

當使用者給我一個 Vivado 專案路徑，依序執行以下步驟，不要跳過。

**Why:** 使用者要求建立此流程，確保每次拿到新專案都能快速、完整地理解架構再開始 debug。
**How to apply:** 使用者說「去這裡看」或給我一個 Vivado 專案路徑時，執行此流程。

---

## Step 1：確認專案結構
```bash
ls /path/to/vivado_project/
```
確認有哪些資料夾：`.srcs`, `.gen`, `.sim`, `.runs`

---

## Step 2：找 Block Design（BD）
```bash
find .../xxx.srcs/sources_1/bd/ -name "*.bd"
```
BD 是整個系統架構的核心，必須先讀它。

---

## Step 3：從 BD JSON 解析 IP 清單與連線
```bash
grep -E "design_tree|interface_ports" xxx.bd
```
從 `design_tree` 取得所有 IP 名稱，從 `nets → interface_ports` 取得各 IP 之間的連線關係，畫出資料流方向。

格式：
```
IP_A/output_stream → IP_B/input_stream
```

---

## Step 4：找各 IP 的 RTL（確認 port 寬度）
```bash
find .../rx_loopback.gen -name "*.v" -path "*/ipshared/*/hdl/verilog/IP名稱.v"
```
對每個 IP 讀 top-level module 的 port 宣告：
- `input_stream_TDATA` 的位元寬（32-bit？8-bit？）
- 確認 TDATA 格式（FP16？fixed？packed I/Q？）

---

## Step 5：找 HLS 原始碼（確認輸入輸出數量）
```bash
find C:/VitisProject/Vitis/ -name "*.cpp" -o -name "*.h" | grep -v autopilot
```
從 `.cpp` 讀：
- loop 的迭代次數 → 輸入讀幾個 beat
- 輸出寫幾個 beat
- 定義檔（`.h`）的 `#define FRAME_XXX` 等常數

---

## Step 6：確認模擬 Log（如果有跑過 sim）
```bash
ls .../xxx.sim/sim_1/behav/xsim/
cat .../xvlog.log      # 編譯錯誤
cat .../elaborate.log  # Elaboration 錯誤（信號路徑不存在）
cat .../xsimkernel.log # Runtime 狀態
cat .../xsimcrash.log  # Crash 原因
```
優先看有無 ERROR。

---

## Step 7：讀 Testbench
```bash
find .../xxx.srcs/sim_1/ -name "*.sv" -o -name "*.v"
```
確認：
- DUT instance 名稱與 port 是否對應 BD wrapper
- 內部信號路徑（hierarchical path）是否正確
- 輸入資料來源（`$readmemh` 路徑）

---

## Step 8：整理並回報架構

格式化輸出：
1. **鏈路圖**（IP → IP，含資料寬度）
2. **各 IP 輸入輸出數量**（beats in / beats out）
3. **資料格式**（哪個 bits 是 I，哪個是 Q，FP16 還是 fixed）
4. **潛在問題點**（寬度不符、信號名稱錯誤、數量不對）

---

## 常見 Debug 模式

| 錯誤類型 | 查法 |
|---------|------|
| Syntax error | `xvlog.log` |
| 信號路徑不存在 | `elaborate.log` → 去 RTL 找正確名稱 |
| Simulation 跑完但結果錯 | 對比各 IP 輸入輸出數量，找哪段數量不對 |
| IP port 名稱 | 看 `xxx_stub.v` 或 `ipshared/hdl/verilog/xxx.v` 的 module 宣告 |
| BD 內部 hierarchy | `DUT.bd_xxx_i.ip_instance_name.inst.signal_name` |
