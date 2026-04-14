#ifndef RS_DECODER_RX_H
#define RS_DECODER_RX_H

#include "ap_int.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"
#include <stdint.h>

// [修改 1] AXI-Stream 接口改為 8-bit，與 Encoder 對齊
typedef ap_axiu<8, 0, 0, 0> axis_word;

void rs_decoder_rx(
    hls::stream<axis_word> &input_stream,
    hls::stream<axis_word> &output_stream);

#endif