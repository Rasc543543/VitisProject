#include "top.h"

void add1(data_t in[N_SAMPLES], data_t out[N_SAMPLES]) {
#pragma HLS INTERFACE m_axi port=in  depth=N_SAMPLES bundle=gmem0
#pragma HLS INTERFACE m_axi port=out depth=N_SAMPLES bundle=gmem1
#pragma HLS INTERFACE s_axilite port=return

    for (int i = 0; i < N_SAMPLES; i++) {
#pragma HLS PIPELINE II=1
        out[i] = in[i] + 1;
    }
}
