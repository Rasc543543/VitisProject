#include "ofdm_cfo_sync.h"
#include <cmath>
#include <complex>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace std;

// 宣告輔助分析函式
int check_and_analyze_output(
    hls::stream<axis_word> &output_stream, 
    vector<vector<complex<double>>> &golden_time_payloads,
    int &symbol_detected_count, int rx_sample_idx,
    double &max_err_i, double &max_err_q, double &sum_sq_err,
    float sim_cfos[]
);

int main() {
  cout << "INFO: [COSIM 212-302] Starting C TB testing ... " << endl;
  cout << "=====================================================" << endl;
  cout << " OFDM CFO Sync HLS Testbench (Vitis C Simulation)" << endl;
  cout << "=====================================================" << endl;

  hls::stream<axis_word> input_stream("tb_input_stream");
  hls::stream<axis_word> output_stream("tb_output_stream");

  // --------------------------------------------------------
  // 1. 產生模擬訊號 (配置你原始正確的 128 CP + 128 CP 封包結構)
  // --------------------------------------------------------
  vector<complex<float>> tx_signal;
  default_random_engine gen(123);
  normal_distribution<float> noise_dist(0.0, 0.01); 
  uniform_real_distribution<float> data_dist(-0.7, 0.7);

  const int NUM_PACKETS = 10;
  const int GAP_SAMPLES = 512;
  const float PI_VAL = 3.14159265358979323846f;

  float sim_cfos[NUM_PACKETS] = {-7.5f, -4.5f, -1.45f, -0.5f, 0.0f, 0.5f, 1.5f, 4.5f, 7.5f, 7.8f};
  int packet_start_indices[NUM_PACKETS];

  vector<vector<complex<double>>> golden_time_payloads(
      NUM_PACKETS, vector<complex<double>>(FFT_LEN));

  int current_total_samples = 0;

  for (int p = 0; p < NUM_PACKETS; p++) {
    float current_cfo = sim_cfos[p];
    float current_phase = 0.0f;

    packet_start_indices[p] = current_total_samples;

    // 產生 2048 點原始時域資料
    vector<complex<float>> payload(FFT_LEN);
    for (int i = 0; i < FFT_LEN; i++) {
      float r = data_dist(gen);
      float img = data_dist(gen);
      payload[i] = complex<float>(r, img);
      golden_time_payloads[p][i] = complex<double>((double)r, (double)img);
    }

    // 擷取時域資料主體的最後 128 點 (SUB_CP_LEN)
    vector<complex<float>> tail_128(SUB_CP_LEN);
    for (int i = 0; i < SUB_CP_LEN; i++) {
      tail_128[i] = payload[FFT_LEN - SUB_CP_LEN + i];
    }

    // 組裝單個完整封包 (128 CP + 128 CP + 2048 點資料 = 2304 點)
    vector<complex<float>> packet_raw;
    for (int i = 0; i < SUB_CP_LEN; i++) packet_raw.push_back(tail_128[i]); 
    for (int i = 0; i < SUB_CP_LEN; i++) packet_raw.push_back(tail_128[i]); 
    for (int i = 0; i < FFT_LEN; i++)    packet_raw.push_back(payload[i]); 

    // 套用連續相位旋轉，模擬實體空中的 CFO 頻偏
    for (size_t i = 0; i < packet_raw.size(); i++) {
      float cos_val = cos(current_phase);
      float sin_val = sin(current_phase);
      complex<float> cfo_rotator(cos_val, sin_val);
      tx_signal.push_back(packet_raw[i] * cfo_rotator);

      current_phase += (2.0f * PI_VAL * current_cfo) / FFT_LEN;
      if (current_phase > 2.0f * PI_VAL)  current_phase -= 2.0f * PI_VAL;
      if (current_phase < -2.0f * PI_VAL) current_phase += 2.0f * PI_VAL;
    }

    current_total_samples += packet_raw.size();

    // 插入封包間隔雜訊
    if (p < NUM_PACKETS - 1) {
      for (int i = 0; i < GAP_SAMPLES; i++) {
        tx_signal.push_back(complex<float>(noise_dist(gen), noise_dist(gen)));
      }
      current_total_samples += GAP_SAMPLES;
    }
  }

  // 注入尾端雜訊以推擠最後的 pipeline 殘留
  for (int i = 0; i < 5000; i++) {
    tx_signal.push_back(complex<float>(noise_dist(gen), noise_dist(gen)));
  }

  cout << "[INFO] Total injected samples: " << tx_signal.size() << endl;
  for (int p = 0; p < NUM_PACKETS; p++) {
    cout << "   - Packet #" << (p + 1) << " injected at sample index: " 
         << packet_start_indices[p] << " with simulated CFO: " << sim_cfos[p] << endl;
  }
  cout << "=====================================================" << endl;

  // --------------------------------------------------------
  // 2. 模擬硬體大水庫蓄水與連續 38000 拍超強推擠 (與舊版 C-Sim 一致)
  // --------------------------------------------------------
  size_t tx_ptr = 0;
  int dma_cnt = 0;
  const int FIXED_DMA_SIZE = 4096; // 依然滿足 4096 點固定 DMA Block 傳輸特徵

  int rx_sample_idx = 0;
  int symbol_detected_count = 0;
  double max_err_i = 0.0, max_err_q = 0.0, sum_sq_err = 0.0;

  // 生產者蓄水階段
  while (tx_ptr < tx_signal.size()) {
    ap_int<16> raw_i = (ap_int<16>)(tx_signal[tx_ptr].real() * 32768.0f);
    ap_int<16> raw_q = (ap_int<16>)(tx_signal[tx_ptr].imag() * 32768.0f);

    axis_word din;
    din.data.range(15, 0) = raw_i;
    din.data.range(31, 16) = raw_q;
    din.keep = 0xF; din.strb = 0xF;
    
    if (dma_cnt == FIXED_DMA_SIZE - 1 || tx_ptr == tx_signal.size() - 1) {
      din.last = 1;
      dma_cnt = 0;
    } else {
      din.last = 0;
      dma_cnt++;
    }
    
    input_stream.write(din);
    tx_ptr++;
  }

  // 消費者推擠階段
  int total_ticks = tx_signal.size() + 5000;
  for (int tick = 0; tick < total_ticks; tick++) {
    ofdm_cfo_sync(input_stream, output_stream);

    while (!output_stream.empty()) {
      rx_sample_idx = check_and_analyze_output(
          output_stream, golden_time_payloads, symbol_detected_count, rx_sample_idx, 
          max_err_i, max_err_q, sum_sq_err, sim_cfos
      );
    }
  }

  cout << "=====================================================" << endl;
  if (symbol_detected_count != NUM_PACKETS) {
    cout << "[ERROR] Expected " << NUM_PACKETS << " packets, but only detected "
         << symbol_detected_count << "!" << endl;
    return 3; 
  }

  cout << "[SUCCESS] Vitis Co-simulation finished successfully!" << endl;
  cout << "=====================================================" << endl;
  return 0;
}

// 分析輸出
int check_and_analyze_output(
    hls::stream<axis_word> &output_stream, 
    vector<vector<complex<double>>> &golden_time_payloads,
    int &symbol_detected_count, int rx_sample_idx,
    double &max_err_i, double &max_err_q, double &sum_sq_err,
    float sim_cfos[]
) {
  axis_word dout = output_stream.read();

  ap_int<16> rx_i_raw = dout.data.range(15, 0);
  ap_int<16> rx_q_raw = dout.data.range(31, 16);
  double rx_i = (double)rx_i_raw / 32768.0;
  double rx_q = (double)rx_q_raw / 32768.0;

  if (rx_sample_idx == 0) {
    symbol_detected_count++;
    max_err_i = 0.0; max_err_q = 0.0; sum_sq_err = 0.0;
    cout << "-----------------------------------------------------" << endl;
    cout << "[TESTBENCH] Captured Output for Packet #" << symbol_detected_count << endl;
    cout << "[TESTBENCH] Target Simulated CFO: " << fixed << setprecision(6) << sim_cfos[symbol_detected_count - 1] << endl;
  }

  complex<double> t_ideal = golden_time_payloads[symbol_detected_count - 1][rx_sample_idx];
  double err_i = abs(t_ideal.real() - rx_i);
  double err_q = abs(t_ideal.imag() - rx_q);
  
  if (err_i > max_err_i) max_err_i = err_i;
  if (err_q > max_err_q) max_err_q = err_q;
  sum_sq_err += (err_i * err_i + err_q * err_q);

  if (rx_sample_idx % 500 == 0 || (rx_sample_idx >= 1 && rx_sample_idx <= 5) || rx_sample_idx == 10) {
    cout << fixed << setprecision(5);
    cout << "   Sample " << setw(4) << rx_sample_idx
         << " | TX Ideal: (" << setw(8) << t_ideal.real() << ", " << setw(8) << t_ideal.imag() << ")"
         << " | RX Corrected: (" << setw(8) << rx_i << ", " << setw(8) << rx_q << ")"
         << " | Error: (" << setw(8) << err_i << ", " << setw(8) << err_q << ")" << endl;
  }

  rx_sample_idx++;

  if (dout.last == 1) {
    double mse = sum_sq_err / (double)FFT_LEN;
    cout << "[TESTBENCH] Packet #" << symbol_detected_count << " Finished!" << endl;
    cout << "   - Checked Samples: " << rx_sample_idx << " (Expected 2048)" << endl;
    cout << "   - Max Real Error:  " << max_err_i << endl;
    cout << "   - Max Imag Error:  " << max_err_q << endl;
    cout << "   - Mean Squared Error (MSE): " << mse << endl;
    
    if (mse > 0.005) {
      cout << "[WARNING] MSE is relatively high! Check NCO rotation direction or precision loss." << endl;
    } else {
      cout << "[PASS] Packet #" << symbol_detected_count << " CFO correction precision within budget." << endl;
    }
    rx_sample_idx = 0;
  }
  return rx_sample_idx;
}