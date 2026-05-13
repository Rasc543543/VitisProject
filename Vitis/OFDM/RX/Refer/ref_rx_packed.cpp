#include "ref_rx_defs.h"

#define CFO_TRACK_W 5  // locked state window: scan ±5 around last best_cfo

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

    // CFO tracking state: persists across frames
    static int  cfo_state  = 0;
    static bool cfo_locked = false;

Load_Packed:
    for (int i = 0; i < FFT_LENGTH; i++)
    {
#pragma HLS PIPELINE II = 1
        axis_t pkt = input_stream.read();
        buf_i[i] = pkt.data(15, 0);
        buf_q[i] = pkt.data(31, 16);
    }

    half max_metric = (half)-1.0f;
    int  new_best   = cfo_state;

    if (!cfo_locked)
    {
        // Unlocked: full 141-point scan
Loop_CFO_Scan:
        for (int offset = -CFO_RANGE; offset <= CFO_RANGE; offset++)
        {
            half p_metric[4] = {0, 0, 0, 0};
#pragma HLS ARRAY_PARTITION variable = p_metric complete

        Loop_Metric:
            for (int p = 0; p < PILOT_TONES; p++)
            {
#pragma HLS PIPELINE II = 1
                if (p == 136) continue; // skip corrupted DC bin
                int idx = LEFT_GUARD + offset + p * 7;
                half vi = fixed16_to_half(buf_i[idx]);
                half vq = fixed16_to_half(buf_q[idx]);
                p_metric[p & 0x3] += (vi * vi + vq * vq);
            }
            half metric = p_metric[0] + p_metric[1] + p_metric[2] + p_metric[3];
            if (metric > max_metric)
            {
                max_metric = metric;
                new_best   = offset;
            }
        }
        cfo_locked = true;
    }
    else
    {
        // Locked: scan ±W around last known best_cfo (11 points)
Loop_CFO_Track:
        for (int d = -CFO_TRACK_W; d <= CFO_TRACK_W; d++)
        {
#pragma HLS LOOP_TRIPCOUNT min=11 max=11
            int offset = cfo_state + d;
            if (offset < -CFO_RANGE || offset > CFO_RANGE) continue;

            half p_metric[4] = {0, 0, 0, 0};
#pragma HLS ARRAY_PARTITION variable = p_metric complete

        Loop_Track_Metric:
            for (int p = 0; p < PILOT_TONES; p++)
            {
#pragma HLS PIPELINE II = 1
                if (p == 136) continue; // skip corrupted DC bin
                int idx = LEFT_GUARD + offset + p * 7;
                half vi = fixed16_to_half(buf_i[idx]);
                half vq = fixed16_to_half(buf_q[idx]);
                p_metric[p & 0x3] += (vi * vi + vq * vq);
            }
            half metric = p_metric[0] + p_metric[1] + p_metric[2] + p_metric[3];
            if (metric > max_metric)
            {
                max_metric = metric;
                new_best   = offset;
            }
        }
        // boundary_hit: best drifted to window edge → rescan next frame
        int drift = new_best - cfo_state;
        if (drift >= CFO_TRACK_W || drift <= -CFO_TRACK_W)
            cfo_locked = false;
    }

    cfo_state     = new_best;
    int best_cfo  = cfo_state;

#ifndef __SYNTHESIS__
    extern int g_debug_best_cfo;
    g_debug_best_cfo = best_cfo;
#endif

    int base = LEFT_GUARD + best_cfo;

Loop_Process_Packed:
    for (int i = 0; i < PILOT_TONES - 1; i++)
    {
#pragma HLS PIPELINE off
        if (i == 136) continue; // merged into i==135 (DC bin bypass)

        // i==135: anchor on pilot[137] to skip corrupted DC bin at pilot[136]
        int i2   = (i == 135) ? 137 : (i + 1);
        int idx1 = base + i  * 7;
        int idx2 = base + i2 * 7;

        float p_inv1 = (float)PILOT_ROM_INV[i];
        float h1_real = (float)fixed16_to_half(buf_i[idx1]) * p_inv1;
        float h1_imag = (float)fixed16_to_half(buf_q[idx1]) * p_inv1;

        float p_inv2 = (float)PILOT_ROM_INV[i2];
        float h2_real = (float)fixed16_to_half(buf_i[idx2]) * p_inv2;
        float h2_imag = (float)fixed16_to_half(buf_q[idx2]) * p_inv2;

        // i==135: span=14, 1/14; otherwise: span=7, 1/7
        float inv_span = (i == 135) ? (1.0f / 14.0f) : (1.0f / 7.0f);
        float m_real = (h2_real - h1_real) * inv_span;
        float m_imag = (h2_imag - h1_imag) * inv_span;
        float cur_h_r = h1_real;
        float cur_h_i = h1_imag;

        int k_max = (i == 135) ? 13 : 6;

Loop_Interp:
        for (int k = 1; k <= k_max; k++)
        {
#pragma HLS PIPELINE II = 1
#pragma HLS LOOP_TRIPCOUNT min=6 max=13
            cur_h_r += m_real;
            cur_h_i += m_imag;

            if (i == 135 && k == 7) continue; // DC bin position: advance H but skip output

            int didx = idx1 + k;
            float d_i = (float)fixed16_to_half(buf_i[didx]);
            float d_q = (float)fixed16_to_half(buf_q[didx]);

            float num_i = d_i * cur_h_r + d_q * cur_h_i;
            float num_q = d_q * cur_h_r - d_i * cur_h_i;

            float h_power = cur_h_r * cur_h_r + cur_h_i * cur_h_i;
            if (h_power < 0.0001f) h_power = 1.0f;

            float out_i = num_i / h_power;
            float out_q = num_q / h_power;

            axis_t out_pkt;
            out_pkt.data = pack_iq((half)out_i, (half)out_q);
            out_pkt.keep = 0xF;
            out_pkt.strb = 0xF;
            out_pkt.last = (i == PILOT_TONES - 2 && k == 6) ? 1 : 0;
            output_stream.write(out_pkt);
        }
    }
}
