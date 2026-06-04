# cp_major Back-to-Back 幀同步失敗 Debug 記錄

**日期：** 2026-06-03  
**狀態：** 根本原因未確定，修法 v1 無效

---

## 問題描述

### TB 版本
- **v4d（zero frame gap）：✅ PASS** — 每幀之間插 2048 zeros + tlast
- **v4e（back-to-back LFSR）：❌ FAIL** — 4 幀直接連續送入，無任何間隔

### v4e 失敗結果（修法 v1 後仍相同）

```
Pkt[0]  MSE=0.000000  marker@[0]   CORRECT
Pkt[1]  MSE=0.333978  marker@[28]  TIMING ERROR
Pkt[2]  MSE=0.336064  marker@[19]  TIMING ERROR
Pkt[3]  MSE=0.000000  marker@[0]   CORRECT
```

觀察：
- Pkt[0] 和 Pkt[3] 正確，Pkt[1] 和 Pkt[2] 偏移（28、19，每次不同）
- Pkt[3] 正確原因不明（理應也受前一幀錯誤觸發影響）

### 幀結構

```
Frame N：CP(256 samples) + Data(2048 samples) = 2304 samples
Frame 0：samples 0..2303
Frame 1：samples 2304..4607
Frame 2：samples 4608..6911
Frame 3：samples 6912..9215
```

S&C 峰值：
- Frame 0 k_peak = 2303，trigger 在 k = 2306（+3 cycle delay）
- Frame 1 k_peak = 4607，trigger 應在 k = 4610

---

## 程式架構

```
unpack_axis → calc_power_and_corr2 → sync_search → combined_correction
                  (IL=7, II=1)          (IL=3, II=1)    (outer-while + inner-for)
                                              ↓ (stream_sync_flag, every cycle)
                                          payload_gate
                                           (stream_payload depth=1024)
```

關鍵 FIFO：

| FIFO | depth | 說明 |
|------|-------|------|
| stream_data_payload | 256 | calc_power_and_corr2 → payload_gate |
| stream_sync_flag | 256 | sync_search → payload_gate |
| stream_payload | 1024 BRAM | payload_gate → combined_correction |
| stream_locked_corr | 4 | sync_search → combined_correction |

---

## 關鍵參數（已驗證）

- lag = 2176（FFT_LEN=2048 + SUB_CP_LEN=128）
- c1 = x[n] × x[n-2176]*，128 點累加 → timing metric
- c2 = x[n] × x[n-2048]*，c3 = x[n-2048] × x[n-2176]*，→ CFO 估計
- delay_payload[2]，x_payload = x[k-2050]
- j_fire = k_peak + 3 = 2306，x[2306-2050] = x[256] = Data[0] ✓
- post_detection 長度 = FFT_LEN = 2048 cycles

---

## 原始 sync_search 邏輯（修法前）

```
每 cycle：
  if (post_detection):
      讀入 power/corr → 丟棄
      只計 counter (0..2047)
      counter==2047：reset acc + warmup + ptr_sum，關閉 post_detection
  else:
      正常累加 power/corr
      計算 metric_m0，shift m1←m0 m2←m1 m3←m2
      if is_true_peak: 觸發，開啟 post_detection
```

---

## 修法 v1（2026-06-03）

### 假設的根本原因

Frame 0 trigger 在 k=2306 觸發。此時 Frame 1 CP 已在 k=2304 開始（trigger 晚了 2 cycle）。

post_detection 期間（k=2306..4353），**所有 power/corr sample 被讀取後丟棄**。Frame 1 的 CP（k=2304..2559，共 256 samples）全部在此期間消失。post_detection 結束後 accumulator 被清零重啟，找不到 Frame 1 的 CP correlation，觸發隨機假峰。

### 修改內容（ofdm_cfo_sync.cc，sync_search 函式）

**主要變動：**

```cpp
// Before（只在 else 分支 accumulate）
if (post_detection) {
    // 讀入並丟棄
} else {
    // accumulate + detect
}

// After（無條件 accumulate，post_detection 只壓制 trigger）
{  // Always accumulate
    // delay array update
    // acc_pwr, acc_corr1..3 update
    // search_warmup++
}
metric_m0 = r_sq + i_sq;  // 計算在 if-else 外

if (post_detection) {
    if (post_counter == FFT_LEN-1) {
        // 只 reset metric_m*/corr_m*/thresh（不 reset acc）
        post_detection = false; post_counter = 0;
    } else {
        post_counter++;
        // 繼續 shift metric registers
    }
} else {
    // is_true_peak 判斷 + 觸發
}
```

**預期效果：**  
Frame 1 CP 在 k=2304..2559 仍正常進入 accumulator，M[k] 在 k=4607 正常出峰，post_detection（k=2306..4353）結束後 trigger 在 k=4610 正確觸發。

### 結果

**無效。** RTL Sim 結果與修法前完全相同。

---

## 待釐清的問題

### 問題 A：IP 是否真的更新？

Vivado Behavioral Sim 使用的 RTL 來自 HLS synthesis。若只改了 .cc 原始碼但未重跑 `csynth`，匯出的 IP 不會改變。

確認步驟：
1. Vitis HLS → Run C Synthesis（重跑 csynth）
2. Export RTL as IP（重新匯出）
3. Vivado → Update IP → Re-generate Output Products
4. 重跑 Behavioral Simulation

### 問題 B：M[k] 是否真的在 k=4607 出峰？

S&C metric（lag 2176）的峰值位置 k=2303 是由 RTL sim 驗證的，但 back-to-back 情況下前後幀的相互干擾（cross-correlation）可能改變峰值位置。

需要在 TB 中加 debug 輸出，直接觀察 metric 數值。

### 問題 C：Pkt[3] 為何正確？

Pkt[1] 在 marker@[28] 觸發（偏早），Pkt[2] 在 marker@[19] 觸發。這兩次錯誤觸發後，post_detection 各跑 2048 cycle，之後的狀態剛好讓 Pkt[3] 的真實峰值被正確偵測到？還是 TB 在最後一幀後有 zero padding？

---

## 當前程式碼（關鍵段落）

### sync_search 完整邏輯（修法 v1 後）

```cpp
while (true) {
#pragma HLS PIPELINE II = 1
#ifndef __SYNTHESIS__
    if (in_power.empty() || in_corr1.empty() || in_corr2.empty() || in_corr3.empty()) break;
#endif
    acc_t         pwr_n = in_power.read();
    complex_acc_t c1_n  = in_corr1.read();
    complex_acc_t c2_n  = in_corr2.read();
    complex_acc_t c3_n  = in_corr3.read();

    bool trigger_this_cycle = false;

    // Always accumulate regardless of post_detection state
    {
        acc_t pwr_d = delay_pwr[ptr_sum]; /* ... read delay lines ... */

        delay_pwr[ptr_sum] = pwr_n; /* ... write delay lines ... */
        if (ptr_sum == SUB_CP_LEN - 1) ptr_sum = 0; else ptr_sum++;

        acc_t pwr_to_sub = (search_warmup < SUB_CP_LEN) ? 0 : pwr_d;
        /* ... c_to_sub 同理 ... */

        acc_pwr   = acc_pwr + pwr_n - pwr_to_sub;
        acc_corr1 = /* sliding window update */;
        /* acc_corr2, acc_corr3 同理 */

        if (search_warmup < SUB_CP_LEN) search_warmup++;
    }

    metric_m0     = r_sq + i_sq;          // |acc_corr1|²
    target_thresh = threshold_config * p_sq; // 0.5 × acc_pwr²

    if (post_detection) {
        if (post_counter == (ap_uint<12>)(FFT_LEN - 1)) {
            post_detection = false; post_counter = 0;
            // Reset ONLY metric registers (acc state preserved)
            metric_m0 = metric_m1 = metric_m2 = metric_m3 = 0;
            thresh_m2 = thresh_m3 = 0;
            corr*_m1 = corr*_m2 = corr*_m3 = 0;
        } else {
            post_counter++;
            // Keep shifting metric registers during post_detection
            thresh_m3 = thresh_m2; thresh_m2 = target_thresh;
            metric_m3 = metric_m2; metric_m2 = metric_m1; metric_m1 = metric_m0;
            corr*_m3 = corr*_m2; corr*_m2 = corr*_m1; corr*_m1 = acc_corr*;
        }
    } else {
        bool warmup_done  = (search_warmup >= SUB_CP_LEN);
        bool is_true_peak = warmup_done &&
                            (metric_m3 > thresh_m3) &&
                            (metric_m3 > metric_m2) &&
                            (metric_m2 > metric_m1) &&
                            (acc_pwr > 1.0);
        if (is_true_peak) {
            out_corr.write(corr_val);
            trigger_this_cycle = true;
            post_detection = true; post_counter = 0;
            metric_m* = 0; corr*_m* = 0;
        } else {
            /* shift metric registers */
        }
    }

    out_sync_flag.write(trigger_this_cycle);
}
```

### is_true_peak 條件（不變）

```cpp
bool is_true_peak = warmup_done &&
                    (metric_m3 > thresh_m3) &&   // 超過門檻
                    (metric_m3 > metric_m2) &&   // 三點遞減（m3 是峰）
                    (metric_m2 > metric_m1) &&
                    (acc_pwr > 1.0);             // 防零功率誤觸發
```

---

## 下一步診斷方向

1. **確認 IP 更新**：重跑 csynth → 重新匯出 → 更新 Vivado IP → 重跑 sim

2. **加 debug print 到 testbench**：觀察 sync_search 輸出的 `stream_sync_flag` 波形，確認 trigger 真正發生在哪個 sample

3. **觀察 metric 波形**：在 Vivado 波形視窗加 acc_corr1 相關信號，看 M[k] 是否在 k=4607 出現峰值

4. **簡化 TB 測試**：先用 2 幀 back-to-back（v4f），確認 Pkt[1] 是否能過，再擴展到 4 幀

---

## 檔案位置

| 檔案 | 說明 |
|------|------|
| `C:\VitisProject\Vitis\OFDM\RX\cp_major\ofdm_cfo_sync.cc` | 修法 v1 後的主程式 |
| `C:\VitisProject\Vitis\OFDM\RX\cp_major\ofdm_cfo_sync.cc.bak_20260603` | 修法前備份 |
| `C:\Vivado\IP_solo_test\tb_ofdm_loopback.v` | Vivado Behavioral Sim TB（含 v4e） |
