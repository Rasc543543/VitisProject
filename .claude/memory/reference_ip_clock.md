---
name: IP Clock 設定記錄
description: test_Brian chain 各 HLS IP 的合成 clock 設定（250 MHz vs 100 MHz），供未來修改參考
type: reference
originSessionId: 3ca4b32f-26cb-4dea-a851-482860a48d77
---
文檔位置：`C:\VitisProject\ip_clock_settings.md`

## 250 MHz IP（test_Brian chain 中）
- `symbol_mapper_hls_0` → `C:\OFDM\TX\Mapper_32bit`
- `reference_signal_tx_0` → `C:\OFDM\TX\Refer\packed`

## 其他 250 MHz IP（完整系統，不在 test_Brian chain）
- energy_dispersal、energy_descramble、rs_encoder_tx（C:\OFDM 版本）、rs_decoder_rx（C:\OFDM 版本）

## 現狀
test_Brian WNS = +1.068 ns，timing 全通過，250 MHz IP 目前不需要修改。
