#include "rs_encoder_tx.h"

// =========================================================
// Hardwired GF Multiplication (No LUT, Pure XOR)
// Primitive Polynomial: 0x11D (x^8 + x^4 + x^3 + x^2 + 1)
// =========================================================

// GEN_POLY[0] = 0x3B, GEN_POLY[15] = 0x3B
inline uint8_t gf_mul_0x3B(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0x3B;
    if (fb & 0x02)
        r ^= 0x76;
    if (fb & 0x04)
        r ^= 0xEC;
    if (fb & 0x08)
        r ^= 0xC5;
    if (fb & 0x10)
        r ^= 0x97;
    if (fb & 0x20)
        r ^= 0x33;
    if (fb & 0x40)
        r ^= 0x66;
    if (fb & 0x80)
        r ^= 0xCC;
    return r;
}

// GEN_POLY[1] = 0x24
inline uint8_t gf_mul_0x24(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0x24;
    if (fb & 0x02)
        r ^= 0x48;
    if (fb & 0x04)
        r ^= 0x90;
    if (fb & 0x08)
        r ^= 0x3D;
    if (fb & 0x10)
        r ^= 0x7A;
    if (fb & 0x20)
        r ^= 0xF4;
    if (fb & 0x40)
        r ^= 0xF5;
    if (fb & 0x80)
        r ^= 0xF7;
    return r;
}

// GEN_POLY[2] = 0x32
inline uint8_t gf_mul_0x32(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0x32;
    if (fb & 0x02)
        r ^= 0x64;
    if (fb & 0x04)
        r ^= 0xC8;
    if (fb & 0x08)
        r ^= 0x8D;
    if (fb & 0x10)
        r ^= 0x07;
    if (fb & 0x20)
        r ^= 0x0E;
    if (fb & 0x40)
        r ^= 0x1C;
    if (fb & 0x80)
        r ^= 0x38;
    return r;
}

// GEN_POLY[3] = 0x62
inline uint8_t gf_mul_0x62(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0x62;
    if (fb & 0x02)
        r ^= 0xC4;
    if (fb & 0x04)
        r ^= 0x95;
    if (fb & 0x08)
        r ^= 0x37;
    if (fb & 0x10)
        r ^= 0x6E;
    if (fb & 0x20)
        r ^= 0xDC;
    if (fb & 0x40)
        r ^= 0xA5;
    if (fb & 0x80)
        r ^= 0x57;
    return r;
}

// GEN_POLY[4] = 0xE5
inline uint8_t gf_mul_0xE5(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0xE5;
    if (fb & 0x02)
        r ^= 0xD7;
    if (fb & 0x04)
        r ^= 0xB3;
    if (fb & 0x08)
        r ^= 0x7B;
    if (fb & 0x10)
        r ^= 0xF6;
    if (fb & 0x20)
        r ^= 0xF1;
    if (fb & 0x40)
        r ^= 0xFF;
    if (fb & 0x80)
        r ^= 0xE3;
    return r;
}

// GEN_POLY[5] = 0x29
inline uint8_t gf_mul_0x29(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0x29;
    if (fb & 0x02)
        r ^= 0x52;
    if (fb & 0x04)
        r ^= 0xA4;
    if (fb & 0x08)
        r ^= 0x55;
    if (fb & 0x10)
        r ^= 0xAA;
    if (fb & 0x20)
        r ^= 0x49;
    if (fb & 0x40)
        r ^= 0x92;
    if (fb & 0x80)
        r ^= 0x39;
    return r;
}

// GEN_POLY[6] = 0x41
inline uint8_t gf_mul_0x41(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0x41;
    if (fb & 0x02)
        r ^= 0x82;
    if (fb & 0x04)
        r ^= 0x19;
    if (fb & 0x08)
        r ^= 0x32;
    if (fb & 0x10)
        r ^= 0x64;
    if (fb & 0x20)
        r ^= 0xC8;
    if (fb & 0x40)
        r ^= 0x8D;
    if (fb & 0x80)
        r ^= 0x07;
    return r;
}

// GEN_POLY[7] = 0xA3
inline uint8_t gf_mul_0xA3(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0xA3;
    if (fb & 0x02)
        r ^= 0x5B;
    if (fb & 0x04)
        r ^= 0xB6;
    if (fb & 0x08)
        r ^= 0x71;
    if (fb & 0x10)
        r ^= 0xE2;
    if (fb & 0x20)
        r ^= 0xD9;
    if (fb & 0x40)
        r ^= 0xAF;
    if (fb & 0x80)
        r ^= 0x43;
    return r;
}

// GEN_POLY[8] = 0x08
inline uint8_t gf_mul_0x08(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0x08;
    if (fb & 0x02)
        r ^= 0x10;
    if (fb & 0x04)
        r ^= 0x20;
    if (fb & 0x08)
        r ^= 0x40;
    if (fb & 0x10)
        r ^= 0x80;
    if (fb & 0x20)
        r ^= 0x1D;
    if (fb & 0x40)
        r ^= 0x3A;
    if (fb & 0x80)
        r ^= 0x74;
    return r;
}

// GEN_POLY[9] = 0x1E
inline uint8_t gf_mul_0x1E(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0x1E;
    if (fb & 0x02)
        r ^= 0x3C;
    if (fb & 0x04)
        r ^= 0x78;
    if (fb & 0x08)
        r ^= 0xF0;
    if (fb & 0x10)
        r ^= 0xFD;
    if (fb & 0x20)
        r ^= 0xE7;
    if (fb & 0x40)
        r ^= 0xD3;
    if (fb & 0x80)
        r ^= 0xBB;
    return r;
}

// GEN_POLY[10] = 0xD1
inline uint8_t gf_mul_0xD1(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0xD1;
    if (fb & 0x02)
        r ^= 0xBF;
    if (fb & 0x04)
        r ^= 0x63;
    if (fb & 0x08)
        r ^= 0xC6;
    if (fb & 0x10)
        r ^= 0x91;
    if (fb & 0x20)
        r ^= 0x3F;
    if (fb & 0x40)
        r ^= 0x7E;
    if (fb & 0x80)
        r ^= 0xFC;
    return r;
}

// GEN_POLY[11] = 0x44
inline uint8_t gf_mul_0x44(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0x44;
    if (fb & 0x02)
        r ^= 0x88;
    if (fb & 0x04)
        r ^= 0x0D;
    if (fb & 0x08)
        r ^= 0x1A;
    if (fb & 0x10)
        r ^= 0x34;
    if (fb & 0x20)
        r ^= 0x68;
    if (fb & 0x40)
        r ^= 0xD0;
    if (fb & 0x80)
        r ^= 0xBD;
    return r;
}

// GEN_POLY[12] = 0xBD
inline uint8_t gf_mul_0xBD(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0xBD;
    if (fb & 0x02)
        r ^= 0x67;
    if (fb & 0x04)
        r ^= 0xCE;
    if (fb & 0x08)
        r ^= 0x81;
    if (fb & 0x10)
        r ^= 0x1F;
    if (fb & 0x20)
        r ^= 0x3E;
    if (fb & 0x40)
        r ^= 0x7C;
    if (fb & 0x80)
        r ^= 0xF8;
    return r;
}

// GEN_POLY[13] = 0x68
inline uint8_t gf_mul_0x68(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0x68;
    if (fb & 0x02)
        r ^= 0xD0;
    if (fb & 0x04)
        r ^= 0xBD;
    if (fb & 0x08)
        r ^= 0x67;
    if (fb & 0x10)
        r ^= 0xCE;
    if (fb & 0x20)
        r ^= 0x81;
    if (fb & 0x40)
        r ^= 0x1F;
    if (fb & 0x80)
        r ^= 0x3E;
    return r;
}

// GEN_POLY[14] = 0x0D
inline uint8_t gf_mul_0x0D(uint8_t fb)
{
#pragma HLS INLINE
    uint8_t r = 0;
    if (fb & 0x01)
        r ^= 0x0D;
    if (fb & 0x02)
        r ^= 0x1A;
    if (fb & 0x04)
        r ^= 0x34;
    if (fb & 0x08)
        r ^= 0x68;
    if (fb & 0x10)
        r ^= 0xD0;
    if (fb & 0x20)
        r ^= 0xBD;
    if (fb & 0x40)
        r ^= 0x67;
    if (fb & 0x80)
        r ^= 0xCE;
    return r;
}

// =========================================================
// Hardwired LFSR Step - CORRECTED MAPPING
// =========================================================
// Original logic:
//   for (j = 0; j < 15; j++)
//       parity[j] = parity[j+1] ^ fb * GEN_POLY[15-j]
//   parity[15] = fb * GEN_POLY[0]
//
// Mapping:
//   parity[0]  <- GEN_POLY[15] = 0x3B
//   parity[1]  <- GEN_POLY[14] = 0x0D
//   parity[2]  <- GEN_POLY[13] = 0x68
//   parity[3]  <- GEN_POLY[12] = 0xBD
//   parity[4]  <- GEN_POLY[11] = 0x44
//   parity[5]  <- GEN_POLY[10] = 0xD1
//   parity[6]  <- GEN_POLY[9]  = 0x1E
//   parity[7]  <- GEN_POLY[8]  = 0x08
//   parity[8]  <- GEN_POLY[7]  = 0xA3
//   parity[9]  <- GEN_POLY[6]  = 0x41
//   parity[10] <- GEN_POLY[5]  = 0x29
//   parity[11] <- GEN_POLY[4]  = 0xE5
//   parity[12] <- GEN_POLY[3]  = 0x62
//   parity[13] <- GEN_POLY[2]  = 0x32
//   parity[14] <- GEN_POLY[1]  = 0x24
//   parity[15] <- GEN_POLY[0]  = 0x3B
// =========================================================
inline void lfsr_step_hardwired(uint8_t input, uint8_t parity[16])
{
#pragma HLS INLINE
#pragma HLS ARRAY_PARTITION variable = parity complete

    uint8_t fb = input ^ parity[0];

    // Parallel GF multiplications
    uint8_t m_3B = gf_mul_0x3B(fb); // for parity[0] and parity[15]
    uint8_t m_0D = gf_mul_0x0D(fb); // for parity[1]
    uint8_t m_68 = gf_mul_0x68(fb); // for parity[2]
    uint8_t m_BD = gf_mul_0xBD(fb); // for parity[3]
    uint8_t m_44 = gf_mul_0x44(fb); // for parity[4]
    uint8_t m_D1 = gf_mul_0xD1(fb); // for parity[5]
    uint8_t m_1E = gf_mul_0x1E(fb); // for parity[6]
    uint8_t m_08 = gf_mul_0x08(fb); // for parity[7]
    uint8_t m_A3 = gf_mul_0xA3(fb); // for parity[8]
    uint8_t m_41 = gf_mul_0x41(fb); // for parity[9]
    uint8_t m_29 = gf_mul_0x29(fb); // for parity[10]
    uint8_t m_E5 = gf_mul_0xE5(fb); // for parity[11]
    uint8_t m_62 = gf_mul_0x62(fb); // for parity[12]
    uint8_t m_32 = gf_mul_0x32(fb); // for parity[13]
    uint8_t m_24 = gf_mul_0x24(fb); // for parity[14]

    // Shift and XOR - CORRECTED ORDER
    parity[0] = parity[1] ^ m_3B;   // GEN_POLY[15] = 0x3B
    parity[1] = parity[2] ^ m_0D;   // GEN_POLY[14] = 0x0D
    parity[2] = parity[3] ^ m_68;   // GEN_POLY[13] = 0x68
    parity[3] = parity[4] ^ m_BD;   // GEN_POLY[12] = 0xBD
    parity[4] = parity[5] ^ m_44;   // GEN_POLY[11] = 0x44
    parity[5] = parity[6] ^ m_D1;   // GEN_POLY[10] = 0xD1
    parity[6] = parity[7] ^ m_1E;   // GEN_POLY[9]  = 0x1E
    parity[7] = parity[8] ^ m_08;   // GEN_POLY[8]  = 0x08
    parity[8] = parity[9] ^ m_A3;   // GEN_POLY[7]  = 0xA3
    parity[9] = parity[10] ^ m_41;  // GEN_POLY[6]  = 0x41
    parity[10] = parity[11] ^ m_29; // GEN_POLY[5]  = 0x29
    parity[11] = parity[12] ^ m_E5; // GEN_POLY[4]  = 0xE5
    parity[12] = parity[13] ^ m_62; // GEN_POLY[3]  = 0x62
    parity[13] = parity[14] ^ m_32; // GEN_POLY[2]  = 0x32
    parity[14] = parity[15] ^ m_24; // GEN_POLY[1]  = 0x24
    parity[15] = m_3B;              // GEN_POLY[0]  = 0x3B
}

// =========================================================
// Main Function
// =========================================================
void rs_encoder_tx(
    hls::stream<axis_word> &input_stream,
    hls::stream<axis_word> &output_stream)
{
#pragma HLS INTERFACE axis port = input_stream
#pragma HLS INTERFACE axis port = output_stream
#pragma HLS INTERFACE ap_ctrl_none port = return

    static uint8_t data_buf[274];
    static uint8_t parity[16];

#pragma HLS ARRAY_PARTITION variable = parity complete
#pragma HLS ARRAY_PARTITION variable = data_buf cyclic factor = 4 dim = 1

    while (1)
    {
#ifndef __SYNTHESIS__
        if (input_stream.empty())
            break;
#endif

        int valid_len = 0;
        bool last = false;

        // 刪除原本的 bool synced = false;

        // =================================================
        // State 1: COLLECT Data (純 AXI-Stream 同步)
        // =================================================
    COLLECT_LOOP:
        do
        {
#pragma HLS PIPELINE II = 1
            axis_word in_val = input_stream.read();

            // 無條件直接存入資料，不再尋找 0x47
            // 最大容量防護：確保不超過 274 Bytes (8PSK 最大長度)
            if (valid_len < 274)
            {
                data_buf[valid_len] = in_val.data;
                valid_len++;
            }

            // 100% 信任硬體的 TLAST 訊號來判斷封包什麼時候收完
            if (in_val.last == 1)
            {
                last = true;
            }

        } while (!last);
        // =================================================
        // State 2: Process
        // =================================================
        if (valid_len == 188)
        {
            // --- QPSK Mode ---

        QPSK_RESET:
            for (int i = 0; i < 16; i++)
            {
#pragma HLS UNROLL
                parity[i] = 0;
            }

        QPSK_PAD:
            for (int i = 0; i < 51; i++)
            {
#pragma HLS PIPELINE II = 1
                lfsr_step_hardwired(0, parity);
            }

        QPSK_DATA:
            for (int i = 0; i < 188; i++)
            {
#pragma HLS PIPELINE II = 1
                uint8_t d = data_buf[i];
                lfsr_step_hardwired(d, parity);

                axis_word out;
                out.data = d;
                out.keep = 1;
                out.strb = 1;
                out.last = 0;
                output_stream.write(out);
            }

        QPSK_PARITY:
            for (int i = 0; i < 16; i++)
            {
#pragma HLS PIPELINE II = 1
                axis_word out;
                out.data = parity[i];
                out.keep = 1;
                out.strb = 1;
                out.last = (i == 15) ? 1 : 0;
                output_stream.write(out);
            }
        }
        else if (valid_len == 274)
        {
            // --- 8PSK Mode ---

            // === PATH A ===
        PSK8_A_RESET:
            for (int i = 0; i < 16; i++)
            {
#pragma HLS UNROLL
                parity[i] = 0;
            }

        PSK8_A_PAD:
            for (int i = 0; i < 102; i++)
            {
#pragma HLS PIPELINE II = 1
                lfsr_step_hardwired(0, parity);
            }

        PSK8_A_DATA:
            for (int i = 0; i < 137; i++)
            {
#pragma HLS PIPELINE II = 1
                uint8_t d = data_buf[i];
                lfsr_step_hardwired(d, parity);

                axis_word out;
                out.data = d;
                out.keep = 1;
                out.strb = 1;
                out.last = 0;
                output_stream.write(out);
            }

        PSK8_A_PARITY:
            for (int i = 0; i < 16; i++)
            {
#pragma HLS PIPELINE II = 1
                axis_word out;
                out.data = parity[i];
                out.keep = 1;
                out.strb = 1;
                out.last = 0;
                output_stream.write(out);
            }

            // === PATH B ===
        PSK8_B_RESET:
            for (int i = 0; i < 16; i++)
            {
#pragma HLS UNROLL
                parity[i] = 0;
            }

        PSK8_B_PAD:
            for (int i = 0; i < 102; i++)
            {
#pragma HLS PIPELINE II = 1
                lfsr_step_hardwired(0, parity);
            }

        PSK8_B_DATA:
            for (int i = 0; i < 137; i++)
            {
#pragma HLS PIPELINE II = 1
                uint8_t d = data_buf[137 + i];
                lfsr_step_hardwired(d, parity);

                axis_word out;
                out.data = d;
                out.keep = 1;
                out.strb = 1;
                out.last = 0;
                output_stream.write(out);
            }

        PSK8_B_PARITY:
            for (int i = 0; i < 16; i++)
            {
#pragma HLS PIPELINE II = 1
                axis_word out;
                out.data = parity[i];
                out.keep = 1;
                out.strb = 1;
                out.last = (i == 15) ? 1 : 0;
                output_stream.write(out);
            }
        }
        else
        {
            // Debug: 輸出 IP 實際算到的長度
            // Byte 0: 長度低位 (Low Byte)
            // Byte 1: 長度高位 (High Byte)
            // Byte 2, 3: 保持 EE 作為標記

            // Output Byte 0 (Low)
            axis_word out0;
            out0.data = valid_len & 0xFF;
            out0.keep = 1;
            out0.strb = 1;
            out0.last = 0;
            output_stream.write(out0);

            // Output Byte 1 (Len High) - 保持不變
            axis_word out1;
            out1.data = (valid_len >> 8) & 0xFF;
            out1.keep = 1;
            out1.strb = 1;
            out1.last = 0;
            output_stream.write(out1);

            // Output Byte 2 (關鍵修改：吐出 Buffer 的第一個 Byte)
            axis_word out2;
            out2.data = data_buf[0]; // <--- 把原本的 0xEE 改成這個！
            out2.keep = 1;
            out2.strb = 1;
            out2.last = 0;
            output_stream.write(out2);

            // Output Byte 3 (Marker + TLAST) - 保持不變
            axis_word out3;
            out3.data = 0xEE;
            out3.keep = 1;
            out3.strb = 1;
            out3.last = 1; // 拉起 TLAST
            output_stream.write(out3);
        }
    }
}