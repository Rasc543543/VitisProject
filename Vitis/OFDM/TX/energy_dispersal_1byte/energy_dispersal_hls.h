#ifndef ENERGY_DISPERSAL_HLS_H
#define ENERGY_DISPERSAL_HLS_H

#include "ap_int.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"

//=============================================================================
// Configuration
//=============================================================================
#define DATA_WIDTH 8      // Byte-based processing
#define MAX_PRBS_SIZE 400 // ← 改这里：扩展 PRBS 表

//=============================================================================
// Type definitions
//=============================================================================
typedef ap_uint<DATA_WIDTH> data_word;
typedef ap_axiu<DATA_WIDTH, 0, 0, 0> axis_word;

//=============================================================================
// Top function
//=============================================================================
void energy_dispersal(
    hls::stream<axis_word> &input_stream,
    hls::stream<axis_word> &output_stream);

#endif