#include "ofdm_cfo_sync.h"
#include <cmath>
#include <complex>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace std;

int main() {
  cout << "=====================================================" << endl;
  cout << " OFDM CFO Sync HLS Testbench (Vitis C Simulation)" << endl;
  cout << "=====================================================" << endl;

  hls::stream<axis_word> input_stream("tb_input_stream");
  hls::stream<axis_word> output_stream("tb_output_stream");

  // --------------------------------------------------------
  // 1. 產生模擬訊號 (配置 3 個封包，帶有不同頻偏)
  // --------------------------------------------------------
  vector<complex<float>> tx_signal;
  default_random_engine gen(123);
  normal_distribution<float> noise_dist(0.0, 0.01); // 雜訊小一點，以便精準評估定點與演算法誤差
  uniform_real_distribution<float> data_dist(-0.7, 0.7);

  const int NUM_PACKETS = 10;
  const int GAP_SAMPLES = 512;
  const float PI_VAL = 3.14159265358979323846f;

  float sim_cfos[NUM_PACKETS] = {-7.5f, -4.5f, -1.45f, -0.5f, 0.0f, 0.5f, 1.5f, 4.5f, 7.5f, 7.8f};
  int packet_start_indices[NUM_PACKETS];

  // 保存 3 個封包最原始的、完全沒頻偏的黃金理想時域 Payload (2048點)
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

    // 組裝單個封包的時域結構 (128 + 128 + 2048 = 2304 點)
    vector<complex<float>> packet_raw;
    for (int i = 0; i < SUB_CP_LEN; i++)
      packet_raw.push_back(tail_128[i]); // 第一段 CP
    for (int i = 0; i < SUB_CP_LEN; i++)
      packet_raw.push_back(tail_128[i]); // 第二段 CP
    for (int i = 0; i < FFT_LEN; i++)
      packet_raw.push_back(payload[i]); // 原始主體

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

  // 後段雜訊以推擠 pipeline 輸出
  for (int i = 0; i < 5000; i++) {
    tx_signal.push_back(complex<float>(noise_dist(gen), noise_dist(gen)));
  }

  cout << "[INFO] Total injected samples: " << tx_signal.size() << endl;
  for (int p = 0; p < NUM_PACKETS; p++) {
    cout << "  - Packet #" << (p + 1) << " injected at sample index: " 
         << packet_start_indices[p] << " with simulated CFO: " << sim_cfos[p] << endl;
  }
  cout << "=====================================================" << endl;

  // --------------------------------------------------------
  // 2. 推送至 HLS 輸入端
  // --------------------------------------------------------
  for (size_t i = 0; i < tx_signal.size(); i++) {
    ap_int<16> raw_i = (ap_int<16>)(tx_signal[i].real() * 32768.0f);
    ap_int<16> raw_q = (ap_int<16>)(tx_signal[i].imag() * 32768.0f);

    axis_word din;
    din.data.range(15, 0) = raw_i;
    din.data.range(31, 16) = raw_q;
    din.keep = 0xF;
    din.strb = 0xF;
    din.last = 0;  // ADC 連續串流，不提供外部封包邊界資訊
    input_stream.write(din);
  }

  // --------------------------------------------------------
  // 3. 執行 HLS 核心
  // --------------------------------------------------------
  ofdm_cfo_sync(input_stream, output_stream);

  // --------------------------------------------------------
  // 4. 解析輸出並進行誤差分析
  // --------------------------------------------------------
  int rx_sample_idx = 0;
  int symbol_detected_count = 0;

  double max_err_i = 0.0;
  double max_err_q = 0.0;
  double sum_sq_err = 0.0;
  int total_rx_samples = 0;

  while (!output_stream.empty()) {
    axis_word dout = output_stream.read();

    ap_int<16> rx_i_raw = dout.data.range(15, 0);
    ap_int<16> rx_q_raw = dout.data.range(31, 16);
    double rx_i = (double)rx_i_raw / 32768.0;
    double rx_q = (double)rx_q_raw / 32768.0;

    if (rx_sample_idx == 0) {
      symbol_detected_count++;
      max_err_i = 0.0;
      max_err_q = 0.0;
      sum_sq_err = 0.0;
      cout << "-----------------------------------------------------" << endl;
      cout << "[TESTBENCH] Captured Output for Packet #" << symbol_detected_count << endl;
      cout << "[TESTBENCH] Target Simulated CFO: " << sim_cfos[symbol_detected_count - 1] << endl;
    }

    complex<double> t_ideal = golden_time_payloads[symbol_detected_count - 1][rx_sample_idx];
    double err_i = abs(t_ideal.real() - rx_i);
    double err_q = abs(t_ideal.imag() - rx_q);
    
    if (err_i > max_err_i) max_err_i = err_i;
    if (err_q > max_err_q) max_err_q = err_q;
    sum_sq_err += (err_i * err_i + err_q * err_q);
    total_rx_samples++;

    // 每 500 個點抽樣列印一次
    if (rx_sample_idx % 500 == 0 || rx_sample_idx == 10|| rx_sample_idx == 1|| rx_sample_idx == 2|| rx_sample_idx == 3|| rx_sample_idx == 4|| rx_sample_idx == 5) {
      cout << fixed << setprecision(5);
      cout << "  Sample " << setw(4) << rx_sample_idx
           << " | TX Ideal: (" << setw(8) << t_ideal.real() << ", " << setw(8) << t_ideal.imag() << ")"
           << " | RX Corrected: (" << setw(8) << rx_i << ", " << setw(8) << rx_q << ")"
           << " | Error: (" << setw(8) << err_i << ", " << setw(8) << err_q << ")" << endl;
    }

    rx_sample_idx++;

    if (dout.last == 1) {
      double mse = sum_sq_err / (double)FFT_LEN;
      cout << fixed << setprecision(6);
      cout << "[TESTBENCH] Packet #" << symbol_detected_count << " Finished!" << endl;
      cout << "  - Checked Samples: " << rx_sample_idx << " (Expected 2048)" << endl;
      cout << "  - Max Real Error:  " << max_err_i << endl;
      cout << "  - Max Imag Error:  " << max_err_q << endl;
      cout << "  - Mean Squared Error (MSE): " << mse << endl;
      
      if (rx_sample_idx != FFT_LEN) {
        cout << "[ERROR] Packet length mismatch! Expected 2048, got " << rx_sample_idx << endl;
        return 0;
      }
      
      // 驗證誤差是否在合理範圍內（定點量化誤差與演算法誤差應小於 0.05）
      if (mse > 0.005) {
        cout << "[WARNING] MSE is relatively high! Check NCO rotation direction or precision loss." << endl;
      } else {
        cout << "[PASS] Packet #" << symbol_detected_count << " CFO correction precision within budget." << endl;
      }
      
      rx_sample_idx = 0;
    }
  }

  cout << "=====================================================" << endl;
  if (symbol_detected_count != NUM_PACKETS) {
    cout << "[ERROR] Expected " << NUM_PACKETS << " packets, but only detected "
         << symbol_detected_count << "!" << endl;
    return 0;
  }

  cout << "[SUCCESS] Vitis Testbench finished successfully!" << endl;
  cout << "=====================================================" << endl;
  return 0;
}