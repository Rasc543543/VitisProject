#ifndef RS_ENCODER_TX_H
#define RS_ENCODER_TX_H

#include <stdint.h>
#include "ap_int.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"

// 定義 AXI Stream 介面型別 (8-bit width)
// 改名為 axis_word，但底層仍然是 8-bit 的資料寬度
typedef ap_axiu<8, 0, 0, 0> axis_word;

void rs_encoder_tx(
    hls::stream<axis_word> &input_stream,
    hls::stream<axis_word> &output_stream);

#endif