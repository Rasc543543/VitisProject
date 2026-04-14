#include "demapper_types.h"
#include <ap_fixed.h>

#ifndef __SYNTHESIS__
#include <stdio.h> // 引入 printf 用於模擬偵錯
#endif

typedef ap_fixed<16, 2, AP_RND, AP_SAT> fixed16;

const fixed16 C1_FX = 0.92388;
const fixed16 S1_FX = 0.38268;
const fixed16 SQRT2_INV_FX = 0.7071;
const float DIST_8PSK_VAL = 0.76537f;

void p8psk_demap_packed(
    hls::stream<axis_word_32> &input_stream,
    hls::stream<axis_soft_out> &output_stream)
{
#pragma HLS INTERFACE ap_ctrl_none port = return
#pragma HLS INTERFACE axis port = input_stream
#pragma HLS INTERFACE axis port = output_stream register

    ap_uint<16> I_buffer[FRAME_SYMBOLS];
    ap_uint<16> Q_buffer[FRAME_SYMBOLS];
#pragma HLS BIND_STORAGE variable = I_buffer type = ram_2p
#pragma HLS BIND_STORAGE variable = Q_buffer type = ram_2p

#pragma HLS allocation instances = fdiv limit = 1 operation

    // ✅ 將陣列宣告移到 while 迴圈外，確保 Partition 順利映射為暫存器
    float p_sum_err[4];
    float p_sum_sig[4];
#pragma HLS ARRAY_PARTITION variable = p_sum_err complete
#pragma HLS ARRAY_PARTITION variable = p_sum_sig complete

    while (true)
    {
#pragma HLS PIPELINE off
#ifndef __SYNTHESIS__
        if (input_stream.empty())
            break;
#endif

    // --- Phase 1: 接收 Packed 資料 ---
    recv_packed:
        for (int i = 0; i < FRAME_SYMBOLS; i++)
        {
#pragma HLS PIPELINE II = 1
            axis_word_32 pkt = input_stream.read();
            I_buffer[i] = pkt.data.range(15, 0);
            Q_buffer[i] = pkt.data.range(31, 16);
        }

    // --- Phase 2: SNR 統計與初始化 ---
    init_snr:
        for (int k = 0; k < 4; k++)
        {
#pragma HLS UNROLL
            // ✅ 使用標準的迴圈清零，確保硬體與 C-Sim 行為一致
            p_sum_err[k] = 0.0f;
            p_sum_sig[k] = 0.0f;
        }

    calc_snr:
        for (int i = 0; i < FRAME_SYMBOLS; i++)
        {
#pragma HLS PIPELINE II = 1
            fixed16 val_I = (fixed16)bits_to_half(I_buffer[i]);
            fixed16 val_Q = (fixed16)bits_to_half(Q_buffer[i]);

            fixed16 avI = (val_I < 0) ? (fixed16)-val_I : val_I;
            fixed16 avQ = (val_Q < 0) ? (fixed16)-val_Q : val_Q;

            fixed16 sel_I = (avQ < avI) ? C1_FX : S1_FX;
            fixed16 sel_Q = (avQ < avI) ? S1_FX : C1_FX;
            fixed16 ref_I = (val_I > 0) ? sel_I : (fixed16)-sel_I;
            fixed16 ref_Q = (val_Q > 0) ? sel_Q : (fixed16)-sel_Q;

            float err_sq = (float)((val_I - ref_I) * (val_I - ref_I) + (val_Q - ref_Q) * (val_Q - ref_Q));
            float sig_sq = (float)(ref_I * ref_I + ref_Q * ref_Q);

            int idx = i & 0x3;
            p_sum_err[idx] += err_sq;
            p_sum_sig[idx] += sig_sq;
        }

        // 總結統計量
        float sum_err = p_sum_err[0] + p_sum_err[1] + p_sum_err[2] + p_sum_err[3];
        float sum_sig = p_sum_sig[0] + p_sum_sig[1] + p_sum_sig[2] + p_sum_sig[3];

        float noise_pwr = (sum_err < 1e-6f) ? 1e-6f : sum_err;
        float csi_f = (4.0f * DIST_8PSK_VAL) * (sum_sig / noise_pwr);

        // ✅ 依照你的需求，保留寫死的 128.0f
        float csi_factor = (csi_f > 128.0f) ? 128.0f : csi_f;

        // [偵錯點]：列印關鍵數值確認
#ifndef __SYNTHESIS__
        printf("\n--- SNR Estimation Result ---\n");
        printf("  Sum Signal (P_sig): %f\n", sum_sig);
        printf("  Sum Error  (P_err): %f\n", sum_err);
        printf("  Calculated CSI Factor: %f\n", csi_factor);
        printf("-----------------------------\n");
#endif

    // --- Phase 3: 輸出 LLR ($II=3$) ---
    process_output:
        for (int i = 0; i < FRAME_SYMBOLS; i++)
        {
#pragma HLS PIPELINE II = 3
            fixed16 vI = (fixed16)bits_to_half(I_buffer[i]);
            fixed16 vQ = (fixed16)bits_to_half(Q_buffer[i]);
            fixed16 avI = (vI < 0) ? (fixed16)-vI : vI;
            fixed16 avQ = (vQ < 0) ? (fixed16)-vQ : vQ;

            float m0 = (float)((avI - avQ) * SQRT2_INV_FX);
            ap_uint<8> llr_b0 = quantize_llr(m0 * csi_factor);
            ap_uint<8> llr_b1 = quantize_llr((float)vI * csi_factor);
            ap_uint<8> llr_b2 = quantize_llr((float)vQ * csi_factor);

            write_output(output_stream, llr_b0, false);
            write_output(output_stream, llr_b1, false);
            write_output(output_stream, llr_b2, (i == FRAME_SYMBOLS - 1));
        }
    }
}