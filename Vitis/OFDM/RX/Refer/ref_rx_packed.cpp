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
#pragma HLS ARRAY_PARTITION variable = buf_i type = cyclic factor = 8
#pragma HLS ARRAY_PARTITION variable = buf_q type = cyclic factor = 8

Load_Packed:
    for (int i = 0; i < FFT_LENGTH; i++)
    {
#pragma HLS PIPELINE II = 1
        axis_t pkt = input_stream.read();
        buf_i[i] = pkt.data(15, 0);
        buf_q[i] = pkt.data(31, 16);
    }

    int64_t max_metric = -1;
    int     best_cfo   = 0;

    // Main scan: 17 iterations, all 8 offsets within ±CFO_RANGE
    // ap_int<16>*ap_int<16> → ap_int<32>, accumulated into int64_t; rotating slot distance=8
Loop_CFO_Scan:
    for (int base_off = -CFO_RANGE; base_off + 7 <= CFO_RANGE; base_off += 8)
    {
#pragma HLS LOOP_TRIPCOUNT min=17 max=17

        int64_t pm0[8]={}, pm1[8]={}, pm2[8]={}, pm3[8]={};
        int64_t pm4[8]={}, pm5[8]={}, pm6[8]={}, pm7[8]={};
#pragma HLS ARRAY_PARTITION variable = pm0 complete
#pragma HLS ARRAY_PARTITION variable = pm1 complete
#pragma HLS ARRAY_PARTITION variable = pm2 complete
#pragma HLS ARRAY_PARTITION variable = pm3 complete
#pragma HLS ARRAY_PARTITION variable = pm4 complete
#pragma HLS ARRAY_PARTITION variable = pm5 complete
#pragma HLS ARRAY_PARTITION variable = pm6 complete
#pragma HLS ARRAY_PARTITION variable = pm7 complete

    Loop_Metric_1:
        for (int p = 0; p < 136; p++)
        {
#pragma HLS PIPELINE II = 1
            int k = p & 0x7;
            int b = LEFT_GUARD + base_off + p * 7;

            { ap_int<16> ri=buf_i[b  ],rq=buf_q[b  ]; pm0[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+1],rq=buf_q[b+1]; pm1[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+2],rq=buf_q[b+2]; pm2[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+3],rq=buf_q[b+3]; pm3[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+4],rq=buf_q[b+4]; pm4[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+5],rq=buf_q[b+5]; pm5[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+6],rq=buf_q[b+6]; pm6[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+7],rq=buf_q[b+7]; pm7[k]+=ri*ri+rq*rq; }
        }

    Loop_Metric_2:
        for (int p = 137; p < PILOT_TONES; p++)
        {
#pragma HLS PIPELINE II = 1
            int k = p & 0x7;
            int b = LEFT_GUARD + base_off + p * 7;

            { ap_int<16> ri=buf_i[b  ],rq=buf_q[b  ]; pm0[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+1],rq=buf_q[b+1]; pm1[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+2],rq=buf_q[b+2]; pm2[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+3],rq=buf_q[b+3]; pm3[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+4],rq=buf_q[b+4]; pm4[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+5],rq=buf_q[b+5]; pm5[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+6],rq=buf_q[b+6]; pm6[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+7],rq=buf_q[b+7]; pm7[k]+=ri*ri+rq*rq; }
        }

    Loop_Pick_Best:
        {
            int64_t m;
            m=pm0[0]+pm0[1]+pm0[2]+pm0[3]+pm0[4]+pm0[5]+pm0[6]+pm0[7]; if(m>max_metric){max_metric=m;best_cfo=base_off  ;}
            m=pm1[0]+pm1[1]+pm1[2]+pm1[3]+pm1[4]+pm1[5]+pm1[6]+pm1[7]; if(m>max_metric){max_metric=m;best_cfo=base_off+1;}
            m=pm2[0]+pm2[1]+pm2[2]+pm2[3]+pm2[4]+pm2[5]+pm2[6]+pm2[7]; if(m>max_metric){max_metric=m;best_cfo=base_off+2;}
            m=pm3[0]+pm3[1]+pm3[2]+pm3[3]+pm3[4]+pm3[5]+pm3[6]+pm3[7]; if(m>max_metric){max_metric=m;best_cfo=base_off+3;}
            m=pm4[0]+pm4[1]+pm4[2]+pm4[3]+pm4[4]+pm4[5]+pm4[6]+pm4[7]; if(m>max_metric){max_metric=m;best_cfo=base_off+4;}
            m=pm5[0]+pm5[1]+pm5[2]+pm5[3]+pm5[4]+pm5[5]+pm5[6]+pm5[7]; if(m>max_metric){max_metric=m;best_cfo=base_off+5;}
            m=pm6[0]+pm6[1]+pm6[2]+pm6[3]+pm6[4]+pm6[5]+pm6[6]+pm6[7]; if(m>max_metric){max_metric=m;best_cfo=base_off+6;}
            m=pm7[0]+pm7[1]+pm7[2]+pm7[3]+pm7[4]+pm7[5]+pm7[6]+pm7[7]; if(m>max_metric){max_metric=m;best_cfo=base_off+7;}
        }
    }

    // Epilogue: base_off=66, only offsets 66~70 within CFO_RANGE (pm0~pm4)
    {
        const int base_off = 66;
        int64_t pm0[8]={}, pm1[8]={}, pm2[8]={}, pm3[8]={}, pm4[8]={};
#pragma HLS ARRAY_PARTITION variable = pm0 complete
#pragma HLS ARRAY_PARTITION variable = pm1 complete
#pragma HLS ARRAY_PARTITION variable = pm2 complete
#pragma HLS ARRAY_PARTITION variable = pm3 complete
#pragma HLS ARRAY_PARTITION variable = pm4 complete

    Loop_Metric_Last_1:
        for (int p = 0; p < 136; p++)
        {
#pragma HLS PIPELINE II = 1
            int k = p & 0x7;
            int b = LEFT_GUARD + base_off + p * 7;

            { ap_int<16> ri=buf_i[b  ],rq=buf_q[b  ]; pm0[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+1],rq=buf_q[b+1]; pm1[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+2],rq=buf_q[b+2]; pm2[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+3],rq=buf_q[b+3]; pm3[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+4],rq=buf_q[b+4]; pm4[k]+=ri*ri+rq*rq; }
        }

    Loop_Metric_Last_2:
        for (int p = 137; p < PILOT_TONES; p++)
        {
#pragma HLS PIPELINE II = 1
            int k = p & 0x7;
            int b = LEFT_GUARD + base_off + p * 7;

            { ap_int<16> ri=buf_i[b  ],rq=buf_q[b  ]; pm0[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+1],rq=buf_q[b+1]; pm1[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+2],rq=buf_q[b+2]; pm2[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+3],rq=buf_q[b+3]; pm3[k]+=ri*ri+rq*rq; }
            { ap_int<16> ri=buf_i[b+4],rq=buf_q[b+4]; pm4[k]+=ri*ri+rq*rq; }
        }

    Loop_Pick_Best_Last:
        {
            int64_t m;
            m=pm0[0]+pm0[1]+pm0[2]+pm0[3]+pm0[4]+pm0[5]+pm0[6]+pm0[7]; if(m>max_metric){max_metric=m;best_cfo=base_off  ;}
            m=pm1[0]+pm1[1]+pm1[2]+pm1[3]+pm1[4]+pm1[5]+pm1[6]+pm1[7]; if(m>max_metric){max_metric=m;best_cfo=base_off+1;}
            m=pm2[0]+pm2[1]+pm2[2]+pm2[3]+pm2[4]+pm2[5]+pm2[6]+pm2[7]; if(m>max_metric){max_metric=m;best_cfo=base_off+2;}
            m=pm3[0]+pm3[1]+pm3[2]+pm3[3]+pm3[4]+pm3[5]+pm3[6]+pm3[7]; if(m>max_metric){max_metric=m;best_cfo=base_off+3;}
            m=pm4[0]+pm4[1]+pm4[2]+pm4[3]+pm4[4]+pm4[5]+pm4[6]+pm4[7]; if(m>max_metric){max_metric=m;best_cfo=base_off+4;}
        }
    }

    int base = LEFT_GUARD + best_cfo;

Loop_Process_Packed:
    for (int i = 0; i < PILOT_TONES - 1; i++)
    {
#pragma HLS PIPELINE off
        if (i == 136) continue;

        int i2   = (i == 135) ? 137 : (i + 1);
        int idx1 = base + i  * 7;
        int idx2 = base + i2 * 7;

        float p_inv1  = (float)PILOT_ROM_INV[i];
        float h1_real = (float)fixed16_to_half(buf_i[idx1]) * p_inv1;
        float h1_imag = (float)fixed16_to_half(buf_q[idx1]) * p_inv1;

        float p_inv2  = (float)PILOT_ROM_INV[i2];
        float h2_real = (float)fixed16_to_half(buf_i[idx2]) * p_inv2;
        float h2_imag = (float)fixed16_to_half(buf_q[idx2]) * p_inv2;

        float inv_span = (i == 135) ? (1.0f / 14.0f) : (1.0f / 7.0f);
        float m_real   = (h2_real - h1_real) * inv_span;
        float m_imag   = (h2_imag - h1_imag) * inv_span;

        int k_max = (i == 135) ? 13 : 6;

Loop_Interp:
        for (int k = 1; k <= k_max; k++)
        {
#pragma HLS PIPELINE II = 1
#pragma HLS LOOP_TRIPCOUNT min=6 max=13
            float h_r = h1_real + (float)k * m_real;
            float h_i = h1_imag + (float)k * m_imag;

            if (i == 135 && k == 7) continue;

            int   didx  = idx1 + k;
            float d_i   = (float)fixed16_to_half(buf_i[didx]);
            float d_q   = (float)fixed16_to_half(buf_q[didx]);

            float num_i = d_i * h_r + d_q * h_i;
            float num_q = d_q * h_r - d_i * h_i;

            float h_power = h_r * h_r + h_i * h_i;
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
