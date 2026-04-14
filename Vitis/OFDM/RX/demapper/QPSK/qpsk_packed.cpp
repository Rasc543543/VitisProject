#include "demapper_types.h"

const float QPSK_REF_VAL = 0.70710678f;
const float DIST_QPSK = 1.41421356f;

static void get_hard_decision_qpsk(float i, float q, float &ref_i, float &ref_q)
{
#pragma HLS INLINE
    ref_i = (i > 0) ? QPSK_REF_VAL : -QPSK_REF_VAL;
    ref_q = (q > 0) ? QPSK_REF_VAL : -QPSK_REF_VAL;
}

void qpsk_demap_packed(
    hls::stream<axis_word_32> &input_stream,
    hls::stream<axis_soft_out> &output_stream)
{
#pragma HLS INTERFACE ap_ctrl_none port = return
#pragma HLS INTERFACE axis port = input_stream
#pragma HLS INTERFACE axis port = output_stream

    ap_uint<16> I_buffer[FRAME_SYMBOLS];
    ap_uint<16> Q_buffer[FRAME_SYMBOLS];
#pragma HLS BIND_STORAGE variable = I_buffer type = ram_2p
#pragma HLS BIND_STORAGE variable = Q_buffer type = ram_2p

    float p_sum_err[8];
    float p_sum_sig[8];
#pragma HLS ARRAY_PARTITION variable = p_sum_err complete
#pragma HLS ARRAY_PARTITION variable = p_sum_sig complete

    while (true)
    {
#pragma HLS PIPELINE off
#ifndef __SYNTHESIS__
        if (input_stream.empty())
            break;
#endif

    init_reset:
        for (int k = 0; k < 8; k++)
        {
#pragma HLS UNROLL
            p_sum_err[k] = 0.0f;
            p_sum_sig[k] = 0.0f;
        }

    // Loop 1: 純 stream 讀取 + BRAM 寫入，無浮點運算
    recv_data:
        for (int i = 0; i < FRAME_SYMBOLS; i++)
        {
#pragma HLS PIPELINE II = 1
            axis_word_32 pkt = input_stream.read();
            I_buffer[i] = pkt.data(15, 0);
            Q_buffer[i] = pkt.data(31, 16);
        }

    // Loop 2: 純浮點 SNR 計算，從 BRAM 讀取，不涉及 stream
    calc_snr:
        for (int i = 0; i < FRAME_SYMBOLS; i++)
        {
#pragma HLS PIPELINE II = 1
            float val_I = (float)bits_to_half(I_buffer[i]);
            float val_Q = (float)bits_to_half(Q_buffer[i]);
            float ref_I, ref_Q;
            get_hard_decision_qpsk(val_I, val_Q, ref_I, ref_Q);
            float err_I = val_I - ref_I;
            float err_Q = val_Q - ref_Q;
            int idx = i & 0x7;
            p_sum_err[idx] += (err_I * err_I + err_Q * err_Q);
            p_sum_sig[idx] += (ref_I * ref_I + ref_Q * ref_Q);
        }
        float sum_error_sq = p_sum_err[0] + p_sum_err[1] + p_sum_err[2] + p_sum_err[3] + p_sum_err[4] + p_sum_err[5] + p_sum_err[6] + p_sum_err[7];
        float sum_signal_sq = p_sum_sig[0] + p_sum_sig[1] + p_sum_sig[2] + p_sum_sig[3] + p_sum_sig[4] + p_sum_sig[5] + p_sum_sig[6] + p_sum_sig[7];

        float noise_pwr = (sum_error_sq < 1e-6f) ? 1e-6f : sum_error_sq;
        float csi_factor = (2.0f * DIST_QPSK) * (sum_signal_sq / noise_pwr);

        // [修正] 套用統一的 10.0 天花板
        if (csi_factor > MAX_CSI_FACTOR)
            csi_factor = MAX_CSI_FACTOR;

    process_output:
        for (int i = 0; i < FRAME_SYMBOLS * 2; i++)
        {
#pragma HLS PIPELINE II = 1
            // 定義索引與頻道
            int sym_idx = i >> 1;
            bool is_q_ch = (i & 1);

            // [關鍵修正]：平行觸發 BRAM 讀取，不帶條件判斷
            // HLS 會在同一個週期送出兩個位址，消除位址計算的 Pipeline 錯位
            ap_uint<16> val_i = I_buffer[sym_idx];
            ap_uint<16> val_q = Q_buffer[sym_idx];

            // 讀取後再進行 Mux 選擇
            ap_uint<16> raw_val = is_q_ch ? val_q : val_i;

            // 後續解調運算
            float val = (float)bits_to_half(raw_val);
            ap_uint<8> data_out = quantize_llr(val * csi_factor);

            axis_soft_out out_pkt;
            out_pkt.data = data_out;
            out_pkt.last = (i == (FRAME_SYMBOLS * 2 - 1));
            out_pkt.keep = 1;
            out_pkt.strb = 1;
            output_stream.write(out_pkt);
        }
    }
}