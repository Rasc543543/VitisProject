# CFO Cache 優化設計文件

整理日期：2026-05-11
對應檔案：`Vitis/OFDM/RX/Refer/ref_rx_packed.cpp`

---

## 背景：CFO 是什麼

CFO（Carrier Frequency Offset）是發射端與接收端載波頻率不一致造成的偏移，來源有兩種：

| 來源 | 行為 | 變化速度 |
|------|------|---------|
| 硬體振盪器誤差 | 幀間幾乎固定 | 極慢（溫度漂移，分鐘級）|
| Doppler 效應 | 衛星移動造成頻率連續偏移 | 緩慢但持續（秒~分鐘級）|

### Doppler 的「漸進漂移」特性

Doppler 不是突然跳變，而是連續、可預測地移動：

```
LEO 衛星（高度 600 km，載波 2.4 GHz）：
  全程通過時間：~600 秒
  Doppler 範圍：+60 kHz → -60 kHz
  變化速率：~200 Hz/s

若 subcarrier 間距 = 15 kHz：
  每移動 1 個 offset 需要 75 秒 = 75,000 幀
  → 每幀最多漂移 1 個 subcarrier
```

這個特性保證 Tracking Mode（每幀最多移 ±1 格）可以完整追蹤 Doppler。

---

## 現況（原始版本）

```cpp
// local 變數，每幀重置
half max_metric = (half)-1.0f;
int best_cfo = 0;

// 無條件跑 141 次完整掃描
for (int offset = -CFO_RANGE; offset <= CFO_RANGE; offset++) {  // 141 次
    for (int p = 0; p < PILOT_TONES; p++) {                      // 273 次
        half vi = fixed16_to_half(buf_i[idx]);
        half vq = fixed16_to_half(buf_q[idx]);
        p_metric[p & 0x3] += (vi * vi + vq * vq);               // 浮點乘法
    }
    if (metric > max_metric) best_cfo = offset;
}
int base = LEFT_GUARD + best_cfo;
```

**每幀代價：141 × 273 = 38,493 次能量計算，約 42,864 cycles = 428 µs @100 MHz**

---

## 四個版本比較

| | 原始版本 | Simple Cache | Neighbor Check | **Tracking Mode** |
|--|--|--|--|--|
| 穩定時代價 | 428 µs | ~0 µs | ~9 µs | ~9 µs |
| Doppler 漂移代價 | 428 µs | 錯誤（不追蹤）| 428 µs/幀 | **~9 µs** |
| 適合 loopback | ✅ | ✅ | ✅ | ✅ |
| 適合衛星通訊 | ❌ | ❌ | ❌ | **✅** |
| 實作複雜度 | — | 低 | 中 | 中 |

---

## 各版本設計說明

### 版本 A：Simple Cache

```cpp
static int  best_cfo   = 0;
static bool cfo_locked = false;

if (!cfo_locked) {
    // 完整 141 次掃描
    cfo_locked = true;
}
// 之後完全不掃，直接用舊值
```

- 只掃一次，永久鎖定
- **問題：** 無法追蹤任何 CFO 漂移，Doppler 下直接失效
- 結論：只適合 loopback（CFO 永遠為 0），不值得實作

---

### 版本 B：Neighbor Check Cache

```cpp
static int  best_cfo   = 0;
static bool cfo_locked = false;

half e_left  = measure_energy(best_cfo - 1);
half e_cur   = measure_energy(best_cfo);
half e_right = measure_energy(best_cfo + 1);

bool need_rescan = !cfo_locked || (e_left > e_cur) || (e_right > e_cur);

if (need_rescan) {
    // 141 次完整重掃
    cfo_locked = true;
}
```

- 鄰居能量更大 → 觸發完整 141 次重掃（428 µs）
- **問題：** Doppler 每幀都在漂移 → 每幀都觸發重掃 → 與原始版本相同代價
- 結論：對衛星通訊無效，不值得實作

---

### 版本 C：Tracking Mode（建議實作）

```cpp
static int  best_cfo    = 0;
static bool initialized = false;

if (!initialized) {
    // 第一幀：完整 141 次掃描建立初始鎖定
    half max_metric = (half)-1.0f;
    for (int offset = -CFO_RANGE; offset <= CFO_RANGE; offset++) {
        half energy = 0;
        for (int p = 0; p < PILOT_TONES; p++) {
            if (p == 136) continue;
            int idx = LEFT_GUARD + offset + p * 7;
            half vi = fixed16_to_half(buf_i[idx]);
            half vq = fixed16_to_half(buf_q[idx]);
            energy += (vi < (half)0 ? -vi : vi) + (vq < (half)0 ? -vq : vq);  // SAL
        }
        if (energy > max_metric) { max_metric = energy; best_cfo = offset; }
    }
    initialized = true;
} else {
    // 第 2+ 幀：只看 3 個點，跟著漂移走
    half e_left  = measure_energy(best_cfo - 1);
    half e_cur   = measure_energy(best_cfo);
    half e_right = measure_energy(best_cfo + 1);

    if      (e_left > e_cur && e_left > e_right)  best_cfo -= 1;
    else if (e_right > e_cur && e_right > e_left) best_cfo += 1;
    // 否則 best_cfo 不動
}

int base = LEFT_GUARD + best_cfo;
```

- 第一幀：428 µs（完整掃描建立初始點）
- 第 2+ 幀：永遠只需 ~9 µs，**不管 Doppler 多快**
- Doppler 漂移 → 每幀跟著移 ±1 格，不需要重掃
- 對 loopback（CFO=0）同樣有效：best_cfo 穩定不動

**為什麼不需要重掃：** Doppler 每幀最多漂移 1 格，neighbor check 必然能偵測到，直接更新 best_cfo 即可。

---

## 優化 2：SAL 能量計算

與 Tracking Mode 獨立，可一起套用。

### 原理

CFO Scan 的目標只是找哪個 offset 能量最大（argmax），不需要精確的能量數值。

L1 norm（`|I|+|Q|`）和 L2 norm（`I²+Q²`）的 argmax 結果相同，因為兩者都是正單調函數，最大值位置不變。

```
範例：
  offset A：I=3, Q=4 → L2=25, L1=7
  offset B：I=1, Q=1 → L2=2,  L1=2
  兩種算法都選 A，結論相同
```

### 程式碼改動

只改第 43 行：

```cpp
// ❌ 現行：浮點乘法，HLS 推 DSP48
p_metric[p & 0x3] += (vi * vi + vq * vq);

// ✅ SAL：取絕對值，只需 sign bit 操作
p_metric[p & 0x3] += (vi < (half)0 ? -vi : vi) + (vq < (half)0 ? -vq : vq);
```

### 效益

- 移除每幀 38,493 次浮點乘法
- DSP48 使用量降低（具體數字需 synthesis 確認）
- **不影響 latency（cycles 不變）**
- 對能量計算的任何版本都可套用（全掃 / Tracking 的 3-point check）

---

## 實作計畫

1. **先做 SAL**（單行，風險極低）→ csim 確認 argmax 不變
2. **再做 Tracking Mode** → csim 驗證 BER=0
3. **cosim** → 確認時序正確
4. **上板驗證** → 量測 QPSK frame time，確認 < 573 µs

### 預期效果

| 指標 | 現行 | 優化後 |
|------|------|--------|
| CFO Scan 代價（穩定）| 428 µs | ~9 µs |
| QPSK frame time | ~1001 µs | **~573 µs** |
| QPSK data rate | 1.5 Mbps | **~2.63 Mbps** |
| 衛星 Doppler 追蹤 | ❌ | ✅ |

---

## measure_energy 輔助函式

Tracking Mode 中重複用到的 3-point energy 計算，建議抽成 inline 函式：

```cpp
static inline half measure_energy(
    ap_uint<16> buf_i[], ap_uint<16> buf_q[], int cfo_offset)
{
#pragma HLS INLINE
    half energy = 0;
    for (int p = 0; p < PILOT_TONES; p++) {
#pragma HLS PIPELINE II=1
        if (p == 136) continue;
        int idx = LEFT_GUARD + cfo_offset + p * 7;
        half vi = fixed16_to_half(buf_i[idx]);
        half vq = fixed16_to_half(buf_q[idx]);
        energy += (vi < (half)0 ? -vi : vi) + (vq < (half)0 ? -vq : vq);
    }
    return energy;
}
```
