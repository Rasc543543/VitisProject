#ifndef OFDM_CFO_SYNC_H
#define OFDM_CFO_SYNC_H

#include <ap_axi_sdata.h>
#include <ap_fixed.h>
#include <ap_int.h>
#include <complex>
#include <hls_math.h>
#include <hls_stream.h>

// ============================================================================
// 系統與演算法參數
// ============================================================================
constexpr int FFT_LEN = 2048;
constexpr int CP_LEN = 256;
constexpr int SUB_CP_LEN = 128;

// ============================================================================
// 定點數 (Fixed-Point) 型態定義與位元成長控制
// ============================================================================
typedef ap_axiu<32, 0, 0, 0> axis_word;

// 用於內部 Task 傳輸的結構體（HLS 禁止將 ap_axis/ap_axiu 用作非介面的內部 stream）
struct internal_axis_t {
  ap_uint<32> data;
  bool last;
};
typedef ap_fixed<16, 1, AP_TRN, AP_WRAP> data_t;
typedef std::complex<data_t> complex_data_t;
typedef ap_fixed<32, 2, AP_TRN, AP_WRAP> mult_t;
typedef std::complex<mult_t> complex_mult_t;
typedef ap_fixed<25, 9, AP_TRN, AP_WRAP> acc_t;
typedef std::complex<acc_t> complex_acc_t;
typedef ap_fixed<40, 9, AP_TRN, AP_WRAP> metric_t;

// 儲存並傳遞三個鎖定相關值的結構體
struct cfo_corr_t {
  complex_acc_t corr1;
  complex_acc_t corr2;
  complex_acc_t corr3;
};

// 定義高精度度量型態防止平方溢位
typedef ap_fixed<40, 18, AP_TRN, AP_WRAP> sq_metric_t;

// CFO 估測與補償使用浮點
typedef float cfo_t;

// ============================================================================
// Top Function 宣告
// ============================================================================
void ofdm_cfo_sync(hls::stream<axis_word> &input_stream,
                   hls::stream<axis_word> &output_stream);

#endif // OFDM_CFO_SYNC_H