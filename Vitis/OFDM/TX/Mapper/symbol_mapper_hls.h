// symbol_mapper_hls.h
// [Updated for FP16 Output - 32-bit Packed]
#ifndef SYMBOL_MAPPER_HLS_H
#define SYMBOL_MAPPER_HLS_H

#include <ap_int.h>
#include <ap_fixed.h>
#include <ap_axi_sdata.h>
#include <hls_stream.h>
#include "hls_half.h" // <--- 引入半精度浮點數庫

// ============================================
// AXI-Stream 介面類型
// ============================================
typedef ap_axiu<8, 0, 0, 0> axis_word_8;   // 8-bit Input
typedef ap_axiu<32, 0, 0, 0> axis_word_32; // 32-bit Output: Packed I/Q (FP16)

// ============================================
// [修改] 資料型別定義 -> 改用 half
// ============================================
typedef half symbol_t;

// ============================================
// 32-bit 輸出格式定義 (FP16)
// ============================================
// Bit [31:16] = Q 分量 (FP16 Raw Bits)
// Bit [15:0]  = I 分量 (FP16 Raw Bits)
// ============================================

#define OUTPUT_SYMBOLS 1632
#define QPSK_INPUT_BYTES (OUTPUT_SYMBOLS * 2)
#define PSK8_INPUT_BYTES (OUTPUT_SYMBOLS * 3)
#define MAX_INPUT_BYTES PSK8_INPUT_BYTES

void symbol_mapper_hls(
    hls::stream<axis_word_8> &input_stream,
    hls::stream<axis_word_32> &output_stream);

#endif // SYMBOL_MAPPER_HLS_H