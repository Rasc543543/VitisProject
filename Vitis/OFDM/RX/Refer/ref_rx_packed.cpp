#include "ref_rx_defs.h"

void ref_rx_packed(
    hls::stream<axis_t> &input_stream,
    hls::stream<axis_t> &output_stream)
{
#pragma HLS INTERFACE ap_ctrl_none port = return
#pragma HLS INTERFACE axis port = input_stream
#pragma HLS INTERFACE axis port = output_stream

    static ap_uint<16> buf_i[FFT_LENGTH];
    static ap_uint<16> buf_q[FFT_LENGTH];
#pragma HLS BIND_STORAGE variable = buf_i type = ram_2p
#pragma HLS BIND_STORAGE variable = buf_q type = ram_2p

Load_Packed:
    for (int i = 0; i < FFT_LENGTH; i++)
    {
#pragma HLS PIPELINE II = 1
        axis_t pkt = input_stream.read();
        buf_i[i] = pkt.data(15, 0);
        buf_q[i] = pkt.data(31, 16);
    }

    half max_metric = (half)-1.0f;
    int best_cfo = 0;

Loop_CFO_Scan:
    for (int offset = -CFO_RANGE; offset <= CFO_RANGE; offset++)
    {
        half p_metric[4] = {0, 0, 0, 0};
#pragma HLS ARRAY_PARTITION variable = p_metric complete

    Loop_Metric:
        for (int p = 0; p < PILOT_TONES; p++)
        {
#pragma HLS PIPELINE II = 1
            int idx = LEFT_GUARD + offset + p * 7;
            // ✅ 改用 fixed16_to_half 解碼
            half vi = fixed16_to_half(buf_i[idx]);
            half vq = fixed16_to_half(buf_q[idx]);
            p_metric[p & 0x3] += (vi * vi + vq * vq);
        }
        half metric = p_metric[0] + p_metric[1] + p_metric[2] + p_metric[3];
        if (metric > max_metric)
        {
            max_metric = metric;
            best_cfo = offset;
        }
    }

    int base = LEFT_GUARD + best_cfo;

Loop_Process_Packed:
    for (int i = 0; i < PILOT_TONES - 1; i++)
    {
#pragma HLS PIPELINE off
        int idx1 = base + i * 7;
        int idx2 = base + (i + 1) * 7;

        // 用 float 做通道估計，避免 FP16 精度不足造成插值誤差放大
        float p_inv1 = (float)PILOT_ROM_INV[i];
        float h1_real = (float)fixed16_to_half(buf_i[idx1]) * p_inv1;
        float h1_imag = (float)fixed16_to_half(buf_q[idx1]) * p_inv1;

        float p_inv2 = (float)PILOT_ROM_INV[i + 1];
        float h2_real = (float)fixed16_to_half(buf_i[idx2]) * p_inv2;
        float h2_imag = (float)fixed16_to_half(buf_q[idx2]) * p_inv2;

        // 插值斜率 (float 精度)
        float m_real = (h2_real - h1_real) * (1.0f / 7.0f);
        float m_imag = (h2_imag - h1_imag) * (1.0f / 7.0f);
        float cur_h_r = h1_real;
        float cur_h_i = h1_imag;

Loop_Interp:
        for (int k = 1; k <= 6; k++)
        {
#pragma HLS PIPELINE II = 1
            cur_h_r += m_real;
            cur_h_i += m_imag;

            int didx = idx1 + k;
            // 1. 解碼接收到的 Data (Y)，轉為 float 計算
            float d_i = (float)fixed16_to_half(buf_i[didx]);
            float d_q = (float)fixed16_to_half(buf_q[didx]);

            // 2. 執行 Y * H* (ZF equalization numerator)
            float num_i = d_i * cur_h_r + d_q * cur_h_i;
            float num_q = d_q * cur_h_r - d_i * cur_h_i;

            // 3. 計算通道能量 |H|^2
            float h_power = cur_h_r * cur_h_r + cur_h_i * cur_h_i;

            // 4. 避免除以 0
            if (h_power < 0.0001f) {
                h_power = 1.0f;
            }

            // 5. ZF 等化：Y * H* / |H|^2
            float out_i = num_i / h_power;
            float out_q = num_q / h_power;

            // 6. 輸出時轉回 FP16 pack
            axis_t out_pkt;
            out_pkt.data = pack_iq((half)out_i, (half)out_q);
            out_pkt.keep = 0xF;
            out_pkt.strb = 0xF;
            out_pkt.last = (i == PILOT_TONES - 2 && k == 6) ? 1 : 0;
            output_stream.write(out_pkt);
        }
    }
}