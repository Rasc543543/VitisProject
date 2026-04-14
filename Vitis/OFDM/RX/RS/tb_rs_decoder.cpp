#include "rs_decoder_rx.h"
#include "tb_golden_patterns.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// GF Tables (same as encoder)
// ============================================================================
const uint8_t GF_EXP_TB[256] = {
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

const uint8_t GF_LOG_TB[256] = {
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

const uint8_t GEN_POLY_TB[16] = {
    0x3B, 0x24, 0x32, 0x62, 0xE5, 0x29, 0x41, 0xA3,
    0x08, 0x1E, 0xD1, 0x44, 0xBD, 0x68, 0x0D, 0x3B};

// ============================================================================
// GF Multiply (Software)
// ============================================================================
uint8_t gf_mul_sw(uint8_t a, uint8_t b)
{
    if (a == 0 || b == 0)
        return 0;
    int sum = (int)GF_LOG_TB[a] + (int)GF_LOG_TB[b];
    if (sum >= 255)
        sum -= 255;
    return GF_EXP_TB[sum];
}

// ============================================================================
// LFSR Step (Same as encoder)
// ============================================================================
void lfsr_step(uint8_t input, uint8_t parity[16])
{
    uint8_t feedback = input ^ parity[0];
    for (int j = 0; j < 15; j++)
        parity[j] = parity[j + 1] ^ gf_mul_sw(feedback, GEN_POLY_TB[15 - j]);
    parity[15] = gf_mul_sw(feedback, GEN_POLY_TB[0]);
}

// ============================================================================
// RS Encode QPSK (204, 188)
// ============================================================================
void rs_encode_qpsk(const uint8_t data[188], uint8_t codeword[204])
{
    uint8_t parity[16] = {0};

    // 1. Padding: 51 zeros
    for (int i = 0; i < 51; i++)
        lfsr_step(0, parity);

    // 2. Data: 188 bytes
    for (int i = 0; i < 188; i++)
        lfsr_step(data[i], parity);

    // 3. Build codeword
    for (int i = 0; i < 188; i++)
        codeword[i] = data[i];
    for (int i = 0; i < 16; i++)
        codeword[188 + i] = parity[i];
}

// ============================================================================
// RS Encode 8PSK chunk (153, 137)
// ============================================================================
void rs_encode_8psk_chunk(const uint8_t data[137], uint8_t codeword[153])
{
    uint8_t parity[16] = {0};

    // 1. Padding: 102 zeros
    for (int i = 0; i < 102; i++)
        lfsr_step(0, parity);

    // 2. Data: 137 bytes
    for (int i = 0; i < 137; i++)
        lfsr_step(data[i], parity);

    // 3. Build codeword
    for (int i = 0; i < 137; i++)
        codeword[i] = data[i];
    for (int i = 0; i < 16; i++)
        codeword[137 + i] = parity[i];
}

// ============================================================================
// Main Testbench
// ============================================================================
int main()
{
    hls::stream<axis_word> input_stream, output_stream;

    printf("=== RS Decoder C-Simulation Testbench ===\n\n");

    int test_pass = 0;
    int test_fail = 0;

    // ========================================================================
    // Test 1: QPSK No Error (golden pattern from TX encoder)
    // ========================================================================
    printf("Test 1: QPSK No Error - golden pattern (204 -> 188)\n");
    {
        printf("  Codeword[0..4] = %02X %02X %02X %02X %02X\n",
               golden_qpsk_codeword[0], golden_qpsk_codeword[1],
               golden_qpsk_codeword[2], golden_qpsk_codeword[3], golden_qpsk_codeword[4]);
        printf("  Parity[0..4]   = %02X %02X %02X %02X %02X\n",
               golden_qpsk_codeword[188], golden_qpsk_codeword[189],
               golden_qpsk_codeword[190], golden_qpsk_codeword[191], golden_qpsk_codeword[192]);

        for (int i = 0; i < 204; i++)
        {
            axis_word in_word;
            in_word.data = golden_qpsk_codeword[i];
            in_word.keep = 1;
            in_word.strb = 1;
            in_word.last = (i == 203) ? 1 : 0;
            input_stream.write(in_word);
        }

        rs_decoder_rx(input_stream, output_stream);

        int out_count = 0;
        int errors = 0;

        while (!output_stream.empty())
        {
            axis_word out_word = output_stream.read();
            if (out_count < 188 && out_word.data != golden_qpsk_input[out_count])
            {
                if (errors < 5)
                    printf("  ERROR [%d]: got 0x%02X, expected 0x%02X\n",
                           out_count, (uint8_t)out_word.data, golden_qpsk_input[out_count]);
                errors++;
            }
            out_count++;
        }

        printf("  Received %d bytes (expected 188)\n", out_count);

        if (out_count == 188 && errors == 0)
        {
            printf("  *** PASS ***\n\n");
            test_pass++;
        }
        else
        {
            printf("  *** FAIL *** (errors=%d)\n\n", errors);
            test_fail++;
        }
    }

    // ========================================================================
    // Test 2: QPSK 1 Error
    // ========================================================================
    printf("Test 2: QPSK 1 Error at position 50\n");
    {
        uint8_t data[188];
        data[0] = 0x47;
        for (int i = 1; i < 188; i++)
            data[i] = i & 0xFF;

        uint8_t codeword[204];
        rs_encode_qpsk(data, codeword);

        // Inject error
        codeword[50] ^= 0xAB;
        printf("  Injected error at position 50: XOR 0xAB\n");

        for (int i = 0; i < 204; i++)
        {
            axis_word in_word;
            in_word.data = codeword[i];
            in_word.keep = 1;
            in_word.strb = 1;
            in_word.last = (i == 203) ? 1 : 0;
            input_stream.write(in_word);
        }

        rs_decoder_rx(input_stream, output_stream);

        int out_count = 0;
        int errors = 0;

        while (!output_stream.empty())
        {
            axis_word out_word = output_stream.read();
            if (out_count < 188 && out_word.data != data[out_count])
            {
                if (errors < 5)
                    printf("  ERROR [%d]: got 0x%02X, expected 0x%02X\n",
                           out_count, (uint8_t)out_word.data, data[out_count]);
                errors++;
            }
            out_count++;
        }

        printf("  Received %d bytes\n", out_count);

        if (out_count == 188 && errors == 0)
        {
            printf("  *** PASS *** (error corrected)\n\n");
            test_pass++;
        }
        else
        {
            printf("  *** FAIL *** (errors=%d)\n\n", errors);
            test_fail++;
        }
    }

    // ========================================================================
    // Test 3: QPSK 8 Errors (Max)
    // ========================================================================
    printf("Test 3: QPSK 8 Errors (Maximum)\n");
    {
        uint8_t data[188];
        data[0] = 0x47;
        for (int i = 1; i < 188; i++)
            data[i] = i & 0xFF;

        uint8_t codeword[204];
        rs_encode_qpsk(data, codeword);

        // Inject 8 errors
        int err_pos[] = {5, 20, 45, 78, 100, 130, 160, 185};
        uint8_t err_val[] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88};
        for (int i = 0; i < 8; i++)
        {
            codeword[err_pos[i]] ^= err_val[i];
            printf("  Injected error at position %d: XOR 0x%02X\n", err_pos[i], err_val[i]);
        }

        for (int i = 0; i < 204; i++)
        {
            axis_word in_word;
            in_word.data = codeword[i];
            in_word.keep = 1;
            in_word.strb = 1;
            in_word.last = (i == 203) ? 1 : 0;
            input_stream.write(in_word);
        }

        rs_decoder_rx(input_stream, output_stream);

        int out_count = 0;
        int errors = 0;

        while (!output_stream.empty())
        {
            axis_word out_word = output_stream.read();
            if (out_count < 188 && out_word.data != data[out_count])
            {
                if (errors < 5)
                    printf("  ERROR [%d]: got 0x%02X, expected 0x%02X\n",
                           out_count, (uint8_t)out_word.data, data[out_count]);
                errors++;
            }
            out_count++;
        }

        printf("  Received %d bytes\n", out_count);

        if (out_count == 188 && errors == 0)
        {
            printf("  *** PASS *** (8 errors corrected)\n\n");
            test_pass++;
        }
        else
        {
            printf("  *** FAIL *** (errors=%d)\n\n", errors);
            test_fail++;
        }
    }

    // ========================================================================
    // Test 4: QPSK 9 Errors (Uncorrectable - must be dropped)
    // ========================================================================
    printf("Test 4: QPSK 9 Errors (Uncorrectable - expect 0 bytes output)\n");
    {
        uint8_t data[188];
        data[0] = 0x47;
        for (int i = 1; i < 188; i++)
            data[i] = i & 0xFF;

        uint8_t codeword[204];
        rs_encode_qpsk(data, codeword);

        // Inject 9 errors
        int err_pos[] = {5, 20, 45, 78, 100, 130, 160, 185, 190};
        for (int i = 0; i < 9; i++)
        {
            codeword[err_pos[i]] ^= 0x55;
            printf("  Injected error at position %d\n", err_pos[i]);
        }

        for (int i = 0; i < 204; i++)
        {
            axis_word in_word;
            in_word.data = codeword[i];
            in_word.keep = 1;
            in_word.strb = 1;
            in_word.last = (i == 203) ? 1 : 0;
            input_stream.write(in_word);
        }

        rs_decoder_rx(input_stream, output_stream);

        int out_count = 0;
        while (!output_stream.empty())
        {
            output_stream.read();
            out_count++;
        }

        printf("  Received %d bytes (expected 188 - raw data passthrough)\n", out_count);

        if (out_count == 188)
        {
            printf("  *** PASS *** (uncorrectable packet passed through raw)\n\n");
            test_pass++;
        }
        else
        {
            printf("  *** FAIL *** (expected 188 bytes, got %d)\n\n", out_count);
            test_fail++;
        }
    }

    // ========================================================================
    // Test 5: 8PSK No Error (golden pattern from TX encoder)
    // ========================================================================
    printf("Test 5: 8PSK No Error - golden pattern (306 -> 274)\n");
    {
        printf("  Chunk A parity[0..3] = %02X %02X %02X %02X\n",
               golden_8psk_codeword[137], golden_8psk_codeword[138],
               golden_8psk_codeword[139], golden_8psk_codeword[140]);
        printf("  Chunk B parity[0..3] = %02X %02X %02X %02X\n",
               golden_8psk_codeword[290], golden_8psk_codeword[291],
               golden_8psk_codeword[292], golden_8psk_codeword[293]);

        for (int i = 0; i < 306; i++)
        {
            axis_word in_word;
            in_word.data = golden_8psk_codeword[i];
            in_word.keep = 1;
            in_word.strb = 1;
            in_word.last = (i == 305) ? 1 : 0;
            input_stream.write(in_word);
        }

        rs_decoder_rx(input_stream, output_stream);

        int out_count = 0;
        int errors = 0;

        while (!output_stream.empty())
        {
            axis_word out_word = output_stream.read();
            if (out_count < 274 && out_word.data != golden_8psk_input[out_count])
            {
                if (errors < 5)
                    printf("  ERROR [%d]: got 0x%02X, expected 0x%02X\n",
                           out_count, (uint8_t)out_word.data, golden_8psk_input[out_count]);
                errors++;
            }
            out_count++;
        }

        printf("  Received %d bytes (expected 274)\n", out_count);

        if (out_count == 274 && errors == 0)
        {
            printf("  *** PASS ***\n\n");
            test_pass++;
        }
        else
        {
            printf("  *** FAIL *** (errors=%d)\n\n", errors);
            test_fail++;
        }
    }

    // ========================================================================
    // Test 6: 8PSK 8 Errors (Max correctable - 4 errors per chunk)
    // ========================================================================
    printf("Test 6: 8PSK 8 Errors (4 per chunk - max correctable)\n");
    {
        uint8_t data[274];
        data[0] = 0x47;
        for (int i = 1; i < 274; i++)
            data[i] = (uint8_t)(i * 3 + 7);

        uint8_t codeword[306];
        rs_encode_8psk_chunk(&data[0], &codeword[0]);
        rs_encode_8psk_chunk(&data[137], &codeword[153]);

        // 4 errors in chunk A (positions 0..152)
        int err_pos_a[] = {10, 30, 80, 120};
        for (int i = 0; i < 4; i++)
        {
            codeword[err_pos_a[i]] ^= 0xA5;
            printf("  Chunk A error at position %d\n", err_pos_a[i]);
        }
        // 4 errors in chunk B (positions 153..305)
        int err_pos_b[] = {160, 200, 240, 290};
        for (int i = 0; i < 4; i++)
        {
            codeword[err_pos_b[i]] ^= 0x5A;
            printf("  Chunk B error at position %d\n", err_pos_b[i]);
        }

        for (int i = 0; i < 306; i++)
        {
            axis_word in_word;
            in_word.data = codeword[i];
            in_word.keep = 1;
            in_word.strb = 1;
            in_word.last = (i == 305) ? 1 : 0;
            input_stream.write(in_word);
        }

        rs_decoder_rx(input_stream, output_stream);

        int out_count = 0;
        int errors = 0;

        while (!output_stream.empty())
        {
            axis_word out_word = output_stream.read();
            if (out_count < 274 && out_word.data != data[out_count])
            {
                if (errors < 5)
                    printf("  ERROR [%d]: got 0x%02X, expected 0x%02X\n",
                           out_count, (uint8_t)out_word.data, data[out_count]);
                errors++;
            }
            out_count++;
        }

        printf("  Received %d bytes\n", out_count);

        if (out_count == 274 && errors == 0)
        {
            printf("  *** PASS *** (8 errors corrected)\n\n");
            test_pass++;
        }
        else
        {
            printf("  *** FAIL *** (errors=%d)\n\n", errors);
            test_fail++;
        }
    }

    // ========================================================================
    // Test 7: 8PSK 9 Errors in one chunk (Uncorrectable - must be dropped)
    // ========================================================================
    printf("Test 7: 8PSK 9 Errors in chunk A (Uncorrectable - expect 0 bytes output)\n");
    {
        uint8_t data[274];
        data[0] = 0x47;
        for (int i = 1; i < 274; i++)
            data[i] = (uint8_t)(i * 3 + 7);

        uint8_t codeword[306];
        rs_encode_8psk_chunk(&data[0], &codeword[0]);
        rs_encode_8psk_chunk(&data[137], &codeword[153]);

        // 9 errors in chunk A
        int err_pos[] = {0, 15, 30, 50, 70, 90, 110, 130, 148};
        for (int i = 0; i < 9; i++)
        {
            codeword[err_pos[i]] ^= 0x33;
            printf("  Chunk A error at position %d\n", err_pos[i]);
        }

        for (int i = 0; i < 306; i++)
        {
            axis_word in_word;
            in_word.data = codeword[i];
            in_word.keep = 1;
            in_word.strb = 1;
            in_word.last = (i == 305) ? 1 : 0;
            input_stream.write(in_word);
        }

        rs_decoder_rx(input_stream, output_stream);

        int out_count = 0;
        while (!output_stream.empty())
        {
            output_stream.read();
            out_count++;
        }

        printf("  Received %d bytes (expected 274 - raw data passthrough)\n", out_count);

        if (out_count == 274)
        {
            printf("  *** PASS *** (uncorrectable packet passed through raw)\n\n");
            test_pass++;
        }
        else
        {
            printf("  *** FAIL *** (expected 274 bytes, got %d)\n\n", out_count);
            test_fail++;
        }
    }

    // ========================================================================
    // Summary
    // ========================================================================
    printf("============================================================\n");
    printf("  Results: %d PASSED, %d FAILED\n", test_pass, test_fail);
    printf("============================================================\n");

    return (test_fail == 0) ? 0 : 1;
}