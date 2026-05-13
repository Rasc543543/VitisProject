#include <cstdio>
#include <cstdlib>
#include "top.h"

// csim working directory: <proj>/<proj_name>/solution1/csim/build/
// 需要往上 4 層才回到專案根目錄
#define INPUT_FILE  "../../../../tb/data/input.dat"
#define GOLDEN_FILE "../../../../tb/data/golden.dat"

int main() {
    data_t in[N_SAMPLES];
    data_t out[N_SAMPLES];
    data_t golden[N_SAMPLES];

    // --- 讀 input ---
    FILE *fin = fopen(INPUT_FILE, "r");
    if (!fin) { printf("[ERROR] Cannot open %s\n", INPUT_FILE); return 1; }
    for (int i = 0; i < N_SAMPLES; i++) {
        int val; fscanf(fin, "%d", &val); in[i] = (data_t)val;
    }
    fclose(fin);

    // --- 讀 golden ---
    FILE *fgold = fopen(GOLDEN_FILE, "r");
    if (!fgold) { printf("[ERROR] Cannot open %s\n", GOLDEN_FILE); return 1; }
    for (int i = 0; i < N_SAMPLES; i++) {
        int val; fscanf(fgold, "%d", &val); golden[i] = (data_t)val;
    }
    fclose(fgold);

    // --- 跑 DUT ---
    top(in, out);

    // --- 比對輸出 vs golden ---
    int err = 0;
    for (int i = 0; i < N_SAMPLES; i++) {
        if (out[i] != golden[i]) {
            printf("[MISMATCH] idx=%-4d  got=%-8d  expected=%d\n",
                   i, (int)out[i], (int)golden[i]);
            if (++err >= 20) { printf("  ... (too many errors, stopping)\n"); break; }
        }
    }

    if (err == 0) printf("\n[PASS] All %d samples match.\n", N_SAMPLES);
    else          printf("\n[FAIL] %d mismatches found.\n", err);

    return err;
}
