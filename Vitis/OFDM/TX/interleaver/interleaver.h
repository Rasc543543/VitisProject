#ifndef INTERLEAVER_H
#define INTERLEAVER_H
//=============================================================================
// 引入HLS相關頭文件
#include <hls_stream.h>
#include <ap_int.h>
#include <ap_axi_sdata.h>
// 使用標準類型
typedef ap_axiu<8, 0, 0, 0> axis_word;

//=============================================================================

void tx_interleaver
(
    hls::stream<axis_word>& input_stream,
    hls::stream<axis_word>& output_stream
);

//=============================================================================

#endif

