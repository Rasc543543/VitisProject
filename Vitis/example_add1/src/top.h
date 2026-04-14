#pragma once
#include <ap_int.h>

#define DATA_WIDTH 16
#define N_SAMPLES  16

typedef ap_int<DATA_WIDTH> data_t;

void add1(data_t in[N_SAMPLES], data_t out[N_SAMPLES]);
