#include "ref_rx_defs.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdio>
#include <stdint.h>
#include <cstdlib>

void ref_rx_packed(hls::stream<axis_t> &input_stream, hls::stream<axis_t> &output_stream);

// Set by ref_rx_packed (via extern) to report best_cfo back to TB
int g_debug_best_cfo = -1;

// Box-Muller Gaussian noise (sigma = std dev per I or Q component)
static float gaussian_noise(float sigma) {
    float u1 = ((float)rand() + 1.0f) / ((float)RAND_MAX + 1.0f);
    float u2 = (float)rand() / (float)RAND_MAX;
    return sigma * sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
}

// 8PSK unit-circle constellation (k=0..7, 45deg steps)
static const float PSK8_I[8] = {  1.0f,  0.707f,  0.0f, -0.707f, -1.0f, -0.707f,  0.0f,  0.707f };
static const float PSK8_Q[8] = {  0.0f,  0.707f,  1.0f,  0.707f,  0.0f, -0.707f, -1.0f, -0.707f };

// modulation: 0=QPSK, 1=8PSK
// snr_db >= 90 means no noise
void run_snr_test(int cfo_offset, float phase_deg, float snr_db, int modulation)
{
    hls::stream<axis_t> in_stream("in_stream");
    hls::stream<axis_t> out_stream("out_stream");

    float ch_gain   = 0.5f;
    float phase_rad = phase_deg * (float)M_PI / 180.0f;
    float ch_r      = cosf(phase_rad) * ch_gain;
    float ch_i      = sinf(phase_rad) * ch_gain;

    // SNR = signal_power / (2*sigma^2), signal_power = ch_gain^2 = 0.25
    float sig_power   = ch_gain * ch_gain;
    float noise_sigma = (snr_db < 90.0f)
                        ? sqrtf(sig_power / (2.0f * powf(10.0f, snr_db / 10.0f)))
                        : 0.0f;

    // Build signal in float domain first, then add noise
    float frame_i[FFT_LENGTH] = {};
    float frame_q[FFT_LENGTH] = {};

    std::vector<float> gold_i(DATA_TONES), gold_q(DATA_TONES);

    // Phase 1: generate signal + channel + CFO shift
    int d_idx = 0, p_idx = 0;
    for (int i = 0; i < FFT_LENGTH; i++) {
        float ti = 0.0f, tq = 0.0f;
        int active_idx = i - LEFT_GUARD;
        if (i >= LEFT_GUARD && i < FFT_LENGTH - RIGHT_GUARD) {
            if (active_idx % 7 == 0) {
                ti = 1.0f / (float)PILOT_ROM_INV[p_idx++];
            } else {
                if (modulation == 0) {
                    gold_i[d_idx] = (d_idx % 2 == 0) ? 0.707f : -0.707f;
                    gold_q[d_idx] = 0.707f;
                } else {
                    gold_i[d_idx] = PSK8_I[d_idx % 8];
                    gold_q[d_idx] = PSK8_Q[d_idx % 8];
                }
                ti = gold_i[d_idx];
                tq = gold_q[d_idx];
                d_idx++;
            }
        }
        int s_idx = i + cfo_offset;
        if (s_idx >= 0 && s_idx < FFT_LENGTH) {
            frame_i[s_idx] = ti * ch_r - tq * ch_i;
            frame_q[s_idx] = ti * ch_i + tq * ch_r;
        }
    }

    // Phase 2: add AWGN to all bins, pack into AXI-Stream
    for (int i = 0; i < FFT_LENGTH; i++) {
        float fi = frame_i[i] + (noise_sigma > 0.0f ? gaussian_noise(noise_sigma) : 0.0f);
        float fq = frame_q[i] + (noise_sigma > 0.0f ? gaussian_noise(noise_sigma) : 0.0f);
        int16_t hw_i = (int16_t)(fi * 2048.0f);
        int16_t hw_q = (int16_t)(fq * 2048.0f);
        axis_t pkt;
        pkt.data = pack_int16(hw_i, hw_q);
        pkt.keep = 0xF;
        pkt.strb = 0xF;
        pkt.last = (i == FFT_LENGTH - 1) ? 1 : 0;
        in_stream.write(pkt);
    }

    // Phase 3: run DUT (sets g_debug_best_cfo internally)
    ref_rx_packed(in_stream, out_stream);

    // Phase 4: measure reconstruction error
    float total_error = 0.0f, max_error = 0.0f;
    for (int i = 0; i < DATA_TONES; i++) {
        axis_t out = out_stream.read();
        float ri = (float)u16_to_half(out.data(15, 0));
        float rq = (float)u16_to_half(out.data(31, 16));
        float err = sqrtf(powf(ri - gold_i[i], 2.0f) + powf(rq - gold_q[i], 2.0f));
        total_error += err;
        if (err > max_error) max_error = err;
    }

    printf("  SNR=%5.1fdB | best_cfo=%3d | AvgErr=%.6f | MaxErr=%.6f\n",
           snr_db, g_debug_best_cfo, total_error / DATA_TONES, max_error);
}

int main()
{
    srand(42);

    // Frame 1: unlocked → full scan
    printf("=== Frame 1 (Unlocked, QPSK, no noise) ===\n");
    run_snr_test(70, 45.0f, 99.0f, 0);

    // Frame 2: locked → ±5 scan
    printf("=== Frame 2 (Locked, QPSK, no noise) ===\n");
    run_snr_test(70, 45.0f, 99.0f, 0);

    printf("\n>>> C-SIM DONE!\n");
    return 0;
}
