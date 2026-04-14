#pragma once
#include <ap_int.h>
#include <ap_fixed.h>

// ---- 每個專案修改這裡 ----
#define DATA_WIDTH 16
#define N_SAMPLES  256

typedef ap_int<DATA_WIDTH> data_t;

void top(data_t in[N_SAMPLES], data_t out[N_SAMPLES]);
