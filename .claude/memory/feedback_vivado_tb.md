---
name: Vivado TB 腳位對應規則
description: 寫 Vivado TB 時必須對應 design_1_wrapper，不能直接用 HLS IP 的腳位名
type: feedback
---

Vivado TB 的 DUT 要實例化 `design_1_wrapper`，不是 HLS 直接合成的 `rs_decoder_rx` 或其他 IP。

**Why:** Block Design 包完之後腳位名稱會加上 `_0` suffix 並改成 `input_stream_0_tdata` 格式，直接用 HLS IP 腳位名對不上 wrapper。

**How to apply:** 每次寫新的 Vivado TB 前，先找到對應的 `design_1_wrapper.v`（或同名 wrapper）看腳位定義，再照著寫實例化。腳位命名規則：
- `ap_clk` → `ap_clk_0`
- `ap_rst_n` → `ap_rst_n_0`
- `input_stream_TDATA` → `input_stream_0_tdata`（全小寫，加 `_0`）
- `output_stream_TVALID` → `output_stream_0_tvalid`
- `[0:0]` width 的 keep/strb/last 要宣告成 `reg [0:0]`，不能用 `reg`
