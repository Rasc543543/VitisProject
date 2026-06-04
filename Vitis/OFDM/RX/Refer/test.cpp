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

      half max_metric = (half)-1.0f;
      int  best_cfo   = 0;

  Loop_CFO_Scan:
      for (int base_off = -CFO_RANGE; base_off <= CFO_RANGE; base_off += 8)
      {
  #pragma HLS LOOP_TRIPCOUNT min=18 max=18

          half p_metric[8][4] = {};
  #pragma HLS ARRAY_PARTITION variable = p_metric complete dim = 0

      Loop_Metric:
          for (int p = 0; p < PILOT_TONES; p++)
          {
  #pragma HLS PIPELINE II = 1
              if (p == 136) continue;

              for (int n = 0; n < 8; n++)
              {
  #pragma HLS UNROLL
                  int offset = base_off + n;
                  if (offset > CFO_RANGE) continue;
                  int idx = LEFT_GUARD + offset + p * 7;
                  half vi = fixed16_to_half(buf_i[idx]);
                  half vq = fixed16_to_half(buf_q[idx]);
                  p_metric[n][p & 0x3] += (vi * vi + vq * vq);
              }
          }

      Loop_Pick_Best:
          for (int n = 0; n < 8; n++)
          {
  #pragma HLS UNROLL
              int offset = base_off + n;
              if (offset > CFO_RANGE) continue;
              half metric = p_metric[n][0] + p_metric[n][1]
                          + p_metric[n][2] + p_metric[n][3];
              if (metric > max_metric)
              {
                  max_metric = metric;
                  best_cfo   = offset;
              }
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
          float cur_h_r  = h1_real;
          float cur_h_i  = h1_imag;

          int k_max = (i == 135) ? 13 : 6;

  Loop_Interp:
          for (int k = 1; k <= k_max; k++)
          {
  #pragma HLS PIPELINE II = 1
  #pragma HLS LOOP_TRIPCOUNT min=6 max=13
              cur_h_r += m_real;
              cur_h_i += m_imag;

              if (i == 135 && k == 7) continue;

              int   didx  = idx1 + k;
              float d_i   = (float)fixed16_to_half(buf_i[didx]);
              float d_q   = (float)fixed16_to_half(buf_q[didx]);

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