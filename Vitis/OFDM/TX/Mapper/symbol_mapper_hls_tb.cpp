// symbol_mapper_hls_tb.cpp
// [Updated for FP16 Output]
#include "symbol_mapper_hls.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include "hls_half.h" // <--- 引入

#define TOLERANCE 0.001f

// ... (Ref Tables 維持不變，用 float 即可) ...
static const float REF_QPSK_I[4] = {0.70710678f, -0.70710678f, 0.70710678f, -0.70710678f};
static const float REF_QPSK_Q[4] = {0.70710678f, 0.70710678f, -0.70710678f, -0.70710678f};
// ... (PSK8 Table 略) ...
static const float REF_PSK8_I[8] = {0.9238795f, 0.3826834f, -0.9238795f, -0.3826834f, 0.9238795f, 0.3826834f, -0.9238795f, -0.3826834f};
static const float REF_PSK8_Q[8] = {0.3826834f, 0.9238795f, 0.3826834f, 0.9238795f, -0.3826834f, -0.9238795f, -0.3826834f, -0.9238795f};

// [修改] Helper: 將 16-bit Raw Bits 轉回 Float (取代 fixed16_to_float)
float half16_to_float(ap_uint<16> raw)
{
    unsigned short us = (unsigned short)raw;
    half h_val = *(half *)&us;
    return (float)h_val;
}

// ... (generate_test_data 維持不變) ...
void generate_test_data(ap_uint<8> *test_input, float *golden_I, float *golden_Q, int input_bytes, bool is_qpsk)
{
    // 內容與之前相同
    for (int i = 0; i < input_bytes; i++)
        test_input[i] = rand() & 1;
    int bit_idx = 0;
    for (int sym_idx = 0; sym_idx < OUTPUT_SYMBOLS; sym_idx++)
    {
        int symbol_index = 0;
        if (is_qpsk)
        {
            int b0 = test_input[bit_idx++] & 1;
            int b1 = test_input[bit_idx++] & 1;
            symbol_index = (b1 << 1) | b0;
            golden_I[sym_idx] = REF_QPSK_I[symbol_index];
            golden_Q[sym_idx] = REF_QPSK_Q[symbol_index];
        }
        else
        {
            int b0 = test_input[bit_idx++] & 1;
            int b1 = test_input[bit_idx++] & 1;
            int b2 = test_input[bit_idx++] & 1;
            symbol_index = (b2 << 2) | (b1 << 1) | b0;
            golden_I[sym_idx] = REF_PSK8_I[symbol_index];
            golden_Q[sym_idx] = REF_PSK8_Q[symbol_index];
        }
    }
}

// [修改] 驗證 32-bit Output (FP16 Version)
int verify_output_32bit(hls::stream<axis_word_32> &output_stream,
                        float *golden_I, float *golden_Q,
                        const char *mode_name)
{
    int errors = 0;
    std::cout << "\n[" << mode_name << "] Verifying 32-bit packed FP16 output..." << std::endl;

    for (int i = 0; i < OUTPUT_SYMBOLS; i++)
    {
        axis_word_32 out_word = output_stream.read();

        // 解析 32-bit 打包格式
        // [31:16] = Q, [15:0] = I
        ap_uint<16> I_raw = out_word.data.range(15, 0);
        ap_uint<16> Q_raw = out_word.data.range(31, 16);

        // 使用 FP16 轉換
        float out_I = half16_to_float(I_raw);
        float out_Q = half16_to_float(Q_raw);

        // 驗證 (Logic 不變)
        if (fabs(out_I - golden_I[i]) > TOLERANCE)
        {
            if (errors < 10)
                std::cout << "  MISMATCH I[" << i << "]: got " << out_I << ", expected " << golden_I[i] << std::endl;
            errors++;
        }
        if (fabs(out_Q - golden_Q[i]) > TOLERANCE)
        {
            if (errors < 10)
                std::cout << "  MISMATCH Q[" << i << "]: got " << out_Q << ", expected " << golden_Q[i] << std::endl;
            errors++;
        }
    }

    if (errors == 0)
        std::cout << "  [" << mode_name << "] PASSED!" << std::endl;
    else
        std::cout << "  [" << mode_name << "] FAILED with " << errors << " errors" << std::endl;
    return errors;
}

int main()
{
    // ... (Main logic 大致不變，只需確保呼叫新的 verify function) ...
    hls::stream<axis_word_8> input_stream("input_stream");
    hls::stream<axis_word_32> output_stream("output_stream");

    static ap_uint<8> test_input[MAX_INPUT_BYTES];
    static float golden_I[OUTPUT_SYMBOLS];
    static float golden_Q[OUTPUT_SYMBOLS];
    int total_errors = 0;

    // QPSK Test
    generate_test_data(test_input, golden_I, golden_Q, QPSK_INPUT_BYTES, true);
    for (int i = 0; i < QPSK_INPUT_BYTES; i++)
    {
        axis_word_8 w;
        w.data = test_input[i];
        w.keep = 1;
        w.strb = 1;
        w.last = (i == QPSK_INPUT_BYTES - 1);
        input_stream.write(w);
    }
    symbol_mapper_hls(input_stream, output_stream);
    total_errors += verify_output_32bit(output_stream, golden_I, golden_Q, "QPSK");

    // 8PSK Test
    generate_test_data(test_input, golden_I, golden_Q, PSK8_INPUT_BYTES, false);
    for (int i = 0; i < PSK8_INPUT_BYTES; i++)
    {
        axis_word_8 w;
        w.data = test_input[i];
        w.keep = 1;
        w.strb = 1;
        w.last = (i == PSK8_INPUT_BYTES - 1);
        input_stream.write(w);
    }
    symbol_mapper_hls(input_stream, output_stream);
    total_errors += verify_output_32bit(output_stream, golden_I, golden_Q, "8PSK");

    if (total_errors == 0)
        std::cout << "ALL TESTS PASSED!" << std::endl;
    else
        std::cout << "TOTAL ERRORS: " << total_errors << std::endl;

    return (total_errors == 0) ? 0 : 1;
}