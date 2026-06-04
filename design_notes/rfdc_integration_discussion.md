# RFDC 整合討論記錄
# 日期：2026-05-19（初版）/ 2026-05-20（更新：系統架構、PS 分工、訊號形態）

---

## 一、背景

目前 OFDM 系統是數位 loopback（TX→RX 在同一 FPGA 內部），沒有真實 RF 路徑。
下一步目標：連接公司的 Soliton 48DR RF 硬體，實現真實 RF 收發。

---

## 二、公司提供的資料

路徑：`C:\OFDM\training\`

| 檔案/資料夾 | 內容 |
|------------|------|
| `2026031701 48DR bring up.pdf` | 48DR 板子的硬體 bring-up 程序 |
| `zcu111-dds-ila-2020p2 (1).pdf` | ZCU111 參考設計（DDS + ILA loopback） |
| `RFSOC_3U_48DR_Release\RFSOC_3U.xpr` | **公司的完整 Vivado 專案（主要整合對象）** |
| `ip_repo\axi_spi_ctrl_1_0_enc\` | SPI 控制 IP（控制外部 PLL 用）|

---

## 三、RF 硬體架構

### 板子
- Soliton 48DR Board，使用 Zynq UltraScale+ **xczu48dr**（與 ZCU208 相同晶片）
- 8T8R，SMA 接頭

### 時鐘鏈路
```
25 MHz TCXO → LMK04832 → 122.88 MHz → LMX2594 PLL → 4915.2 MHz → ADC/DAC
```

### RFDC 核心參數

**DAC（TX 發射端）：**
| Tile | Channels | DAC Fs | Interpolation | Fabric Clock | NCO |
|------|----------|--------|--------------|-------------|-----|
| Tile0 (DAC228) | CH0, CH2 | 6.4 GSPS | 16× | 50 MHz (clk_dac0) | 1.6 GHz |
| Tile1 (DAC229) | CH0, CH2 | 5.89824 GSPS | 16× | 46.08 MHz (clk_dac1) | 1.8 / 1.9 GHz |
| Tile2 (DAC230) | CH0, CH2 | 5.89824 GSPS | 16× | — | — |
| Tile3 (DAC231) | CH0, CH2 | 5.89824 GSPS | 16× | — | — |

**ADC（RX 接收端）：**
| Tile | Channels | ADC Fs | Decimation | Fabric Clock | Mixer |
|------|----------|--------|------------|-------------|-------|
| Tile0 (ADC224) | CH0, CH1 | 2.94912 GSPS | 16× | 184.32 MHz | Coarse @ Fs/4 |
| Tile1~3 (ADC225~227) | 各 CH0, CH2 | 2.0 GSPS | — | — | — |

---

## 四、公司 Vivado BD（RfSoc_3U）結構

```
ARM Zynq PS
    │ AXI-Lite (100 MHz)
    ▼
AXI Interconnect
    ├──> RFDC IP（設定 ADC/DAC 參數）
    └──> axi_spi_ctrl（設定外部 LMX2594 PLL）

訊號路徑（TX）：
dds_compiler_0 ──> s00_axis（DAC Tile0 CH0）
dds_compiler_1 ──> s02_axis（DAC Tile0 CH2）
dds_compiler_2 ──> s10_axis（DAC Tile1 CH0）
dds_compiler_3 ──> s12_axis（DAC Tile1 CH2）
dds_compiler_4 ──> s20_axis（DAC Tile2 CH0）
dds_compiler_5 ──> s22_axis（DAC Tile2 CH2）
dds_compiler_6 ──> s30_axis（DAC Tile3 CH0）
dds_compiler_7 ──> s32_axis（DAC Tile3 CH2）

訊號路徑（RX）：
m00_axis（ADC Tile0 CH0）──> system_ila_0（只看波形）

xlconcat：匯集各 IP 的 interrupt → ARM IRQ
```

**整合方向**：
- 把 `dds_compiler_0` 換成 OFDM TX IP
- 把 `system_ila_0` 換成 OFDM RX IP
- 其他 7 個 DDS 暫時不動

---

## 五、AXI-Stream 介面規格（已確認）

從 XCI 直接讀出：
```
s00_axis_tdata[31:0]     ← 32-bit
TDATA_NUM_BYTES = 4      ← 4 bytes = 32-bit
```

**格式**：`{Q[31:16], I[15:0]}`（16-bit Q + 16-bit I，複數 I/Q）  
**DDS 輸出格式**：`{sine[31:16], cosine[15:0]}`（完全相同）  
**你的 IFFT 輸出格式**：`{Q[31:16], I[15:0]}`（相容 ✅）

---

## 六、Clock 分析

| 端 | Clock 來源 | 頻率 | 備註 |
|----|-----------|------|------|
| TX（DAC Tile0）| `clk_dac0`（RFDC 輸出）| 50 MHz | DDS 就接在這個 clock |
| RX（ADC Tile0）| ADC fabric clock | 184.32 MHz | = 2949.12 / 16 |
| 你的 IFFT（現在）| 系統 clock | 100 MHz | 需要換 clock 來源 |
| 你的 FFT（現在）| 系統 clock | 100 MHz | 需要換 clock 來源 |

---

## 七、整合時需要做的事

### 確定已知
1. **資料格式相容**：32-bit I/Q，完全一致 ✅
2. **TX 接法**：
   - IFFT m_axis → `axis_clock_converter`（100→50 MHz）→ RFDC s00_axis
   - 或直接把 IFFT 的 aclk 改接 `clk_dac0`（50 MHz），不需要 clock converter
3. **RX 接法**：
   - RFDC m00_axis → `axis_clock_converter`（184→100 MHz）→ FFT s_axis
   - 或把 FFT 的 aclk 改接 ADC fabric clock（184.32 MHz）

### 推薦方向（換 clock 來源）
整條 TX chain 接 `clk_dac0`（50 MHz），整條 RX chain 接 ADC fabric clock（184.32 MHz）。
這樣不需要 clock converter，但整條鏈路的 clock 都要重新規劃。

---

## 八、OFDM 頻率參數影響

換 clock 後，OFDM 的基帶頻率規格會改變：

| 參數 | 現在（loopback 100 MHz）| 接 RFDC TX（50 MHz）|
|------|------------------------|---------------------|
| Sample rate | 100 Msps | 50 Msps |
| Subcarrier spacing | 100M/2048 ≈ 48.8 kHz | 50M/2048 ≈ 24.4 kHz |
| 訊號帶寬（1632 subcarriers）| ≈ 80 MHz | ≈ 40 MHz |
| Center frequency | N/A（loopback）| 由 NCO 決定（1.6~1.9 GHz）|

RX 側 ADC 輸出 184.32 MHz → FFT 跑在 184.32 MHz，subcarrier spacing 又不一樣。  
**TX 和 RX sample rate 不一致**，是目前最大的疑點。

---

## 九、疑點與待確認項目

### ⚠️ 疑點 1：TX 和 RX Sample Rate 不一致
- TX（DAC fabric clock）= 50 MHz → 50 Msps
- RX（ADC fabric clock）= 184.32 MHz → 184.32 Msps
- 兩者相差 3.7×，OFDM 系統如何保持一致？

**可能解釋**：
- DAC 實際上是 8 samples/cycle（TDATA 可能是 256-bit，XCI 的 32-bit 只是單通道表示），因此 TX 實際 = 400 Msps
- 或者系統設計本來就讓 TX/RX 用不同 sample rate，由 CFO/STO 補償

**待確認**：向公司確認 s00_axis 的實際 TDATA 寬度（32-bit 還是 256-bit？）

### ⚠️ 疑點 2：整條 TX chain 換成 50 MHz 的影響
目前部分 HLS IP 跑在 250 MHz（symbol_mapper、reference_signal_tx）。  
降到 50 MHz 是否有 timing 問題？throughput 是否足夠？

### ⚠️ 疑點 3：兩個 Vivado 專案如何合併
你的 OFDM 鏈路在 `test_Brian`，RF 在 `RFSOC_3U_48DR_Release`。  
合併策略：把你的 HLS IP 加入 RFSOC_3U 的 BD，還是反過來？

### ⚠️ 疑點 4：軟體初始化
ARM 需要執行初始化序列（LMX2594 SPI 設定 → RFDC Master Reset → 等 Power-on step=0xF）。  
這個軟體在 `SolBoot_DMATest.c` 裡，需要整合進你的 `main.c`。

---

## 十、系統架構：PS 控制面 vs PL 資料面（2026-05-20）

### 核心概念

FPGA 系統中 PS 有兩種角色，**不能混淆**：

| 角色 | 介面 | 時脈 | 負責內容 |
|------|------|------|----------|
| 控制面 | AXI-Lite | 100 MHz | 啟動 IP、設定暫存器、初始化 RFDC/PLL |
| 資料面 | DMA + AXI-Stream | — | 搬運 baseband payload（274 B / frame） |

**RF 樣本（ADC/DAC 的 sample stream）不走 PS，全程在 PL 內流動。**

### 正確架構示意圖

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                              ZCU208 FPGA                                     │
│                                                                              │
│  ┌─────────────────── PS (ARM Cortex-A53) ─────────────────────────────┐    │
│  │                                                                       │    │
│  │  ① 控制面（AXI-Lite, 100 MHz）── 開機時執行一次                     │    │
│  │      設定 RFDC 暫存器、LMX2594 SPI、等 Power-on Step=0xF            │    │
│  │      設定各 HLS IP 啟動 / 模式切換                                   │    │
│  │                                                                       │    │
│  │  ② 資料面 Payload（DMA）── 每幀執行                                 │    │
│  │      TX DMA：送出 274 B payload → OFDM TX chain                     │    │
│  │      RX DMA：從 OFDM RX chain 收 274 B payload                      │    │
│  └──────┬──────────────────────────────────────────┬─────────────────── ┘    │
│         │ AXI-Lite（控制）                          │ AXI-Stream（payload）   │
│         ▼                                           ▼                        │
│  ┌─────────────────────────────────────────────────────────────────────┐     │
│  │            PL Fabric（OFDM chain + RFDC）                           │     │
│  │                                                                     │     │
│  │  TX:  payload ─► RS─►Viterbi─►mapper─►IFFT ──────────────────────► │     │
│  │                                                    RFDC s00_axis    │     │
│  │                                                    DAC Tile0 CH0    │     │
│  │                                                         │           │     │
│  │                                                    數位→類比         │     │
│  │                                                         ▼           │     │
│  │                                                    SMA TX (RF2) ───────► RF
│  │                                                                     │     │
│  │                                                    SMA RX (RF1) ◄──────  RF
│  │                                                         │           │     │
│  │                                                    類比→數位         │     │
│  │                                                    ADC Tile0 CH0    │     │
│  │                                                    RFDC m00_axis    │     │
│  │  RX:  payload ◄─ RS◄─Viterbi◄─demap◄─ref_rx◄─FFT ◄────────────── │     │
│  └─────────────────────────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 比較：學長組的 PS Buffer 中繼架構

學長組的 RF 單元是**外部模組**（獨立 RF chip 或另一塊板），PS 必須當中介搬資料：

```
PS Buffer 架構（外部 RF 模組）：
  OFDM TX → DMA → PS DDR → DMA → 外部 RF 模組 → 空中
  空中 → 外部 RF 模組 → DMA → PS DDR → DMA → OFDM RX

RFDC 架構（FPGA 內嵌硬核）：
  OFDM TX → RFDC DAC → SMA → 空中
  空中 → SMA → RFDC ADC → OFDM RX
```

**RFDC 是 AXI-Stream 介面，無 memory-mapped 模式，不能從 DDR 取樣本。**  
PS Buffer 中繼對 RFDC 不適用，但對外部 RF 模組是合理設計。

---

## 十一、訊號形態與 SMA 說明（2026-05-20）

### SMA 是什麼

SMA（SubMiniature version A）是板子上的**射頻同軸接頭**，用來接 RF 訊號線，等同於 USB 接口但用於 RF 訊號。板子上有多個 SMA，每個對應一個 DAC/ADC 通道。

```
ZCU208 / 48DR 板子：
  RFDC DAC ──► (O) SMA RF2 ──── 同軸線 ────► 天線 / 另一設備
  RFDC ADC ◄── (O) SMA RF1 ◄─── 同軸線 ─────

  Loopback 測試：RF2 直接接線到 RF1，不需要天線
```

### 訊號從數位到 RF 的形態轉換

```
數位域（FPGA PL）               類比 RF 域                 數位域（FPGA PL）
────────────────────────────────────────────────────────────────────────────

IFFT 輸出                    SMA TX 輸出後               SMA RX 輸入後
[0x1A3F, 0xC201, ...]       連續類比電壓波形              ADC 重新取樣
  16-bit 整數樣本             中心頻率 ~1.6 GHz            還原成數位樣本
  @ 184.32 MHz              在纜線 / 空氣中傳播            @ 184.32 MHz

     DAC 轉換 ──────────────────────────────────────────── ADC 取樣
    （數位→類比）           物理傳播（電磁波）              （類比→數位）
```

**SMA 輸出後，資料以類比電磁波形式存在，直到 RX SMA 進入 ADC 前都是類比訊號。**  
OFDM 資訊編碼在波形的振幅與相位中，途中遭受雜訊、多路徑、衰減都在這段。

### NCO 的作用（頻率上/下轉換）

OFDM IFFT 輸出是基頻訊號（中心 0 Hz），天線無法直接有效輻射。RFDC 內建 NCO（Numerically Controlled Oscillator）負責搬頻：

```
TX：基頻 OFDM（0 Hz 中心，±40 MHz 帶寬）
      ─── × NCO（1.6 GHz）───► RF 訊號（1.6 GHz 中心）─► DAC ─► SMA

RX：SMA ─► ADC ─► RF 樣本（1.6 GHz 中心）
      ─── × NCO（1.6 GHz）───► 基頻 OFDM（0 Hz 中心）─► FFT
```

NCO 設定由 PS 開機時透過 AXI-Lite 寫入，之後全程自動，PS 不再介入。

### DAC 需要 PS 控制嗎

**需要，但只在啟動時一次**，之後 DAC 自動從 AXI-Stream 取樣本轉成類比輸出：

```
上電 → PS 初始化（AXI-Lite）：
  1. 寫 LMX2594 SPI：PLL 鎖相到 4915.2 MHz
  2. 寫 RFDC 暫存器：設定 sample rate / mixer / NCO 頻率
  3. 等 Power-on Step = 0xF（tiles 就緒）

進入運作：
  OFDM TX chain 送樣本 → RFDC DAC 自動轉類比 → SMA 輸出
  （PS 不再介入，整條路自動運行）
```

---

## 十二、Clock Domain 概念與整合策略（2026-05-20）

### 什麼是 Clock Domain

FPGA 中每個暫存器必須由唯一一個時脈驅動。不同時脈的訊號互接稱為 **Clock Domain Crossing（CDC）**，若未處理會有 metastability（亂數據）問題。

```
❌ 直連不同 clock domain（錯誤）：
  OFDM DSP（100 MHz）──直接接──► RFDC（184.32 MHz）
  邊界取樣時序不確定 → 資料損毀

✅ 同一 clock domain（正確）：
  OFDM DSP（184.32 MHz）────────► RFDC（184.32 MHz）
  同一時脈，直接連接

✅ Clock Converter（折衷）：
  OFDM DSP（100 MHz）──► axis_clock_converter ──► RFDC（184.32 MHz）
  IP 內部處理 CDC，安全跨域
```

**100 MHz AXI-Lite clock 只給控制暫存器用，不能參與資料路徑。**  
（來自 ZCU111 教程 Section 8 明確指示）

### 整合方案比較

| 方案 | 做法 | 優點 | 代價 |
|------|------|------|------|
| **A（教程建議）** | 整條 OFDM chain 改接 184.32 MHz | 無 CDC 問題，不需 Clock Converter | HLS IP 全部重新 synthesis，timing 重驗 |
| **B（折衷）** | OFDM 保持 100 MHz，加 axis_clock_converter | HLS IP 不動 | 多一個 IP，須確認 converter 設定 |

**目前傾向方案 B**，待確認 Clock Converter 是否造成 OFDM timing 破壞。

---

## 十三、參考資料
- 討論摘要文件（NotebookLM 整理）：`C:\VitisProject\design_notes\rf_ip_connection_guide.md`
- NotebookLM notebook ID：`c22f280c-41bb-4c2c-bd8c-645d93a03949`（RF IP Connection Guide）
- 公司 Vivado 專案：`C:\OFDM\training\RFSOC_3U_48DR_Release\RFSOC_3U.xpr`
- 公司 SPI IP：`C:\OFDM\training\ip_repo\axi_spi_ctrl_1_0_enc\`
