#include "ref_tx_defs.h"

// =========================================================
// Reference Signal TX (Packed Version) - Float to Int16 版
// =========================================================
void reference_signal_tx_packed(
    hls::stream<axis_t> &input_stream,
    hls::stream<axis_t> &output_stream)
{
#pragma HLS INTERFACE ap_ctrl_none port = return
#pragma HLS INTERFACE axis port = input_stream
#pragma HLS INTERFACE axis port = output_stream

    // 設定放大倍率：放大 2048 倍 (避免 IFFT 削峰，並且充分利用 16-bit 精度)
    const float SCALE_FACTOR = 2048.0f;

#ifndef __SYNTHESIS__
    for (int frame_cnt = 0; frame_cnt < 1; frame_cnt++)
#else
    while (true)
#endif
    {
    Loop_Packed_Frame:
        for (int i = 0; i < FFT_LENGTH; i++)
        {
#pragma HLS PIPELINE II = 1

            axis_t out_pkt;
            out_pkt.keep = 0xF;
            out_pkt.strb = 0xF;
            out_pkt.last = (i == FFT_LENGTH - 1) ? 1 : 0;

            half i_val = 0;
            half q_val = 0;

            // 1. 決定當下頻點的浮點數數值
            if (i < LEFT_GUARD || i >= FFT_LENGTH - RIGHT_GUARD)
            {
                // Guard Bands (上下邊界 填 0)
                i_val = 0;
                q_val = 0;
            }
            else
            {
                // Active Tones 區域
                int active_idx = i - LEFT_GUARD;
                int pilot_idx = active_idx / 7;
                bool is_pilot = (active_idx == (pilot_idx * 7));

                if (is_pilot)
                {
                    // Pilot 位置：I=ROM[pilot_idx], Q=0
                    i_val = PILOT_ROM[pilot_idx];
                    q_val = 0;
                }
                else
                {
                    // Data 位置：讀取 Mapper 傳來的 Packed 資料並解開成 float
                    axis_t in_pkt = input_stream.read();
                    unpack_complex(in_pkt.data, i_val, q_val);
                }
            }

            // 2. 關鍵轉換：Float16 -> 放大 -> Int16
            int16_t i_fixed = (int16_t)(i_val * SCALE_FACTOR);
            int16_t q_fixed = (int16_t)(q_val * SCALE_FACTOR);

            // 3. 打包成 32-bit 送給只懂定點數的 IFFT IP
            out_pkt.data = pack_int16(i_fixed, q_fixed);
            output_stream.write(out_pkt);
        }
    }
}