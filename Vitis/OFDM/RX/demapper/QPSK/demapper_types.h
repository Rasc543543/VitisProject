#ifndef DEMAPPER_TYPES_H
#define DEMAPPER_TYPES_H

#include <ap_int.h>
#include <ap_axi_sdata.h>
#include <hls_stream.h>
#include <hls_math.h>
#include "hls_half.h"

#define FRAME_SYMBOLS 1632

// [修正] 統一 CSI 天花板設定為 10.0
const float MAX_CSI_FACTOR = 128.0f;

typedef ap_axiu<16, 0, 0, 0> axis_half_in;
typedef ap_axiu<32, 0, 0, 0> axis_word_32;
typedef ap_axiu<8, 0, 0, 0> axis_soft_out;

static inline half bits_to_half(ap_uint<16> bits)
{
#pragma HLS INLINE
    unsigned short u = (unsigned short)bits;
    return *(half *)&u;
}

/**
 * 量化函式：包含 0.01 偏移量以補償截斷誤差
 */
static inline ap_uint<8> quantize_llr(float val)
{
#pragma HLS INLINE
    // 加入 0.01 偏移量
    float rounded_val = (val >= 0) ? (val + 0.01f) : (val - 0.01f);

    if (rounded_val > 7.0f)
        rounded_val = 7.0f;
    if (rounded_val < -7.0f)
        rounded_val = -7.0f;

    return (ap_uint<8>)(ap_int<8>)(ap_int<4>)rounded_val;
}

static inline void write_output(hls::stream<axis_soft_out> &stream, ap_uint<8> val, bool last)
{
#pragma HLS INLINE
    axis_soft_out pkt;
    pkt.data = val;
    pkt.keep = 1;
    pkt.strb = 1;
    pkt.last = last ? 1 : 0;
    stream.write(pkt);
}

#endif