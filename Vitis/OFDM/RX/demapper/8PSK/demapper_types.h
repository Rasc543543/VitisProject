#ifndef DEMAPPER_TYPES_H
#define DEMAPPER_TYPES_H

#include <ap_int.h>
#include <ap_axi_sdata.h>
#include <hls_stream.h>
#include <hls_math.h>
#include "hls_half.h"

#define FRAME_SYMBOLS 1632

// Packed Input: [31:16]=Q, [15:0]=I
typedef ap_axiu<32, 0, 0, 0> axis_word_32;
typedef ap_axiu<8, 0, 0, 0> axis_soft_out;

static inline half bits_to_half(ap_uint<16> bits)
{
#pragma HLS INLINE
    unsigned short u = (unsigned short)bits;
    return *(half *)&u;
}

static inline ap_uint<8> quantize_llr(float val)
{
#pragma HLS INLINE
    if (val > 7.0f)
        val = 7.0f;
    if (val < -7.0f)
        val = -7.0f;
    return (ap_uint<8>)((ap_int<8>)((ap_int<4>)val));
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