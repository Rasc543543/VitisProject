# combined_correction Deadlock Analysis

## 發生時間

RTL Behavioral Sim，約 54410000 ns（5441 萬 cycle）後觸發 HLS deadlock detector。

---

## Deadlock 循環

```
payload_gate ──[stream_payload FULL]──► combined_correction
     ▲                                        │
     │                              等 stream_locked_corr（EMPTY）
     │                                        │
sync_search ──[stream_sync_flag FULL]──► payload_gate 無法讀
```

| 角色 | 狀態 | 原因 |
|------|------|------|
| `payload_gate` | 卡在寫 stream_payload | stream_payload 已滿（depth=512），無法寫入 |
| `combined_correction` | 卡在等 stream_locked_corr | FIFO 是空的，無法進入 processing_math |
| `sync_search` | 卡在寫 stream_sync_flag | stream_sync_flag 已滿（depth=256），無法推進 |

三者互等，無法解開。

---

## 造成問題的程式碼

### 問題：`while(true)` + `#pragma HLS PIPELINE II=1` + 條件式多 FIFO read

```cpp
static void combined_correction(...) {
    static bool correcting      = false;
    static bool processing_math = false;
    static ap_uint<6> math_delay_cnt = 0;
    // ...

    while (true) {
#pragma HLS PIPELINE II=1          // ← 問題根源

        // 背景計算（每 cycle 都執行）
        cfo_t phi1 = hls::atan2f(...);
        cfo_t phi2 = hls::atan2f(...);
        cfo_t phi3 = hls::atan2f(...);
        // ...

        if (!correcting) {
            in_corr.read();        // 只在 state 0 讀 stream_locked_corr
            ...
        } else if (processing_math) {
            // 不讀任何 FIFO，只計數 45 cycles
            math_delay_cnt++;
        } else {
            in_payload.read();     // 只在 state 2 讀 stream_payload
            ...
        }
    }
}
```

### 為什麼 HLS 無法正確生成 RTL

**PIPELINE II=1 + 條件式 FIFO read** 的問題：

1. HLS 把整個 while loop body 攤平為單一 pipeline stage chain
2. 在 `processing_math` 狀態（45 cycles），沒有任何 FIFO 被讀取
3. RTL 生成的 ready/valid handshaking 邏輯需要在每個 pipeline 週期決定是否接受輸入
4. HLS 在多個 FIFO 的條件讀取下，會生成錯誤的 stall 條件：
   - RTL 認為 `in_payload` 必須有資料才能推進（即使邏輯上是在 processing_math 不需要讀它）
   - 造成 combined_correction 雖然在等 `stream_locked_corr`，卻對 `stream_payload` 產生隱性背壓

5. 具體：
   - `payload_gate` 寫滿 `stream_payload`（512 個）後 stall
   - `payload_gate` stall → 無法讀 `stream_sync_flag`
   - `stream_sync_flag` 滿（256 個）→ `sync_search` stall
   - `sync_search` stall → 無法寫 `stream_locked_corr`
   - `combined_correction` 等不到 `stream_locked_corr` → 永遠 stall

---

## 根本原因

**HLS `PIPELINE II=1` 無法正確處理「跨多個 FIFO 的多狀態條件讀」。**

HLS pipeline 假設每個 iteration 都會 consume/produce 相同的 FIFO 集合。  
當某些 iteration 不讀任何 FIFO（processing_math 45 cycles），RTL 生成的 flow control 邏輯會錯誤地 stall 整個 pipeline。

---

## 修法：改回 outer-while + inner-for

```cpp
static void combined_correction(...) {
    while (true) {
        // outer while：非 pipeline，阻塞讀 stream_locked_corr
        // atan2f 在這裡執行（~71 cycles），不會 stall 其他 FIFO
        cfo_corr_t corr = in_corr.read();   // 等到有資料才繼續

        cfo_t phi1 = hls::atan2f(...);
        cfo_t phi2 = hls::atan2f(...);
        cfo_t phi3 = hls::atan2f(...);
        // ... 計算 phase_inc ...

        for (int i = 0; i < FFT_LEN; i++) {
#pragma HLS PIPELINE II=1           // 只有 inner for 是 II=1
            in_payload.read();      // 每 cycle 讀一個 payload
            // ... apply correction ...
            out_axis.write(...);
        }
    }
}
```

**為什麼 outer-while + inner-for 不死鎖：**
- outer-while 讀 `in_corr`：blocking wait，不需要 II=1
- atan2f 執行期間（~71 cycles），`payload_gate` 寫 stream_payload，但 combined_correction 不讀它 → payload_gate 最多暫時積累 71 個 item，stream_payload depth=1024 足夠
- inner-for 用 PIPELINE II=1 只讀 `in_payload`，單一 FIFO，HLS 可正確生成 handshaking
- 沒有跨多 FIFO 的條件讀，不會產生矛盾的 stall 條件

---

## 保留的改動（不需退回）

| 項目 | 值 | 說明 |
|------|-----|------|
| `delay_payload` tap 數 | **N=2** | 時序分析正確：j_fire=2306，x[256]=Data[0] |
| calc `pwr` 計算 | sq_r + sq_i 各自 BIND_OP | DSP 利用率改進 |
| sync_search cast | 已移除多餘 `(sq_metric_t)` | 不干擾 BIND_OP 辨識 |

**只有 `combined_correction` 需要退回 outer-while + inner-for，stream_payload depth 改回 1024。**
