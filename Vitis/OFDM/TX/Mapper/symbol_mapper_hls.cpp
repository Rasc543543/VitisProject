// symbol_mapper_hls.cpp
// [Updated for FP16 Output - 32-bit Packed]
#include "symbol_mapper_hls.h"

#ifndef __SYNTHESIS__
#include <iostream>
#endif

// ============================================
// Helper Function: 將 half 轉為 raw bits (uint16)
// 使用指標轉型避免 union 建構子錯誤
// ============================================
ap_uint<16> half_to_u16(half val)
{
#pragma HLS INLINE
    unsigned short us = *(unsigned short *)&val;
    return (ap_uint<16>)us;
}

// ============================================
// QPSK 星座圖 (Gray coded) - 改為 half
// ============================================
static const symbol_t QPSK_I[4] = {
    (half)0.7071067811865475,  // 00
    (half)-0.7071067811865475, // 01
    (half)0.7071067811865475,  // 10
    (half)-0.7071067811865475  // 11
};

static const symbol_t QPSK_Q[4] = {
    (half)0.7071067811865475,  // 00
    (half)0.7071067811865475,  // 01
    (half)-0.7071067811865475, // 10
    (half)-0.7071067811865475  // 11
};

// ============================================
// 8PSK 星座圖 (Gray coded) - 改為 half
// ============================================
static const symbol_t PSK8_I[8] = {
    (half)0.9238795325112867,  // 000
    (half)0.3826834323650898,  // 001
    (half)-0.9238795325112867, // 010
    (half)-0.3826834323650898, // 011
    (half)0.9238795325112867,  // 100
    (half)0.3826834323650898,  // 101
    (half)-0.9238795325112867, // 110
    (half)-0.3826834323650898  // 111
};

static const symbol_t PSK8_Q[8] = {
    (half)0.3826834323650898,  // 000
    (half)0.9238795325112867,  // 001
    (half)0.3826834323650898,  // 010
    (half)0.9238795325112867,  // 011
    (half)-0.3826834323650898, // 100
    (half)-0.9238795325112867, // 101
    (half)-0.3826834323650898, // 110
    (half)-0.9238795325112867  // 111
};

void symbol_mapper_hls(
    hls::stream<axis_word_8> &input_stream,
    hls::stream<axis_word_32> &output_stream)
{
#pragma HLS INTERFACE mode = ap_ctrl_none port = return
#pragma HLS INTERFACE axis port = input_stream
#pragma HLS INTERFACE axis port = output_stream

    static ap_uint<1> buf_in[MAX_INPUT_BYTES];
#pragma HLS BIND_STORAGE variable = buf_in type = ram_1p impl = lutram

#ifdef __SYNTHESIS__
    while (1)
    {
#endif

#ifndef __SYNTHESIS__
        static bool debug_printed = false;
        if (!debug_printed)
        {
            std::cout << "\n[[DEBUG]] HLS Version: 32-bit PARALLEL FP16 OUTPUT\n"
                      << std::endl;
            debug_printed = true;
        }
#endif

        // ===== Phase 1: Collect Input =====
        int byte_cnt = 0;
        bool is_last = false;

    COLLECT:
        while (!is_last)
        {
#pragma HLS PIPELINE II = 1
            axis_word_8 in_word = input_stream.read();
            buf_in[byte_cnt++] = in_word.data[0];
            is_last = (in_word.last == 1);
        }

        bool is_qpsk = (byte_cnt == QPSK_INPUT_BYTES);
        int bit_idx = 0;

        // ===== Phase 2: Map + Parallel Output =====
    PROCESS_OUTPUT:
        for (int sym_idx = 0; sym_idx < OUTPUT_SYMBOLS; sym_idx++)
        {
#pragma HLS PIPELINE II = 1

            symbol_t I_val, Q_val;
            ap_uint<3> symbol_index;

            if (is_qpsk)
            {
                // QPSK
                ap_uint<1> b0 = buf_in[bit_idx++];
                ap_uint<1> b1 = buf_in[bit_idx++];
                symbol_index = (b1, b0);
                I_val = QPSK_I[symbol_index];
                Q_val = QPSK_Q[symbol_index];
            }
            else
            {
                // 8PSK
                ap_uint<1> b0 = buf_in[bit_idx++];
                ap_uint<1> b1 = buf_in[bit_idx++];
                ap_uint<1> b2 = buf_in[bit_idx++];
                symbol_index = (b2, b1, b0);
                I_val = PSK8_I[symbol_index];
                Q_val = PSK8_Q[symbol_index];
            }

            // ============================================
            // 32-bit 打包輸出 (FP16 Pack)
            // [31:16] = Q, [15:0] = I
            // ============================================
            axis_word_32 out_word;

            // 使用 Helper 將 half 轉為 16-bit uint
            ap_uint<16> I_raw = half_to_u16(I_val);
            ap_uint<16> Q_raw = half_to_u16(Q_val);

            out_word.data = (Q_raw, I_raw); // 拼接
            out_word.keep = 0xF;
            out_word.strb = 0xF;
            out_word.last = (sym_idx == OUTPUT_SYMBOLS - 1) ? 1 : 0;

            output_stream.write(out_word);
        }

#ifdef __SYNTHESIS__
    }
#endif
}