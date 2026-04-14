#include "rs_encoder_tx.h"
#include <stdio.h>
#include <vector>
#include <iostream>
#include <iomanip> // For std::hex output

// =========================================================
// 0. 測試參數定義
// =========================================================
const int LEN_QPSK_IN = 188;
const int LEN_8PSK_IN = 274;

// 複製一份係數表到 TB 以供軟體模型使用 (確保與 HW 一致)
const uint8_t GF_EXP_TB[256] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1D, 0x3A, 0x74, 0xE8, 0xCD, 0x87, 0x13, 0x26,
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
const uint8_t GF_LOG_TB[256] = {0xFF, 0x00, 0x01, 0x19, 0x02, 0x32, 0x1A, 0xC6, 0x03, 0xDF, 0x33, 0xEE, 0x1B, 0x68, 0xC7, 0x4B,
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

// 為了讓 TB 能跑，我先提供一個簡單的存取介面 (假設你的 .h 檔沒公開這些表)
// 實務上建議把這些表搬到 .h 檔並加上 static const
// 這裡我們先用簡單的 helper 模擬數學運算
uint8_t gf_mul_sw(uint8_t a, uint8_t b)
{
    if (a == 0 || b == 0)
        return 0;
    // 注意：這裡假設你已經有 GF_LOG_TB 和 GF_EXP_TB
    // 如果沒有，你需要把 rs_encoder_tx.cpp 裡的那兩個大陣列複製到這裡
    int sum = (int)GF_LOG_TB[a] + (int)GF_LOG_TB[b];
    return GF_EXP_TB[sum % 255];
}

// =========================================================
// 1. 軟體黃金模型 (Software Golden Model)
// =========================================================
// 這是用來產生「標準答案」的函式
void run_sw_rs_model(const std::vector<uint8_t> &input_data,
                     std::vector<uint8_t> &expected_output)
{
    // A. 判斷模式
    if (input_data.size() == 188)
    {
        // --- QPSK Mode ---
        uint8_t parity[16] = {0};

        // 1. Padding (51 zeros)
        for (int k = 0; k < 51; k++)
        {
            uint8_t feedback = 0 ^ parity[0];
            for (int j = 0; j < 15; j++)
                parity[j] = parity[j + 1] ^ gf_mul_sw(feedback, GEN_POLY_TB[15 - j]);
            parity[15] = gf_mul_sw(feedback, GEN_POLY_TB[0]);
        }

        // 2. Data Calculation
        for (size_t i = 0; i < input_data.size(); i++)
        {
            uint8_t d = input_data[i];
            uint8_t feedback = d ^ parity[0];
            for (int j = 0; j < 15; j++)
                parity[j] = parity[j + 1] ^ gf_mul_sw(feedback, GEN_POLY_TB[15 - j]);
            parity[15] = gf_mul_sw(feedback, GEN_POLY_TB[0]);

            // 軟體模型也要把 Data 存入預期輸出
            expected_output.push_back(d);
        }

        // 3. Append Parity
        for (int i = 0; i < 16; i++)
            expected_output.push_back(parity[i]);
    }
    else if (input_data.size() == 274)
    {
        // --- 8PSK Mode (Split into 2 chunks) ---

        // Chunk 1 (0~136)
        {
            uint8_t parity[16] = {0};
            // Padding 102
            for (int k = 0; k < 102; k++)
            {
                uint8_t feedback = 0 ^ parity[0];
                for (int j = 0; j < 15; j++)
                    parity[j] = parity[j + 1] ^ gf_mul_sw(feedback, GEN_POLY_TB[15 - j]);
                parity[15] = gf_mul_sw(feedback, GEN_POLY_TB[0]);
            }
            // Data
            for (int i = 0; i < 137; i++)
            {
                uint8_t d = input_data[i];
                uint8_t feedback = d ^ parity[0];
                for (int j = 0; j < 15; j++)
                    parity[j] = parity[j + 1] ^ gf_mul_sw(feedback, GEN_POLY_TB[15 - j]);
                parity[15] = gf_mul_sw(feedback, GEN_POLY_TB[0]);
                expected_output.push_back(d);
            }
            // Parity
            for (int i = 0; i < 16; i++)
                expected_output.push_back(parity[i]);
        }

        // Chunk 2 (137~273)
        {
            uint8_t parity[16] = {0};
            // Padding 102
            for (int k = 0; k < 102; k++)
            {
                uint8_t feedback = 0 ^ parity[0];
                for (int j = 0; j < 15; j++)
                    parity[j] = parity[j + 1] ^ gf_mul_sw(feedback, GEN_POLY_TB[15 - j]);
                parity[15] = gf_mul_sw(feedback, GEN_POLY_TB[0]);
            }
            // Data
            for (int i = 0; i < 137; i++)
            {
                uint8_t d = input_data[137 + i]; // Offset
                uint8_t feedback = d ^ parity[0];
                for (int j = 0; j < 15; j++)
                    parity[j] = parity[j + 1] ^ gf_mul_sw(feedback, GEN_POLY_TB[15 - j]);
                parity[15] = gf_mul_sw(feedback, GEN_POLY_TB[0]);
                expected_output.push_back(d);
            }
            // Parity
            for (int i = 0; i < 16; i++)
                expected_output.push_back(parity[i]);
        }
    }
}

// =========================================================
// 主程式
// =========================================================
int main()
{
    hls::stream<axis_word> s_axis;
    hls::stream<axis_word> m_axis;

    // 儲存所有要測試的輸入資料 (為了給 SW Model 用)
    std::vector<std::vector<uint8_t>> input_packets;
    // 儲存預期的正確輸出資料
    std::vector<uint8_t> expected_stream;

    printf("============================================================\n");
    printf("   RS Encoder: Bit-True Verification & Stress Test\n");
    printf("============================================================\n");

    // 1. 定義測試劇本 (長度)
    std::vector<int> test_lens = {
        LEN_QPSK_IN, // Packet 0
        LEN_8PSK_IN, // Packet 1
        LEN_QPSK_IN, // Packet 2
        LEN_8PSK_IN, // Packet 3
        LEN_8PSK_IN  // Packet 4
    };

    // 2. 產生輸入資料 並 計算黃金標準 (Golden Reference)
    printf("[Gen] Generating Packets and Golden Reference...\n");

    for (size_t i = 0; i < test_lens.size(); i++)
    {
        int len = test_lens[i];
        std::vector<uint8_t> packet;

        // 產生隨機資料
        for (int j = 0; j < len; j++)
        {
            if (j == 0)
                packet.push_back(0x47); // Sync
            else
                packet.push_back((uint8_t)((i * j + j) & 0xFF)); // Random-ish data
        }
        input_packets.push_back(packet);

        // A. 送入 Hardware Stream
        for (size_t j = 0; j < packet.size(); j++)
        {
            axis_word in;
            in.data = packet[j];
            in.keep = 1;
            in.strb = 1;
            in.last = (j == packet.size() - 1) ? 1 : 0;
            s_axis.write(in);
        }

        // B. 計算 Software Golden Result
        run_sw_rs_model(packet, expected_stream);
    }

    printf("[Gen] Total Input Bytes: %lu\n", expected_stream.size()); // 注意：expected 包含 parity

    // 3. 執行硬體 (Hardware Kernel)
    printf("[HW] Running RS Encoder Kernel...\n");
    rs_encoder_tx(s_axis, m_axis);

    // 4. 比對驗證 (Verification)
    printf("[Chk] Verifying Output...\n");

    int errors = 0;
    int out_idx = 0;

    while (!m_axis.empty())
    {
        axis_word hw_out = m_axis.read();

        if (out_idx >= expected_stream.size())
        {
            printf("Error: HW produced more data than expected!\n");
            errors++;
            break;
        }

        uint8_t sw_byte = expected_stream[out_idx];

        if (hw_out.data != sw_byte)
        {
            printf("Error at Byte %d: HW=0x%02X, SW=0x%02X\n",
                   out_idx, (uint8_t)hw_out.data, sw_byte);
            errors++;
            // 為了不讓錯誤訊息洗版，超過 10 個錯誤就停
            if (errors > 10)
                break;
        }

        out_idx++;
    }

    if (out_idx < expected_stream.size())
    {
        printf("Error: HW produced LESS data than expected (Got %d, Exp %lu)\n",
               out_idx, expected_stream.size());
        errors++;
    }

    // 5. 結果報告
    printf("============================================================\n");
    if (errors == 0)
    {
        printf("PASS: Hardware matches Software Golden Model perfectly.\n");
        printf("      Processed %lu bytes (Data + Parity).\n", expected_stream.size());
    }
    else
    {
        printf("FAIL: Found %d mismatches.\n", errors);
        return 1;
    }

    // ============================================================
    // 6. Golden Pattern Dump (for RX testbench)
    //    Input pattern: data[0]=0x47, data[i]=i&0xFF
    // ============================================================
    printf("\n============================================================\n");
    printf("   Golden Pattern Dump (copy into RX testbench)\n");
    printf("============================================================\n");

    // --- QPSK: 188 bytes in, 204 bytes out ---
    {
        std::vector<uint8_t> qpsk_in(188);
        std::vector<uint8_t> qpsk_out;
        qpsk_in[0] = 0x47;
        for (int i = 1; i < 188; i++)
            qpsk_in[i] = i & 0xFF;
        run_sw_rs_model(qpsk_in, qpsk_out);

        printf("\n// QPSK input (188 bytes): data[0]=0x47, data[i]=i\n");
        printf("const uint8_t golden_qpsk_input[188] = {\n    ");
        for (int i = 0; i < 188; i++)
        {
            printf("0x%02X", qpsk_in[i]);
            if (i < 187) printf(", ");
            if ((i + 1) % 16 == 0 && i < 187) printf("\n    ");
        }
        printf("\n};\n");

        printf("\n// QPSK codeword (204 bytes): SW golden output\n");
        printf("const uint8_t golden_qpsk_codeword[204] = {\n    ");
        for (int i = 0; i < 204; i++)
        {
            printf("0x%02X", qpsk_out[i]);
            if (i < 203) printf(", ");
            if ((i + 1) % 16 == 0 && i < 203) printf("\n    ");
        }
        printf("\n};\n");
    }

    // --- 8PSK: 274 bytes in, 306 bytes out ---
    {
        std::vector<uint8_t> psk8_in(274);
        std::vector<uint8_t> psk8_out;
        psk8_in[0] = 0x47;
        for (int i = 1; i < 274; i++)
            psk8_in[i] = i & 0xFF;
        run_sw_rs_model(psk8_in, psk8_out);

        printf("\n// 8PSK input (274 bytes): data[0]=0x47, data[i]=i\n");
        printf("const uint8_t golden_8psk_input[274] = {\n    ");
        for (int i = 0; i < 274; i++)
        {
            printf("0x%02X", psk8_in[i]);
            if (i < 273) printf(", ");
            if ((i + 1) % 16 == 0 && i < 273) printf("\n    ");
        }
        printf("\n};\n");

        printf("\n// 8PSK codeword (306 bytes): SW golden output\n");
        printf("const uint8_t golden_8psk_codeword[306] = {\n    ");
        for (int i = 0; i < 306; i++)
        {
            printf("0x%02X", psk8_out[i]);
            if (i < 305) printf(", ");
            if ((i + 1) % 16 == 0 && i < 305) printf("\n    ");
        }
        printf("\n};\n");
    }

    printf("\n============================================================\n");
    return 0;
}