#include "rs_decoder_rx.h"
#include <stdio.h>
const int T = 8;
const int TWO_T = 16;
const int MAX_BUF_SIZE = 512;

// ============================================================================
// GF(2^8) Arithmetic Helpers
// ============================================================================
inline uint8_t gf_add(uint8_t a, uint8_t b)
{
#pragma HLS INLINE
    return a ^ b;
}

inline uint8_t gf_mul_local(uint8_t a, uint8_t b,
                            const uint8_t GF_EXP[256],
                            const uint8_t GF_LOG[256])
{
#pragma HLS INLINE
    if (a == 0 || b == 0)
        return 0;
    int sum = (int)GF_LOG[a] + (int)GF_LOG[b];
    if (sum >= 255)
        sum -= 255;
    return GF_EXP[sum];
}

inline uint8_t gf_div_local(uint8_t a, uint8_t b,
                            const uint8_t GF_EXP[256],
                            const uint8_t GF_LOG[256])
{
#pragma HLS INLINE
    if (a == 0)
        return 0;
    if (b == 0)
        return 0;
    int idx = (int)GF_LOG[a] - (int)GF_LOG[b];
    if (idx < 0)
        idx += 255;
    return GF_EXP[idx];
}

inline uint8_t gf_pow(int power, const uint8_t GF_EXP[256])
{
#pragma HLS INLINE
    int idx = power % 255;
    if (idx < 0)
        idx += 255;
    return GF_EXP[idx];
}

// ============================================================================
// Main Decoder with Debug Output
// ============================================================================
void rs_decoder_rx(
    hls::stream<axis_word> &input_stream,
    hls::stream<axis_word> &output_stream)
{
#pragma HLS INTERFACE ap_ctrl_none port = return
#pragma HLS INTERFACE axis port = input_stream
#pragma HLS INTERFACE axis port = output_stream

#ifndef __SYNTHESIS__
    if (input_stream.empty())
        return;
#endif

    // GF Tables
    const uint8_t GF_EXP[256] = {
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1D, 0x3A, 0x74, 0xE8, 0xCD, 0x87, 0x13, 0x26,
        0x4C, 0x98, 0x2D, 0x5A, 0xB4, 0x75, 0xEA, 0xC9, 0x8F, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0,
        0x9D, 0x27, 0x4E, 0x9C, 0x25, 0x4A, 0x94, 0x35, 0x6A, 0xD4, 0xB5, 0x77, 0xEE, 0xC1, 0x9F, 0x23,
        0x46, 0x8C, 0x05, 0x0A, 0x14, 0x28, 0x50, 0xA0, 0x5D, 0xBA, 0x69, 0xD2, 0xB9, 0x6F, 0xDE, 0xA1,
        0x5F, 0xBE, 0x61, 0xC2, 0x99, 0x2F, 0x5E, 0xBC, 0x65, 0xCA, 0x89, 0x0F, 0x1E, 0x3C, 0x78, 0xF0,
        0xFD, 0xE7, 0xD3, 0xBB, 0x6B, 0xD6, 0xB1, 0x7F, 0xFE, 0xE1, 0xDF, 0xA3, 0x5B, 0xB6, 0x71, 0xE2,
        0xD9, 0xAF, 0x43, 0x86, 0x11, 0x22, 0x44, 0x88, 0x0D, 0x1A, 0x34, 0x68, 0xD0, 0xBD, 0x67, 0xCE,
        0x81, 0x1F, 0x3E, 0x7C, 0xF8, 0xED, 0xC7, 0x93, 0x3B, 0x76, 0xEC, 0xC5, 0x97, 0x33, 0x66, 0xCC,
        0x85, 0x17, 0x2E, 0x5C, 0xB8, 0x6D, 0xDA, 0xA9, 0x4F, 0x9E, 0x21, 0x42, 0x84, 0x15, 0x2A, 0x54,
        0xA8, 0x4D, 0x9A, 0x29, 0x52, 0xA4, 0x55, 0xAA, 0x49, 0x92, 0x39, 0x72, 0xE4, 0xD5, 0xB7, 0x73,
        0xE6, 0xD1, 0xBF, 0x63, 0xC6, 0x91, 0x3F, 0x7E, 0xFC, 0xE5, 0xD7, 0xB3, 0x7B, 0xF6, 0xF1, 0xFF,
        0xE3, 0xDB, 0xAB, 0x4B, 0x96, 0x31, 0x62, 0xC4, 0x95, 0x37, 0x6E, 0xDC, 0xA5, 0x57, 0xAE, 0x41,
        0x82, 0x19, 0x32, 0x64, 0xC8, 0x8D, 0x07, 0x0E, 0x1C, 0x38, 0x70, 0xE0, 0xDD, 0xA7, 0x53, 0xA6,
        0x51, 0xA2, 0x59, 0xB2, 0x79, 0xF2, 0xF9, 0xEF, 0xC3, 0x9B, 0x2B, 0x56, 0xAC, 0x45, 0x8A, 0x09,
        0x12, 0x24, 0x48, 0x90, 0x3D, 0x7A, 0xF4, 0xF5, 0xF7, 0xF3, 0xFB, 0xEB, 0xCB, 0x8B, 0x0B, 0x16,
        0x2C, 0x58, 0xB0, 0x7D, 0xFA, 0xE9, 0xCF, 0x83, 0x1B, 0x36, 0x6C, 0xD8, 0xAD, 0x47, 0x8E, 0x00};
#pragma HLS BIND_STORAGE variable = GF_EXP type = rom_1p impl = lutram

    const uint8_t GF_LOG[256] = {
        0xFF, 0x00, 0x01, 0x19, 0x02, 0x32, 0x1A, 0xC6, 0x03, 0xDF, 0x33, 0xEE, 0x1B, 0x68, 0xC7, 0x4B,
        0x04, 0x64, 0xE0, 0x0E, 0x34, 0x8D, 0xEF, 0x81, 0x1C, 0xC1, 0x69, 0xF8, 0xC8, 0x08, 0x4C, 0x71,
        0x05, 0x8A, 0x65, 0x2F, 0xE1, 0x24, 0x0F, 0x21, 0x35, 0x93, 0x8E, 0xDA, 0xF0, 0x12, 0x82, 0x45,
        0x1D, 0xB5, 0xC2, 0x7D, 0x6A, 0x27, 0xF9, 0xB9, 0xC9, 0x9A, 0x09, 0x78, 0x4D, 0xE4, 0x72, 0xA6,
        0x06, 0xBF, 0x8B, 0x62, 0x66, 0xDD, 0x30, 0xFD, 0xE2, 0x98, 0x25, 0xB3, 0x10, 0x91, 0x22, 0x88,
        0x36, 0xD0, 0x94, 0xCE, 0x8F, 0x96, 0xDB, 0xBD, 0xF1, 0xD2, 0x13, 0x5C, 0x83, 0x38, 0x46, 0x40,
        0x1E, 0x42, 0xB6, 0xA3, 0xC3, 0x48, 0x7E, 0x6E, 0x6B, 0x3A, 0x28, 0x54, 0xFA, 0x85, 0xBA, 0x3D,
        0xCA, 0x5E, 0x9B, 0x9F, 0x0A, 0x15, 0x79, 0x2B, 0x4E, 0xD4, 0xE5, 0xAC, 0x73, 0xF3, 0xA7, 0x57,
        0x07, 0x70, 0xC0, 0xF7, 0x8C, 0x80, 0x63, 0x0D, 0x67, 0x4A, 0xDE, 0xED, 0x31, 0xC5, 0xFE, 0x18,
        0xE3, 0xA5, 0x99, 0x77, 0x26, 0xB8, 0xB4, 0x7C, 0x11, 0x44, 0x92, 0xD9, 0x23, 0x20, 0x89, 0x2E,
        0x37, 0x3F, 0xD1, 0x5B, 0x95, 0xBC, 0xCF, 0xCD, 0x90, 0x87, 0x97, 0xB2, 0xDC, 0xFC, 0xBE, 0x61,
        0xF2, 0x56, 0xD3, 0xAB, 0x14, 0x2A, 0x5D, 0x9E, 0x84, 0x3C, 0x39, 0x53, 0x47, 0x6D, 0x41, 0xA2,
        0x1F, 0x2D, 0x43, 0xD8, 0xB7, 0x7B, 0xA4, 0x76, 0xC4, 0x17, 0x49, 0xEC, 0x7F, 0x0C, 0x6F, 0xF6,
        0x6C, 0xA1, 0x3B, 0x52, 0x29, 0x9D, 0x55, 0xAA, 0xFB, 0x60, 0x86, 0xB1, 0xBB, 0xCC, 0x3E, 0x5A,
        0xCB, 0x59, 0x5F, 0xB0, 0x9C, 0xA9, 0xA0, 0x51, 0x0B, 0xF5, 0x16, 0xEB, 0x7A, 0x75, 0x2C, 0xD7,
        0x4F, 0xAE, 0xD5, 0xE9, 0xE6, 0xE7, 0xAD, 0xE8, 0x74, 0xD6, 0xF4, 0xEA, 0xA8, 0x50, 0x58, 0xAF};
#pragma HLS BIND_STORAGE variable = GF_LOG type = rom_1p impl = lutram

    static uint8_t data_buf[MAX_BUF_SIZE];
#pragma HLS ARRAY_PARTITION variable = data_buf cyclic factor = 4 dim = 1

    // ====================================================================
    // Stage 1: COLLECT
    // ====================================================================
    int input_len = 0;
    bool tlast_seen = false;

COLLECT_LOOP:
    do
    {
#pragma HLS PIPELINE II = 1
        axis_word in_val = input_stream.read();

        if (input_len < MAX_BUF_SIZE)
        {
            data_buf[input_len] = in_val.data;
            input_len++;
        }

        if (in_val.last == 1)
        {
            tlast_seen = true;
        }
    } while (!tlast_seen && input_len < MAX_BUF_SIZE);

    // ====================================================================
    // Stage 2: Mode Detection (嚴格檢查)
    // ====================================================================
    int process_chunks = 0;
    int current_N = 0;
    int current_K = 0;

    // ✅ 回歸完美寫死的 204 與 306！
    if (input_len == 204)
    {
        process_chunks = 1;
        current_N = 204;
        current_K = 188;
    }
    else if (input_len == 306)
    {
        process_chunks = 2;
        current_N = 153;
        current_K = 137;
    }
    else
    {
        // 若不是精準的 204 或 306，直接放棄這個爛封包，等待下一包！
        return;
    }

    // ====================================================================
    // Stage 3: DECODE
    // ====================================================================
    bool uncorrectable = false;

    for (int chunk = 0; chunk < process_chunks; chunk++)
    {
        if (uncorrectable)
            break;

        int offset = chunk * current_N;

        // --- A. Syndrome Calculation ---
        uint8_t syndrome[TWO_T];
#pragma HLS ARRAY_PARTITION variable = syndrome complete

        for (int i = 0; i < TWO_T; i++)
        {
#pragma HLS UNROLL
            syndrome[i] = 0;
        }

    SYNDROME_LOOP:
        for (int i = 0; i < current_N; i++)
        {
#pragma HLS PIPELINE II = 1
            uint8_t d = data_buf[offset + i];
            for (int j = 0; j < TWO_T; j++)
            {
#pragma HLS UNROLL
                if (syndrome[j] == 0)
                {
                    syndrome[j] = d;
                }
                else
                {
                    int idx = (GF_LOG[syndrome[j]] + j) % 255;
                    syndrome[j] = GF_EXP[idx] ^ d;
                }
            }
        }

        bool syn_zero = true;
        for (int i = 0; i < TWO_T; i++)
        {
#pragma HLS UNROLL
            if (syndrome[i] != 0)
                syn_zero = false;
        }

        if (syn_zero)
            continue;

        // --- B. Berlekamp-Massey ---
        uint8_t sigma[TWO_T + 1];
        uint8_t b[TWO_T + 1];
        uint8_t T_poly[TWO_T + 1];
#pragma HLS ARRAY_PARTITION variable = sigma complete
#pragma HLS ARRAY_PARTITION variable = b complete
#pragma HLS ARRAY_PARTITION variable = T_poly complete

        for (int i = 0; i <= TWO_T; i++)
        {
#pragma HLS UNROLL
            sigma[i] = (i == 0) ? 1 : 0;
            b[i] = (i == 0) ? 1 : 0;
        }

        int L = 0;

    BM_LOOP:
        for (int r = 1; r <= TWO_T; r++)
        {
#pragma HLS PIPELINE II = 1
            uint8_t discr = 0;
            for (int i = 0; i < r && i <= TWO_T; i++)
            {
#pragma HLS UNROLL
                discr ^= gf_mul_local(sigma[i], syndrome[r - 1 - i], GF_EXP, GF_LOG);
            }

            if (discr == 0)
            {
                for (int i = TWO_T; i > 0; i--)
                {
#pragma HLS UNROLL
                    b[i] = b[i - 1];
                }
                b[0] = 0;
            }
            else
            {
                T_poly[0] = sigma[0];
                for (int i = 0; i < TWO_T; i++)
                {
#pragma HLS UNROLL
                    T_poly[i + 1] = sigma[i + 1] ^ gf_mul_local(discr, b[i], GF_EXP, GF_LOG);
                }

                if (2 * L <= r - 1)
                {
                    L = r - L;
                    for (int i = 0; i <= TWO_T; i++)
                    {
#pragma HLS UNROLL
                        b[i] = gf_div_local(sigma[i], discr, GF_EXP, GF_LOG);
                    }
                }
                else
                {
                    for (int i = TWO_T; i > 0; i--)
                    {
#pragma HLS UNROLL
                        b[i] = b[i - 1];
                    }
                    b[0] = 0;
                }

                for (int i = 0; i <= TWO_T; i++)
                {
#pragma HLS UNROLL
                    sigma[i] = T_poly[i];
                }
            }
        }

        int deg_sigma = 0;
        for (int i = TWO_T; i >= 0; i--)
        {
#pragma HLS UNROLL
            if (sigma[i] != 0 && deg_sigma == 0)
                deg_sigma = i;
        }

        if (deg_sigma > T)
        {
            uncorrectable = true;
            break;
        }

        // --- C. Omega ---
        uint8_t omega[TWO_T + 1];
#pragma HLS ARRAY_PARTITION variable = omega complete

    OMEGA_LOOP:
        for (int i = 0; i < TWO_T; i++)
        {
#pragma HLS PIPELINE II = 1
            uint8_t tmp = 0;
            int jmax = (deg_sigma < i) ? deg_sigma : i;
            for (int j = 0; j <= jmax; j++)
            {
#pragma HLS UNROLL
                tmp ^= gf_mul_local(syndrome[i - j], sigma[j], GF_EXP, GF_LOG);
            }
            omega[i] = tmp;
        }
        omega[TWO_T] = 0;

        // --- D. Chien Search ---
        uint8_t reg[TWO_T + 1];
#pragma HLS ARRAY_PARTITION variable = reg complete

        for (int i = 0; i <= TWO_T; i++)
        {
#pragma HLS UNROLL
            reg[i] = sigma[i];
        }

        int num_errors = 0;

    CHIEN_LOOP:
        for (int i = 1; i <= 255; i++)
        {
#pragma HLS PIPELINE II = 1
            uint8_t sum = 1;
            for (int j = 1; j <= deg_sigma; j++)
            {
#pragma HLS UNROLL
                if (reg[j] != 0)
                {
                    int idx = (GF_LOG[reg[j]] + 255 - j) % 255;
                    reg[j] = GF_EXP[idx];
                }
                sum ^= reg[j];
            }

            if (sum == 0)
            {
                int err_pos = i;
                if (err_pos < current_N)
                {
                    int buf_idx = (current_N - 1) - err_pos;

                    // Forney
                    uint8_t X_inv = gf_pow(255 - i, GF_EXP);
                    uint8_t omega_val = omega[0];
                    uint8_t x_pow = X_inv;
                    for (int k = 1; k < TWO_T; k++)
                    {
#pragma HLS UNROLL
                        omega_val ^= gf_mul_local(omega[k], x_pow, GF_EXP, GF_LOG);
                        x_pow = gf_mul_local(x_pow, X_inv, GF_EXP, GF_LOG);
                    }

                    uint8_t sigma_deriv = sigma[1];
                    uint8_t x_sq = gf_mul_local(X_inv, X_inv, GF_EXP, GF_LOG);
                    uint8_t x_pow2 = x_sq;
                    for (int k = 3; k <= deg_sigma; k += 2)
                    {
#pragma HLS UNROLL
                        sigma_deriv ^= gf_mul_local(sigma[k], x_pow2, GF_EXP, GF_LOG);
                        x_pow2 = gf_mul_local(x_pow2, x_sq, GF_EXP, GF_LOG);
                    }

                    if (sigma_deriv != 0)
                    {
                        uint8_t X = gf_pow(i, GF_EXP);
                        uint8_t err_val = gf_div_local(
                            gf_mul_local(X, omega_val, GF_EXP, GF_LOG),
                            sigma_deriv, GF_EXP, GF_LOG);

                        data_buf[offset + buf_idx] ^= err_val;
                    }
                    num_errors++;
                }
            }
        }

        if (num_errors != deg_sigma)
        {
            uncorrectable = true;
        }
    } // End chunk loop

    // ====================================================================
    // Stage 4: OUTPUT
    // ====================================================================

OUTPUT_LOOP:
    for (int chunk = 0; chunk < process_chunks; chunk++)
    {
        int offset = chunk * current_N;

        for (int i = 0; i < current_K; i++)
        {
#pragma HLS PIPELINE II = 1
            axis_word out_val;
            out_val.data = data_buf[offset + i];
            out_val.keep = 1;
            out_val.strb = 1;

            bool is_last_byte = (chunk == process_chunks - 1) && (i == current_K - 1);
            out_val.last = is_last_byte ? 1 : 0;

            output_stream.write(out_val);
        }
    }
}