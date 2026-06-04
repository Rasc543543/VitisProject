#include "ofdm_cfo_sync.h"
#include "sin_lut.h"

enum sync_state_t {
    STATE_SEARCH,
    STATE_OUTPUT_PAYLOAD
};

static void unpack_axis(hls::stream<axis_word> &in_axis, hls::stream<complex_data_t> &out_data, hls::stream<bool> &out_clear_gate);
static void calc_power_and_corr(hls::stream<complex_data_t> &in_data, hls::stream<bool> &in_clear_gate, hls::stream<mult_t> &out_power, hls::stream<complex_mult_t> &out_corr1, hls::stream<complex_mult_t> &out_corr2, hls::stream<complex_mult_t> &out_corr3, hls::stream<complex_data_t> &out_data_bypass);
static void moving_sum_and_metric(hls::stream<complex_data_t> &in_data, hls::stream<mult_t> &in_power, hls::stream<complex_mult_t> &in_corr1, hls::stream<complex_mult_t> &in_corr2, hls::stream<complex_mult_t> &in_corr3, acc_t threshold_config, hls::stream<internal_axis_t> &out_payload, hls::stream<cfo_corr_t> &out_corr);
static void combined_correction(hls::stream<cfo_corr_t> &in_corr, hls::stream<internal_axis_t> &in_payload, hls::stream<axis_word> &out_axis);

void ofdm_cfo_sync(hls::stream<axis_word> &input_stream, hls::stream<axis_word> &output_stream) {
#pragma HLS INTERFACE axis port = input_stream
#pragma HLS INTERFACE axis port = output_stream
#pragma HLS INTERFACE ap_ctrl_none port = return
#pragma HLS DATAFLOW

  const acc_t INTERNAL_THRESHOLD = 0.5;

  hls::stream<complex_data_t>     stream_data("stream_data");
  hls::stream<bool>               stream_clear_gate("stream_clear_gate");
  hls::stream<complex_data_t>     stream_data_bypass("stream_data_bypass");
  hls::stream<mult_t>             stream_power("stream_power");
  hls::stream<complex_mult_t>     stream_corr1("stream_corr1");
  hls::stream<complex_mult_t>     stream_corr2("stream_corr2");
  hls::stream<complex_mult_t>     stream_corr3("stream_corr3");
  // Fix 2: template depth=2048 forces BRAM implementation (pragma alone gets overridden to depth=2)
  hls::stream<internal_axis_t, 2048> stream_payload("stream_payload");
  hls::stream<cfo_corr_t>            stream_locked_corr("stream_locked_corr");

#pragma HLS STREAM variable = stream_data        depth = 256
#pragma HLS STREAM variable = stream_clear_gate  depth = 256
#pragma HLS STREAM variable = stream_data_bypass depth = 256
#pragma HLS STREAM variable = stream_power       depth = 256
#pragma HLS STREAM variable = stream_corr1       depth = 256
#pragma HLS STREAM variable = stream_corr2       depth = 256
#pragma HLS STREAM variable = stream_corr3       depth = 256
#pragma HLS STREAM variable = stream_payload     depth = 2048 impl = bram
#pragma HLS STREAM variable = stream_locked_corr depth = 4    impl = fifo

  unpack_axis(input_stream, stream_data, stream_clear_gate);
  calc_power_and_corr(stream_data, stream_clear_gate, stream_power, stream_corr1, stream_corr2, stream_corr3, stream_data_bypass);
  moving_sum_and_metric(stream_data_bypass, stream_power, stream_corr1, stream_corr2, stream_corr3, INTERNAL_THRESHOLD, stream_payload, stream_locked_corr);
  combined_correction(stream_locked_corr, stream_payload, output_stream);
}

static void unpack_axis(hls::stream<axis_word> &in_axis, hls::stream<complex_data_t> &out_data, hls::stream<bool> &out_clear_gate) {
  while (true) {
#pragma HLS PIPELINE II = 1
#ifndef __SYNTHESIS__
    if (in_axis.empty()) break;
#endif
    axis_word curr_word = in_axis.read();
    data_t i_fixed, q_fixed;
    i_fixed.range(15, 0) = curr_word.data.range(15, 0);
    q_fixed.range(15, 0) = curr_word.data.range(31, 16);
    out_data.write(complex_data_t(i_fixed, q_fixed));
    out_clear_gate.write(curr_word.last ? true : false);
  }
}

static void calc_power_and_corr(hls::stream<complex_data_t> &in_data, hls::stream<bool> &in_clear_gate, hls::stream<mult_t> &out_power, hls::stream<complex_mult_t> &out_corr1, hls::stream<complex_mult_t> &out_corr2, hls::stream<complex_mult_t> &out_corr3, hls::stream<complex_data_t> &out_data_bypass) {
  while (true) {
#pragma HLS PIPELINE II = 1
#ifndef __SYNTHESIS__
    if (in_data.empty()) break;
#endif
    static complex_data_t delay_mem_2048[FFT_LEN];
#pragma HLS BIND_STORAGE variable = delay_mem_2048 type = ram_2p impl = bram
    static ap_uint<11> wr_ptr_2048 = 0;

    static complex_data_t delay_line_128[SUB_CP_LEN];
#pragma HLS BIND_STORAGE variable = delay_line_128 type = ram_2p impl = lutram
    static ap_uint<7> ptr_128 = 0;

    complex_data_t x_n = in_data.read();
    bool clear_gate    = in_clear_gate.read();

    complex_data_t x_n_2048 = delay_mem_2048[wr_ptr_2048];
    delay_mem_2048[wr_ptr_2048] = x_n;

    if (wr_ptr_2048 == FFT_LEN - 1) wr_ptr_2048 = 0;
    else wr_ptr_2048++;

    complex_data_t x_n_2176 = delay_line_128[ptr_128];
    delay_line_128[ptr_128] = x_n_2048;

    if (ptr_128 == SUB_CP_LEN - 1) ptr_128 = 0;
    else ptr_128++;

    mult_t pwr = x_n.real() * x_n.real() + x_n.imag() * x_n.imag();
    mult_t c1_i = x_n.real() * x_n_2176.real() + x_n.imag() * x_n_2176.imag();
    mult_t c1_q = x_n.imag() * x_n_2176.real() - x_n.real() * x_n_2176.imag();
    mult_t c2_i = x_n.real() * x_n_2048.real() + x_n.imag() * x_n_2048.imag();
    mult_t c2_q = x_n.imag() * x_n_2048.real() - x_n.real() * x_n_2048.imag();
    mult_t c3_i = x_n_2048.real() * x_n_2176.real() + x_n_2048.imag() * x_n_2176.imag();
    mult_t c3_q = x_n_2048.imag() * x_n_2176.real() - x_n_2048.real() * x_n_2176.imag();

#pragma HLS BIND_OP variable = pwr  op = mul impl = dsp
#pragma HLS BIND_OP variable = c1_i op = mul impl = dsp
#pragma HLS BIND_OP variable = c1_q op = mul impl = dsp
#pragma HLS BIND_OP variable = c2_i op = mul impl = dsp
#pragma HLS BIND_OP variable = c2_q op = mul impl = dsp
#pragma HLS BIND_OP variable = c3_i op = mul impl = dsp
#pragma HLS BIND_OP variable = c3_q op = mul impl = dsp

    out_power.write(pwr);
    out_corr1.write(complex_mult_t(c1_i, c1_q));
    out_corr2.write(complex_mult_t(c2_i, c2_q));
    out_corr3.write(complex_mult_t(c3_i, c3_q));
    out_data_bypass.write(x_n_2048);
  }
}

static void moving_sum_and_metric(hls::stream<complex_data_t> &in_data, hls::stream<mult_t> &in_power, hls::stream<complex_mult_t> &in_corr1, hls::stream<complex_mult_t> &in_corr2, hls::stream<complex_mult_t> &in_corr3, acc_t threshold_config, hls::stream<internal_axis_t> &out_payload, hls::stream<cfo_corr_t> &out_corr) {
  static sync_state_t  fsm_state = STATE_SEARCH;
  static ap_uint<16>   counter   = 0;

  static acc_t         delay_pwr  [SUB_CP_LEN];
  static complex_acc_t delay_corr1[SUB_CP_LEN];
  static complex_acc_t delay_corr2[SUB_CP_LEN];
  static complex_acc_t delay_corr3[SUB_CP_LEN];
#pragma HLS BIND_STORAGE variable = delay_pwr   type = ram_2p impl = lutram
#pragma HLS BIND_STORAGE variable = delay_corr1 type = ram_2p impl = lutram
#pragma HLS BIND_STORAGE variable = delay_corr2 type = ram_2p impl = lutram
#pragma HLS BIND_STORAGE variable = delay_corr3 type = ram_2p impl = lutram
  static ap_uint<7> ptr_sum = 0;

  static acc_t         acc_pwr = 0;
  static complex_acc_t acc_corr1(0, 0), acc_corr2(0, 0), acc_corr3(0, 0);

  static sq_metric_t   metric_m0 = 0; static sq_metric_t   metric_m1 = 0;
  static sq_metric_t   metric_m2 = 0; static sq_metric_t   metric_m3 = 0;
  static sq_metric_t   metric_m4 = 0;
  static sq_metric_t   thresh_m2 = 0;
  static sq_metric_t   thresh_m3 = 0;

  static complex_acc_t corr1_m1(0, 0), corr2_m1(0, 0), corr3_m1(0, 0);
  static complex_acc_t corr1_m2(0, 0), corr2_m2(0, 0), corr3_m2(0, 0);
  static complex_acc_t corr1_m3(0, 0), corr2_m3(0, 0), corr3_m3(0, 0);
  static complex_acc_t locked_corr1(0, 0), locked_corr2(0, 0), locked_corr3(0, 0);

  static complex_data_t x_n_m1 = complex_data_t(0, 0);
  static complex_data_t x_n_m2 = complex_data_t(0, 0);
  static complex_data_t x_n_m3 = complex_data_t(0, 0);
  static complex_data_t held_sample = complex_data_t(0, 0);

  static ap_uint<8> search_warmup = 0;

  while (true) {
#pragma HLS PIPELINE II = 1
#ifndef __SYNTHESIS__
    if (in_data.empty()) break;
#endif

    complex_data_t x_n   = in_data.read();
    acc_t          pwr_n = in_power.read();
    complex_acc_t  c1_n  = in_corr1.read();
    complex_acc_t  c2_n  = in_corr2.read();
    complex_acc_t  c3_n  = in_corr3.read();

    acc_t          pwr_d = delay_pwr  [ptr_sum];
    complex_acc_t  c1_d  = delay_corr1[ptr_sum];
    complex_acc_t  c2_d  = delay_corr2[ptr_sum];
    complex_acc_t  c3_d  = delay_corr3[ptr_sum];

    delay_pwr  [ptr_sum] = pwr_n;
    delay_corr1[ptr_sum] = c1_n;
    delay_corr2[ptr_sum] = c2_n;
    delay_corr3[ptr_sum] = c3_n;

    if (ptr_sum == SUB_CP_LEN - 1) ptr_sum = 0;
    else ptr_sum++;

    acc_t pwr_to_sub = (search_warmup < SUB_CP_LEN) ? (acc_t)0 : pwr_d;
    complex_acc_t c1_to_sub = (search_warmup < SUB_CP_LEN) ? complex_acc_t(0, 0) : c1_d;
    complex_acc_t c2_to_sub = (search_warmup < SUB_CP_LEN) ? complex_acc_t(0, 0) : c2_d;
    complex_acc_t c3_to_sub = (search_warmup < SUB_CP_LEN) ? complex_acc_t(0, 0) : c3_d;

    acc_pwr = acc_pwr + pwr_n - pwr_to_sub;
    acc_corr1 = complex_acc_t(acc_corr1.real() + c1_n.real() - c1_to_sub.real(), acc_corr1.imag() + c1_n.imag() - c1_to_sub.imag());
    acc_corr2 = complex_acc_t(acc_corr2.real() + c2_n.real() - c2_to_sub.real(), acc_corr2.imag() + c2_n.imag() - c2_to_sub.imag());
    acc_corr3 = complex_acc_t(acc_corr3.real() + c3_n.real() - c3_to_sub.real(), acc_corr3.imag() + c3_n.imag() - c3_to_sub.imag());

    sq_metric_t r_sq = (sq_metric_t)(acc_corr1.real() * acc_corr1.real());
    sq_metric_t i_sq = (sq_metric_t)(acc_corr1.imag() * acc_corr1.imag());
    sq_metric_t p_sq = (sq_metric_t)(acc_pwr * acc_pwr);

#pragma HLS BIND_OP variable = r_sq op = mul impl = dsp
#pragma HLS BIND_OP variable = i_sq op = mul impl = dsp
#pragma HLS BIND_OP variable = p_sq op = mul impl = dsp

    metric_m0 = r_sq + i_sq;
    sq_metric_t target_thresh = (sq_metric_t)(threshold_config * p_sq);
#pragma HLS BIND_OP variable = target_thresh op = mul impl = dsp

    switch (fsm_state) {
    case STATE_SEARCH: {
      bool warmup_done = (search_warmup >= SUB_CP_LEN);
      bool is_true_peak = warmup_done && (metric_m3 > thresh_m3) && (metric_m3 > metric_m2) && (metric_m2 > metric_m1) && (acc_pwr > (acc_t)1.0);

      if (search_warmup < SUB_CP_LEN) {
        search_warmup++;
      }

      if (is_true_peak) {
        search_warmup = 0;
        cfo_corr_t corr_val;
        corr_val.corr1 = corr1_m3;
        corr_val.corr2 = corr2_m3;
        corr_val.corr3 = corr3_m3;
        out_corr.write(corr_val);

        internal_axis_t out_pkt;
        out_pkt.data.range(15, 0)  = x_n_m2.real().range(15, 0);
        out_pkt.data.range(31, 16) = x_n_m2.imag().range(15, 0);
        out_pkt.last = false;
        out_payload.write(out_pkt);

        fsm_state   = STATE_OUTPUT_PAYLOAD;
        counter     = 1;
        metric_m0 = 0; metric_m1 = 0; metric_m2 = 0; metric_m3 = 0; metric_m4 = 0; thresh_m2 = 0; thresh_m3 = 0;
      } else {
        thresh_m3 = thresh_m2;
        thresh_m2 = target_thresh;
      }

      metric_m4 = metric_m3; metric_m3 = metric_m2; metric_m2 = metric_m1; metric_m1 = metric_m0;
      corr1_m3 = corr1_m2; corr2_m3 = corr2_m2; corr3_m3 = corr3_m2;
      corr1_m2 = corr1_m1; corr2_m2 = corr2_m1; corr3_m2 = corr3_m1;
      corr1_m1 = acc_corr1; corr2_m1 = acc_corr2; corr3_m1 = acc_corr3;
      break;
    }

    case STATE_OUTPUT_PAYLOAD: {
      internal_axis_t out_pkt;
      out_pkt.data.range(15, 0)  = x_n_m2.real().range(15, 0);
      out_pkt.data.range(31, 16) = x_n_m2.imag().range(15, 0);

      if (counter == FFT_LEN - 1) {
        out_pkt.last = true; fsm_state = STATE_SEARCH; counter = 0;
        metric_m0 = 0; metric_m1 = 0; metric_m2 = 0; metric_m3 = 0; metric_m4 = 0; thresh_m2 = 0; thresh_m3 = 0;
        x_n_m1 = complex_data_t(0, 0); x_n_m2 = complex_data_t(0, 0); x_n_m3 = complex_data_t(0, 0);

        acc_pwr = 0;
        acc_corr1 = complex_acc_t(0, 0);
        acc_corr2 = complex_acc_t(0, 0);
        acc_corr3 = complex_acc_t(0, 0);
        ptr_sum = 0;
        search_warmup = 0;

      } else {
        out_pkt.last = false; counter++;
      }
      out_payload.write(out_pkt);
      break;
    }
    }

    x_n_m3 = x_n_m2; x_n_m2 = x_n_m1; x_n_m1 = x_n;
  }
}

// cfo_estimator + cfo_correction merged: eliminates stream_phase_inc FIFO.
// Outer while reads corr (blocking), computes phase_inc (~71 cycles, non-pipelined),
// then inner for-loop II=1 reads only stream_payload — no intermediate FIFO stall.
static void combined_correction(hls::stream<cfo_corr_t> &in_corr, hls::stream<internal_axis_t> &in_payload, hls::stream<axis_word> &out_axis) {
  while (true) {
#ifndef __SYNTHESIS__
    if (in_corr.empty()) break;
#endif
    cfo_corr_t corr = in_corr.read();

    cfo_t phi1 = hls::atan2f((float)corr.corr1.imag(), (float)corr.corr1.real());
    cfo_t phi2 = hls::atan2f((float)corr.corr2.imag(), (float)corr.corr2.real());
    cfo_t phi3 = hls::atan2f((float)corr.corr3.imag(), (float)corr.corr3.real());

    cfo_t phi3_fine = phi1 - phi2;
    if (phi3_fine > 3.141592653589793f) phi3_fine -= 2.0f * 3.141592653589793f;
    else if (phi3_fine < -3.141592653589793f) phi3_fine += 2.0f * 3.141592653589793f;

    cfo_t phi3_corrected = phi3;
    if (phi3 > 2.5f && phi3_fine < -2.5f) {
      phi3_corrected = phi3 - 2.0f * 3.141592653589793f;
    } else if (phi3 < -2.5f && phi3_fine > 2.5f) {
      phi3_corrected = phi3 + 2.0f * 3.141592653589793f;
    }

    cfo_t est_cfo_rough = phi3_corrected * (8.0f / 3.141592653589793f);
    cfo_t expected_phi2 = est_cfo_rough * (2.0f * 3.141592653589793f);

    cfo_t k2_float       = (expected_phi2 - phi2) / (2.0f * 3.141592653589793f);
    int   k2             = (k2_float >= 0.0f) ? (int)(k2_float + 0.5f) : (int)(k2_float - 0.5f);
    cfo_t phi2_unwrapped = phi2 + 2.0f * 3.141592653589793f * (float)k2;

    cfo_t phase_inc = -phi2_unwrapped / (float)FFT_LEN;

#ifndef __SYNTHESIS__
    float est_cfo = -phase_inc * (float)FFT_LEN / (2.0f * 3.1415926535f);
    std::cout << "[HLS IP] Estimated phi1: " << phi1
              << ", phi2: " << phi2 << ", phi3: " << phi3 << std::endl;
    std::cout << "[HLS IP] Calculated phaseInc: " << phase_inc
              << " (Estimated CFO: " << est_cfo << ")" << std::endl;
#endif

    ap_fixed<16, 1, AP_TRN, AP_WRAP> phase_inc_fixed =
        (ap_fixed<16, 1, AP_TRN, AP_WRAP>)(phase_inc * 0.3183098861837907f);
    ap_fixed<16, 1, AP_TRN, AP_WRAP> phase_acc =
        (ap_fixed<16, 1, AP_TRN, AP_WRAP>)(phase_inc_fixed * (ap_fixed<16, 9>)255);

    for (int i = 0; i < FFT_LEN; i++) {
#pragma HLS PIPELINE II = 1
      internal_axis_t pkt = in_payload.read();

      phase_acc += phase_inc_fixed;

      ap_uint<8> idx     = phase_acc.range(15, 8);
      ap_uint<8> cos_idx = idx + 64;

      data_t sin_val = sin_lut[idx];
      data_t cos_val = sin_lut[cos_idx];

      data_t x_i, x_q;
      x_i.range(15, 0) = pkt.data.range(15, 0);
      x_q.range(15, 0) = pkt.data.range(31, 16);

      data_t mul_i_cos = (data_t)(x_i * cos_val);
      data_t mul_q_sin = (data_t)(x_q * sin_val);
      data_t mul_i_sin = (data_t)(x_i * sin_val);
      data_t mul_q_cos = (data_t)(x_q * cos_val);

#pragma HLS BIND_OP variable = mul_i_cos op = mul impl = dsp
#pragma HLS BIND_OP variable = mul_q_sin op = mul impl = dsp
#pragma HLS BIND_OP variable = mul_i_sin op = mul impl = dsp
#pragma HLS BIND_OP variable = mul_q_cos op = mul impl = dsp

      data_t out_i = (data_t)(mul_i_cos - mul_q_sin);
      data_t out_q = (data_t)(mul_i_sin + mul_q_cos);

      axis_word out_pkt;
      out_pkt.data.range(15, 0)  = out_i.range(15, 0);
      out_pkt.data.range(31, 16) = out_q.range(15, 0);
      out_pkt.keep = 0xF; out_pkt.strb = 0xF;
      out_pkt.last = (i == FFT_LEN - 1) ? 1 : 0;
      out_axis.write(out_pkt);
    }
  }
}
