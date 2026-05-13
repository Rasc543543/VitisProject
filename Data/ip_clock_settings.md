# OFDM IP Clock Settings 記錄

## test_Brian 完整鏈路（2026-04-28 最終版）

```
DMA → energy_dispersal → RS enc → tx_interleaver_dynam
    → Viterbi enc → tx_interleaver64 → mapper
    → tx_interleaver_float → ref_signal → IFFT → FFT
    → ref_rx → deinterleaver_flo → demap
    → deinterleaver64 → Viterbi dec
    → rx_deinterleaver_dyn → RS dec → energy_descramble → DMA
```

---

### HLS IP — 250 MHz 合成（ClockPeriod = 4 ns）⚠️

若未來要改為 100 MHz，需重新 HLS synthesis + export IP + 更新 Vivado IP catalog。

| IP 名稱（BD） | HLS 專案路徑 | 備註 |
|--------------|-------------|------|
| energy_dispersal_0 | `C:\OFDM\TX\energy_dispersal_1byte` | TX PRBS XOR |
| symbol_mapper_hls_0 | `C:\OFDM\TX\Mapper_32bit\Mapper_32bit\symbol_mapper_hls` | bit→FP16 IQ |
| reference_signal_tx_0 | `C:\OFDM\TX\Refer\packed\ref\reference_signal_tx_packed` | pilot + guard，2048 tones |
| energy_descramble_0 | `C:\OFDM\RX\RX_energy_descramble_1byte` | RX PRBS XOR |

---

### HLS IP — 100 MHz 合成（ClockPeriod = 10 ns）✅

| IP 名稱（BD） | HLS 專案路徑 |
|--------------|-------------|
| rs_encoder_tx_0 | `C:\VitisProject\Vitis\OFDM\TX\RS` |
| tx_interleaver64_dyn_1 | `C:\OFDM\TX\tx_interleaver64_dynamic` |
| ref_rx_packed_0 | `C:\VitisProject\Vitis\OFDM\RX\Refer\ref_rx_fixed` |
| p8psk_demap_packed_0 | `C:\OFDM\RX\_8psk_demap` |
| rx_deinterleaver64_d_0 | `C:\OFDM\RX\rx_deinterleaver64_dynamic` |
| rx_deinterleaver_dyn_0 | `C:\OFDM\RX\rx_deinterleaver_dynamic` |
| rs_decoder_rx_0 | `C:\VitisProject\Vitis\OFDM\RX\RS` |
| Viterbi_decode_0 | `C:\OFDM\Viterbi_decode`（新版，Vitis HLS 2024.2） |

---

### HLS IP — 異常值（tx_interleaver_dynamic）

| IP 名稱（BD） | HLS 專案路徑 | ClockPeriod |
|--------------|-------------|-------------|
| tx_interleaver_dynam_0 | `C:\OFDM\TX\tx_interleaver_dynamic` | 10000000（疑似 ps 單位，= 10 ms，等同未約束）|

---

### Pure RTL（非 HLS，無 clock 設定問題）

| IP 名稱（BD） | 路徑 |
|--------------|------|
| axi_viterbi_encode_w_0 | `C:\OFDM\TX\viterbi` |
| tx_interleaver_float_0 | `C:\OFDM\TX\tx_interleaver_floatIQ` |
| rx_deinterleaver_flo_0 | `C:\OFDM\RX\rx_deinterleaver_floatIQ` |
| smart_ifft_0 | `C:\OFDM\TX\smart_ifft` |
| smart_FFT_0 | `C:\OFDM\RX\smart_fft` |

---

## 現狀評估（2026-04-28）

- test_Brian WNS = **+1.068 ns**，Failing Endpoints = 0，100 MHz timing 全通過
- 250 MHz 合成 IP 共 **4 個**：energy_dispersal、symbol_mapper、reference_signal_tx、energy_descramble
- 系統硬體驗證：BER = 0/2192，**目前不需要修改**

## 若要將 250 MHz IP 改為 100 MHz 的步驟

1. 開啟對應的 Vitis HLS 專案
2. Solution → Solution Settings → clock period 從 `4` 改為 `10`，uncertainty 維持 `10%`（1 ns）
3. 重新 C Synthesis → Export RTL
4. Vivado → IP Catalog → Refresh，更新 IP
5. 重新 Generate Output Products → Synthesis → Implementation → 確認 timing report
