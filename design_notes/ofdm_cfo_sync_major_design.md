# ofdm_cfo_sync cp_major 架構設計文件

> 版本：cp_major（combined_correction）  
> 路徑：`C:\VitisProject\Vitis\OFDM\RX\cp_major\ofdm_cfo_sync.cc`  
> 狀態：Vivado Sim PASS（5/5 封包，無 deadlock，2026-05-29）

---

## 1. 原版架構（Original / cp 目錄）

### 1.1 DATAFLOW 五個 Stage

```
unpack_axis
    │ stream_data, stream_clear_gate
    ▼
calc_power_and_corr
    │ stream_power, stream_corr1/2/3
    │ stream_data_bypass (x[k-2048])
    ▼
moving_sum_and_metric
    │ stream_payload  (depth=4096, 但 HLS 實際合成為 depth=2 ← Bug)
    │ stream_locked_corr (depth=4)
    ▼
cfo_estimator
    │ stream_phase_inc (depth=4)
    ▼
cfo_correction
    │
    ▼
output_stream
```

### 1.2 各 Stage 角色

| Stage | 職責 | 問題 |
|-------|------|------|
| unpack_axis | AXI-S 拆包，分離 I/Q | 無 |
| calc_power_and_corr | 計算瞬時功率、三組相關 c1/c2/c3，輸出 x[k-2048] bypass | 無 |
| moving_sum_and_metric | **雙重職責**：①滑動窗 S&C 峰值偵測 ②FSM 控制 payload 輸出 | 控制路徑與資料路徑耦合 |
| cfo_estimator | 三次 atan2f → phase_inc | 無 |
| cfo_correction | 用 phase_inc 對 payload 做相位旋轉（LUT 查表） | **在 II=1 pipeline 內做條件式 FIFO read** |

### 1.3 原版死鎖根因

**層次一：cfo_correction 條件式讀取（直接原因）**

```cpp
while (true) {
#pragma HLS PIPELINE II=1   // ← II=1 pipelined
    internal_axis_t pkt = in_payload.read();      // 無條件讀
    if (new_symbol) {
        cfo_t phaseInc = in_phase_inc.read();     // ← 條件式讀！
    }
}
```

HLS RTL 對 II=1 pipeline 採保守策略：stall 條件 = `stream_payload.empty OR stream_phase_inc.empty`。即使 `new_symbol=false`，只要 stream_phase_inc 空，整條 pipeline 就卡住。

**死鎖三角形（Vivado Sim 驗證）：**

```
cfo_correction  ──等 stream_phase_inc (空)──→  cfo_estimator
     │                                              │
     │ stream_payload 不被讀取                     等 stream_locked_corr (空)
     ▼                                              │
  stream_payload 滿 (depth=2)  ←── moving_sum 寫入 ←──┘
     │
  moving_sum 被背壓鎖住 → 無法繼續 → stream_locked_corr 永遠空
```

**層次二：stream_payload depth=2（加速器）**

`#pragma HLS STREAM depth=4096` 被 HLS 覆蓋為 depth=2，只需 2 個樣本就能觸發 moving_sum 背壓。

---

## 2. cp_major 架構

### 2.1 DATAFLOW 五個 Stage（重新設計）

```
unpack_axis
    │ stream_data, stream_clear_gate
    ▼
calc_power_and_corr2          (改版：新增 2-tap delay 輸出)
    │ stream_power, stream_corr1/2/3
    │ stream_data_payload (x[k-2050])  ← 新增，取代 stream_data_bypass
    ▼                    ▼
sync_search          payload_gate      (新增 stage)
    │                    │
    │ stream_locked_corr │ stream_payload (depth=512, BRAM)
    │ stream_trigger ────┘ (觸發 payload_gate 開始輸出)
    ▼
combined_correction               (合併 cfo_estimator + cfo_correction)
    │
    ▼
output_stream
```

### 2.2 各 Stage 角色

| Stage | 職責 | 特性 |
|-------|------|------|
| unpack_axis | 同原版 | 不變 |
| calc_power_and_corr2 | 同原版計算，但輸出 x[k-2050] 而非 x[k-2048] | 2-tap shift reg 延遲 |
| sync_search | **只做峰值偵測**，不輸出 payload | 移除 STATE_OUTPUT_PAYLOAD，加 post_detection 靜默期 |
| payload_gate | **無條件讀** stream_data_payload，`read_nb()` 非阻塞偵測 trigger | 永遠不會讓 stream_data_payload 填滿 |
| combined_correction | 讀 corr → atan2f → inner for-loop II=1 讀 payload | 無中間 FIFO，消滅死鎖根源 |

---

## 3. 逐項修改對比

### 3.1 Stage 對應關係

| 原版 Stage | cp_major Stage | 變化 |
|-----------|----------------|------|
| unpack_axis | unpack_axis | **不變** |
| calc_power_and_corr | calc_power_and_corr2 | **修改**：改輸出 x[k-2050] |
| moving_sum_and_metric | sync_search | **重構**：移除 payload 輸出，改輸出 trigger |
| ─（無）─ | payload_gate | **新增**：資料閘門，read_nb 觸發 |
| cfo_estimator | ─（移除）─ | **合併**入 combined_correction |
| cfo_correction | combined_correction | **合併** cfo_estimator + cfo_correction |

### 3.2 FIFO 對比

| FIFO | 原版 | cp_major | 說明 |
|------|------|----------|------|
| stream_data | depth=256 | depth=256 | 不變 |
| stream_clear_gate | depth=256 | depth=256 | 不變 |
| stream_data_bypass | depth=256 | **移除** | 改為 stream_data_payload |
| stream_data_payload | 無 | **新增** depth=256 | x[k-2050]，給 payload_gate |
| stream_power | depth=256 | depth=256 | 不變 |
| stream_corr1/2/3 | depth=256 | depth=256 | 不變 |
| stream_payload | depth=4096（實際=2）| **depth=512 BRAM** | 夠 atan2f latency=71 cycles 緩衝 |
| stream_locked_corr | depth=4 | depth=4 | 不變 |
| stream_phase_inc | depth=4 | **移除** | 消滅死鎖根源 |
| stream_trigger | 無 | **新增** depth=4 | sync_search → payload_gate |

### 3.3 calc_power_and_corr → calc_power_and_corr2

**原版**：
```cpp
out_data_bypass.write(x_n_2048);  // x[k-2048]
```

**cp_major**：
```cpp
// 2-tap shift register
complex_data_t x_payload = delay_payload[1];
delay_payload[1] = delay_payload[0];
delay_payload[0] = x_n_2048;          // 每次 push
out_data_payload.write(x_payload);    // 輸出 x[k-2050]
```

**原因**：當 sync_search 在 k=2306 偵測到峰值並寫入 trigger，payload_gate 在同一個 cycle 讀到的 stream_data_payload 值必須是 x[256]=Data[0]。

計算：
- 峰值在 k=2306
- 需要 x[256] = x[2306-2050]
- 所以需要 delay = 2050，即 2-tap 額外延遲（2048 + 2 = 2050）

### 3.4 moving_sum_and_metric → sync_search

**移除**：
- `STATE_OUTPUT_PAYLOAD`（整個狀態移除）
- `out_payload.write(...)` 的呼叫
- `counter`、`x_n_m1`、`x_n_m2`、`x_n_m3`、`held_sample` 等變數
- `in_data` 讀取（不需要 x[k-2048] 資料）

**保留**：
- 完整滑動窗 S&C metric 計算（acc_pwr, acc_corr1/2/3, delay_pwr/corr, ptr_sum, search_warmup）
- 峰值判斷邏輯（warmup_done, is_true_peak, metric_m0~m3, thresh_m2/m3）
- 偵測到峰值時寫 `out_corr`

**新增**：
```cpp
out_trigger.write(true);  // 觸發 payload_gate
```

**新增 post_detection 靜默期**（防止誤觸）：
```cpp
static bool        post_detection = false;
static ap_uint<12> post_counter   = 0;

if (is_true_peak) {
    out_corr.write(corr_val);
    out_trigger.write(true);
    post_detection = true; post_counter = 0;
    // 重置所有 metric
} else if (post_detection) {
    // 靜默 FFT_LEN=2048 cycles：繼續讀輸入但不計算 metric
    // 靜默期結束後重置 acc_pwr/corr, search_warmup, ptr_sum
}
```

**讀取介面簡化**：
- 原版：讀 5 條 stream（in_data + in_power + in_corr1/2/3）
- cp_major：讀 4 條 stream（in_power + in_corr1/2/3，**不讀** in_data）

### 3.5 新增 payload_gate

```cpp
static void payload_gate(
    hls::stream<complex_data_t>  &in_data_payload,  // x[k-2050]，永遠消耗
    hls::stream<bool>            &in_trigger,        // read_nb，非阻塞
    hls::stream<internal_axis_t> &out_payload)
{
    static bool        outputting = false;
    static ap_uint<12> cnt        = 0;

    while (true) {
#pragma HLS PIPELINE II=1
        complex_data_t x = in_data_payload.read();  // ← 無條件讀，永不阻塞 upstream

        if (!outputting) {
            bool trig;
            if (in_trigger.read_nb(trig))            // ← 非阻塞，不影響 II=1
                { outputting = true; cnt = 0; }
        }

        if (outputting) {
            // 寫出一個 sample
            if (cnt == FFT_LEN - 1) { outputting = false; cnt = 0; }
            else cnt++;
        }
    }
}
```

**關鍵設計點**：
- `in_data_payload.read()` **無條件**執行：calc_power_and_corr2 永遠有人消耗，不會背壓堵塞
- `in_trigger.read_nb()` **非阻塞**：不滿足 II=1 的 stall 條件，stream_trigger 空也不卡
- `out_payload.write()` **有條件**：只在 outputting=true 時寫，conditional write 不影響 II=1

### 3.6 cfo_estimator + cfo_correction → combined_correction

**原版**（兩個獨立 Stage，中間有 stream_phase_inc）：

```
cfo_estimator：
    while(true) {
        corr = in_corr.read()
        phi1/2/3 = atan2f(...)    // 71 cycles
        out_phase_inc.write(phase_inc)
    }

cfo_correction：
    while(true) {
#pragma HLS PIPELINE II=1
        pkt = in_payload.read()   // 無條件
        if (new_symbol)
            phaseInc = in_phase_inc.read()  // ← 條件式，死鎖來源
    }
```

**cp_major**（合併成一個 Stage）：

```cpp
combined_correction：
    while(true) {
        corr = in_corr.read()          // outer: 等待下一幀 corr（blocking）
        phi1/2/3 = atan2f(...)         // outer: 計算 71 cycles（非 pipeline）
        // 計算 phase_inc, phase_inc_fixed, phase_acc

        for (int i = 0; i < FFT_LEN; i++) {
#pragma HLS PIPELINE II=1
            pkt = in_payload.read()    // inner: 只讀 payload，無條件
            // 相位旋轉 + 輸出
        }
    }
```

**消滅死鎖的原因**：
- 不再有 `stream_phase_inc` FIFO
- `in_corr.read()` 在 outer while（非 pipeline），blocking 不影響 inner for-loop
- inner for-loop II=1 只讀 `in_payload`，stall 條件只有 `stream_payload.empty OR output.TREADY=0`
- 無任何兩個 FIFO 的 AND 條件 → 死鎖三角無法形成

**為什麼之前 Fix 1（只改 cfo_correction 結構）仍然死鎖**：

Fix 1 之後，outer while 等 `in_phase_inc`（blocking），stream_payload 此時無人消耗 → 填滿。但 `in_phase_inc` 由 `cfo_estimator` 提供，`cfo_estimator` 又等 `stream_locked_corr`，`stream_locked_corr` 由 `moving_sum` 提供，`moving_sum` 被 `stream_payload` 背壓卡住 → 相同死鎖三角，只是觸發點換了。

合併後，outer while 等 `stream_locked_corr`（不是 phase_inc），但此時 stream_payload 是由 `payload_gate` 提供。`payload_gate` 的 II=1 loop 永遠消耗 `stream_data_payload`，所以 `calc_power_and_corr2` 不會背壓，`sync_search` 能持續工作，`stream_locked_corr` 終究會有資料。**沒有循環依賴**。

---

## 4. 架構差異總覽

| 項目 | 原版 | cp_major |
|------|------|----------|
| DATAFLOW stages 數 | 5 | 5（不同配置）|
| 死鎖風險 | 有（三角循環依賴）| 無 |
| stream_phase_inc | 有（死鎖根源）| **移除** |
| stream_data_bypass | 有 | **移除**（改為 stream_data_payload）|
| stream_trigger | 無 | **新增** |
| stream_data_payload | 無 | **新增**（x[k-2050]）|
| stream_payload depth | 4096（實際 2） | **512 BRAM** |
| moving_sum 職責 | 峰值偵測 + payload 輸出 | **只**峰值偵測 |
| payload 輸出方式 | moving_sum FSM 直接寫 | payload_gate 用 read_nb 閘控 |
| CFO 估測與補償 | 分兩個 Stage | **合一** Stage |
| inner for-loop stall 條件 | `payload.empty OR phase_inc.empty` | **只** `payload.empty OR output.TREADY=0` |

---

## 5. 驗證結果（2026-05-29）

### IP_solo_test Vivado Simulation

| 封包 | IN sent | OUT complete | 延遲 |
|------|---------|--------------|------|
| pkt[0] | 46320 ns | 87615 ns | 41295 ns |
| pkt[1] | 92400 ns | 133135 ns | 40735 ns |
| pkt[2] | 138480 ns | 179395 ns | 40915 ns |
| pkt[3] | 184560 ns | 225855 ns | 41295 ns |
| pkt[4] | 230640 ns | 271355 ns | 40715 ns |

- **5/5 封包全部完整輸出** ✅
- 延遲穩定 ~41000 ns ≈ atan2f(71 cycles) + FFT_LEN(2048 cycles) × 10 ns
- 無任何 DEADLOCK 訊息

### TB 注意事項

1. **Timeout**：改為 `#5_000_000_000`（5 秒），原公式低估 send_word 開銷（每 sample 2 cycles）
2. **Filler samples**：最後一個 packet 送完後，需再送 `FFT_LEN=2048` 個 filler（32'd0），讓 `sync_search` 能完成最後一幀的 `STATE_OUTPUT_PAYLOAD`（因為 sync_search 在任何狀態都讀 input stream）
3. **Summary 設計**：所有結果在模擬結束時統一輸出，避免被 log 洗掉

---

## 6. 已知待修項目

| 問題 | 影響 | 修法 |
|------|------|------|
| AP_TRN 截斷不對稱 | positive CFO MSE > 0.005 | `phase_acc` / `phase_inc_fixed` 改為 `ap_fixed<32,1>` |
| payload_gate 2-tap 對齊在硬體上的 FIFO latency | cosim 可能需微調 delay 數量 | 待 cosim 驗證 |
| test_loopback 接入全 RX 鏈後仍解不出 | 完整系統驗證未過 | 先做 cp_inserter → ofdm_cfo_sync 兩 IP loopback |
