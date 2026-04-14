#ifndef ENERGY_DESCRAMBLE_HLS_H
#define ENERGY_DESCRAMBLE_HLS_H

#include "ap_int.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"

// 必須與 Scrambler 使用相同的 PRBS 表大小
#define MAX_PRBS_SIZE 400

//=============================================================================
// Type definitions
//=============================================================================
#define DATA_WIDTH 8
typedef ap_axiu<DATA_WIDTH, 0, 0, 0> axis_word;

//=============================================================================
// Top function
//=============================================================================
void energy_descramble(
    hls::stream<axis_word> &input_stream,
    hls::stream<axis_word> &output_stream);

#endif