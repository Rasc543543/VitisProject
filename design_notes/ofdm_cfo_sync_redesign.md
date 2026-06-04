# ofdm_cfo_sync 重設計規劃

## 1. 現有架構問題分析

### 1.1 DATAFLOW 五個 stage

```
unpack_axis → calc_power_and_corr → moving_sum_and_metric → cfo_estimator → cfo_correction
```

| Stage | 角色 | 問題 |
|-------|------|------|
| unpack_axis | AXI-Stream 拆包 | 無 |
| calc_power_and_corr | 計算延遲、相關性乘積 | 無 |
| moving_sum_and_metric | 滑動窗口 + FSM + 資料寫入 stream_payload | **混用兩個職責** |
| cfo_estimator | atan2f 計算 phase_inc | 無 |
| cfo_correction | 查 LUT 做相位旋轉 | **II=1 pipeline 內有條件式 FIFO 讀取** |

### 1.2 死鎖根因

`cfo_correction` 在 `#pragma HLS PIPELINE II=1` 的迴圈內做條件式讀取：

```cpp
while (true) {
#pragma HLS PIPELINE II=1
    internal_axis_t pkt = in_payload.read();   // 無條件
    if (new_symbol) {
        cfo_t phaseInc = in_phase_inc.read();  // 條件式 ← 問題所在
        ...
    }
}
```

觸發序列（Vivado Sim 驗證）：
1. `moving_sum` 偵測 peak → 同一 cycle 寫 `stream_locked_corr` + `stream_payload[0]`
2. `cfo_estimator` 讀走 locked_corr，開始 atan2f（約 200 cycles）
3. `cfo_correction` 讀走 payload[0]，new_symbol=1 → 等 phase_inc → BLOCK
4. `moving_sum` 寫 payload[1] → depth=2 FIFO 滿 → BLOCK
5. `cfo_estimator` 算完 → 寫 phase_inc → `cfo_correction` 解鎖
6. 但 payload[1] 寫入後 `moving_sum` 被背壓鎖住直到 depth=2 解除

---

## 2. 兩個問題的本質

**問題 A（架構）：** `moving_sum_and_metric` 同時負責「搜尋 peak」和「把資料寫進 stream_payload」，這兩件事性質不同（控制路徑 vs 資料路徑）卻耦合在同一個 FSM 裡。

**問題 B（實作）：** `cfo_correction` 在 II=1 pipelined loop 內做條件式 FIFO 讀取，RTL stall 條件依賴 stream_phase_inc 是否為空，導致死鎖。

---

## 3. 修改規劃

### Fix 1：cfo_correction 結構重寫（必要，解死鎖）

**原則**：把 phase_inc 的讀取移到 outer loop（非 pipeline），inner loop 只讀 stream_payload。

```cpp
static void cfo_correction(
    hls::stream<internal_axis_t> &in_payload,
    hls::stream<cfo_t>           &in_phase_inc,
    hls::stream<axis_word>       &out_axis)
{
    // Outer loop：每個封包讀一次 phase_inc（blocking，非 pipelined）
    cfo_t phaseInc = in_phase_inc.read();

    ap_fixed<16,1,AP_TRN,AP_WRAP> phase_inc_fixed =
        (ap_fixed<16,1,AP_TRN,AP_WRAP>)(phaseInc * 0.3183098861837907f);

    // 初始相位：補償 Data[0] 在封包內的絕對位置（x[256]）
    // phase_acc 初始化為 255 × phase_inc，加上迴圈第一次的 +=，
    // 第 0 筆輸出的相位 = 256 × phase_inc（對應 x[256]）
    ap_fixed<16,1,AP_TRN,AP_WRAP> phase_acc =
        (ap_fixed<16,1,AP_TRN,AP_WRAP>)(phase_inc_fixed * (ap_fixed<16,9>)255);

    // Inner loop：處理一個封包的 FFT_LEN 筆資料，II=1
    for (int i = 0; i < FFT_LEN; i++) {
#pragma HLS PIPELINE II=1
        internal_axis_t pkt = in_payload.read();   // 只讀 payload，無條件

        phase_acc += phase_inc_fixed;

        ap_uint<8> idx     = phase_acc.range(15, 8);
        ap_uint<8> cos_idx = idx + 64;

        data_t sin_val = sin_lut[idx];
        data_t cos_val = sin_lut[cos_idx];

        data_t x_i, x_q;
        x_i.range(15,0) = pkt.data.range(15, 0);
        x_q.range(15,0) = pkt.data.range(31,16);

        data_t mul_i_cos = (data_t)(x_i * cos_val);
        data_t mul_q_sin = (data_t)(x_q * sin_val);
        data_t mul_i_sin = (data_t)(x_i * sin_val);
        data_t mul_q_cos = (data_t)(x_q * cos_val);

#pragma HLS BIND_OP variable=mul_i_cos op=mul impl=dsp
#pragma HLS BIND_OP variable=mul_q_sin op=mul impl=dsp
#pragma HLS BIND_OP variable=mul_i_sin op=mul impl=dsp
#pragma HLS BIND_OP variable=mul_q_cos op=mul impl=dsp

        data_t out_i = (data_t)(mul_i_cos - mul_q_sin);
        data_t out_q = (data_t)(mul_i_sin + mul_q_cos);

        axis_word out_pkt;
        out_pkt.data.range(15, 0)  = out_i.range(15,0);
        out_pkt.data.range(31,16)  = out_q.range(15,0);
        out_pkt.keep = 0xF;
        out_pkt.strb = 0xF;
        out_pkt.last = pkt.last ? 1 : 0;
        out_axis.write(out_pkt);
    }
}
```

**關鍵變更：**
- 移除 `static bool new_symbol`
- 移除外層 `while(true)`（頂層 `ap_ctrl_none` 自動重啟，outer loop 本身就是「每封包一次」）
- `phase_acc` 改為 local variable（每封包重置），不再是 `static`
- inner for-loop 是唯一 II=1 的區段，且只讀 stream_payload，無條件式 FIFO 讀取

### Fix 2：stream_payload depth（必要，效能）

現有 pragma 被 HLS 覆蓋為 depth=2（RTL 合成結果：`ofdm_cfo_sync_fifo_w33_d2_S`）。

**修法：** 使用 template 參數強制指定深度：

```cpp
// 改前（在 ofdm_cfo_sync top function 裡）：
hls::stream<internal_axis_t> stream_payload("stream_payload");
#pragma HLS STREAM variable=stream_payload depth=4096 impl=fifo

// 改後：
hls::stream<internal_axis_t, 2048> stream_payload("stream_payload");
#pragma HLS STREAM variable=stream_payload depth=2048 impl=bram
```

**為什麼需要 2048：**

Fix 1 後，`cfo_correction` outer loop 先 blocking 等 phase_inc，再進 for-loop 讀 stream_payload。
在等待 atan2f（約 200 cycles）期間，`moving_sum` 持續寫入 stream_payload。
stream_payload 需要能緩衝這段期間寫入的所有資料。
最壞情況 = 整個 FFT_LEN（2048）筆都寫完後才讀 → depth = 2048。

---

## 4. 架構方向說明（本次不改，記錄為後續方向）

### 現有問題

`moving_sum_and_metric` 同時做：
1. **控制路徑**：計算 S&C metric，偵測 peak，發出 corr_val
2. **資料路徑**：透過 FSM 把 Data sample 寫進 stream_payload

這兩個職責耦合導致 stream_payload 的寫入時序依賴於控制 FSM，使 FIFO 深度需求難以預測。

### 理想目標架構

```
                ┌──────────────────────────────────────────┐
                │  calc_power_and_corr                     │
                │  輸出：power, corr1/2/3, data_bypass     │
                └───────────┬──────────────────────────────┘
                            │ stream_data_bypass (x[k-2048])
               ┌────────────┤
               │            │
               ▼            ▼
     sync_search         payload_gate
    （只算 metric，     （持續讀 bypass，
      發 corr_val        收到 trigger 後
      + trigger）        輸出 FFT_LEN 筆）
               │            │
               ▼            ▼
        cfo_estimator   stream_payload
               │            │
               ▼            ▼
         stream_phase_inc  ──→  cfo_correction
```

### 為何本次不做完整拆分

`sync_search` 在 k=2306 觸發 trigger，此時 stream_data_bypass 的當前值是 x[258]（= Data[2]）。
`payload_gate` 需要的第一筆資料是 x[256] = Data[0]（在 pipeline 中以 x_n_m2 保存）。
Trigger 透過 FIFO 傳遞有 ≥1 cycle 延遲，導致 payload_gate 收到 trigger 時，x_m2 已偏移。

解決方案需要：`calc_power_and_corr` 輸出兩條 bypass（一條給 sync，一條給 payload_gate 且多延遲 2 cycle）。
這需要修改 calc 的介面，屬於較大的改動，本次優先解決死鎖。

---

## 5. 本次改動範圍

| 位置 | 改動 |
|------|------|
| `cfo_correction` 函式 | 完全重寫（見 Fix 1 code sketch） |
| `stream_payload` 宣告 | depth=2048, impl=bram（見 Fix 2） |
| 其他 stage | 不動 |

### 改動後預期行為

- `moving_sum_and_metric`：不變，繼續寫 2048 筆到 stream_payload
- `cfo_estimator`：不變，算完 phase_inc 寫入 stream_phase_inc
- `cfo_correction`：outer loop blocking 等 phase_inc → for-loop 讀 2048 筆 → 輸出 → outer loop 再等下一個 phase_inc

**無死鎖原因：** cfo_correction 永遠不會在 II=1 pipeline 內等 stream_phase_inc。
它在非 pipelined 的 outer loop 等，等到了才進入 for-loop，for-loop 只讀 stream_payload（必然有資料，因為 depth=2048 足夠緩衝 atan2f 期間的寫入）。

---

## 6. 後續（本次不動）

- Fix 3：`phase_acc` / `phase_inc_fixed` 從 `ap_fixed<16,1>` 改為 `ap_fixed<32,1>`，修正正負 CFO 的 AP_TRN 截斷不對稱（positive CFO MSE > 0.005）
- 架構重構：完整拆分 sync_search + payload_gate（需解決 timing alignment）
