/*
 * energy_dispersal_tb.cpp
 * Variable Length & Multi-Packet Testbench
 *
 * Scenarios:
 * 1. Packet 0: Ramp pattern, Length = 188
 * 2. Packet 1: 0xAA pattern, Length = 282
 * 3. Packet 2: 0xFF pattern, Length = 376
 *
 * Verifies:
 * - TLAST correctness for variable lengths
 * - PRBS index reset mechanism (each packet starts scrambling from PRBS[0])
 */

#include <iostream>
#include <vector>
#include <iomanip>
#include "energy_dispersal_hls.h"

using namespace std;

// 定義最大需要的 PRBS 生成長度 (大於最大的 376 即可)
#define GEN_PRBS_LEN 400

//=============================================================================
// Golden Model: PRBS Generator (Polynomial: x^15 + x^14 + 1)
//=============================================================================
void generate_prbs_golden(std::vector<uint8_t> &prbs, int length)
{
    int d_reg = 0xa9; // Initial state: 10101001
    prbs.clear();

    for (int i = 0; i < length; i++)
    {
        int res = 0;
        for (int j = 0; j < 8; j++)
        {
            int feedback = ((d_reg >> 13) ^ (d_reg >> 14)) & 0x1;
            d_reg = ((d_reg << 1) | feedback) & 0x7fff;
            res = (res << 1) | feedback;
        }
        prbs.push_back((uint8_t)res);
    }
}

//=============================================================================
// Packet Structure Definition
//=============================================================================
struct TestPacket
{
    int id;
    int length;
    int pattern_type; // 0: Ramp, 1: 0xAA, 2: 0xFF
};

//=============================================================================
// Main Testbench
//=============================================================================
int main()
{
    hls::stream<axis_word> s_axis;
    hls::stream<axis_word> m_axis;

    cout << "========================================" << endl;
    cout << "  HLS Energy Dispersal: Variable Length " << endl;
    cout << "========================================" << endl;

    // 1. Generate Golden PRBS (Mathematically)
    // 我們生成 400 bytes 的標準答案，用來跟 HLS 的 ROM 比對
    vector<uint8_t> prbs_golden;
    generate_prbs_golden(prbs_golden, GEN_PRBS_LEN);

    // 2. Define Test Packets
    vector<TestPacket> packets = {
        {0, 188, 0}, // Ramp
        {1, 282, 1}, // 0xAA
        {2, 376, 2}  // 0xFF
    };

    // 儲存所有預期的輸出數據，用於最後比對
    struct ExpectedData
    {
        uint8_t data;
        bool last;
        int pkt_id;
        int byte_idx;
    };
    vector<ExpectedData> all_expected;

    cout << "\n[TB] Generating Input Data..." << endl;

    // 3. Generate Input Stream
    for (const auto &pkt : packets)
    {
        cout << "  -> Packet " << pkt.id << ": Length=" << pkt.length
             << ", Type=" << pkt.pattern_type << endl;

        for (int i = 0; i < pkt.length; i++)
        {
            axis_word in_word;
            uint8_t input_byte;

            // Pattern Generation
            if (pkt.pattern_type == 0)
                input_byte = i & 0xFF; // Ramp
            else if (pkt.pattern_type == 1)
                input_byte = 0xAA; // Fixed 0xAA
            else
                input_byte = 0xFF; // Fixed 0xFF

            in_word.data = input_byte;
            in_word.keep = 1;
            in_word.strb = 1;
            // 設定 TLAST (只在該封包的最後一個 Byte)
            in_word.last = (i == pkt.length - 1) ? 1 : 0;

            s_axis.write(in_word);

            // Calculate Expected Output
            // 關鍵：每個封包的 PRBS 都是從頭 (prbs_golden[0]) 開始對應
            // 這是驗證 HLS 是否有正確重置 byte_idx 的核心
            uint8_t scrambled = input_byte ^ prbs_golden[i];

            all_expected.push_back({scrambled, (bool)in_word.last, pkt.id, i});
        }
    }
    cout << "[TB] Total bytes generated: " << all_expected.size() << endl;

    // 4. Run DUT
    cout << "\n[TB] Running DUT..." << endl;

    // 用 TB 的迴圈來模擬硬體的 Clock 不斷觸發 IP
    // 只要輸入串流裡面還有資料，就繼續呼叫 IP 去處理
    while (!s_axis.empty())
    {
        energy_dispersal(s_axis, m_axis);
    }

    cout << "[TB] DUT Finished." << endl;
    // 5. Verify Output
    cout << "\n[TB] Verifying Output..." << endl;
    int error_count = 0;
    int received_count = 0;

    while (!m_axis.empty())
    {
        axis_word out_word = m_axis.read();

        if (received_count >= all_expected.size())
        {
            cout << "[ERROR] DUT produced extra data at index " << received_count << endl;
            error_count++;
            continue;
        }

        ExpectedData exp = all_expected[received_count];

        // A. Data Check
        if (out_word.data != exp.data)
        {
            cout << "[ERROR] Pkt" << exp.pkt_id << " Byte " << exp.byte_idx
                 << ": Data Mismatch! Exp 0x" << hex << (int)exp.data
                 << ", Got 0x" << (int)out_word.data.to_uint() << dec << endl;
            error_count++;
        }

        // B. TLAST Check
        if (out_word.last != exp.last)
        {
            cout << "[ERROR] Pkt" << exp.pkt_id << " Byte " << exp.byte_idx
                 << ": TLAST Mismatch! Exp " << exp.last
                 << ", Got " << out_word.last.to_uint() << endl;
            error_count++;
        }

        received_count++;
    }

    // Check for data loss
    if (received_count != all_expected.size())
    {
        cout << "[ERROR] Data Loss! Expected " << all_expected.size()
             << " bytes, but received " << received_count << endl;
        error_count++;
    }

    // 6. Summary
    cout << "\n========================================" << endl;
    if (error_count == 0)
    {
        cout << "*** TEST PASSED ***" << endl;
        cout << "Verified 3 variable-length packets successfully." << endl;
    }
    else
    {
        cout << "*** TEST FAILED *** Errors: " << error_count << endl;
    }
    cout << "========================================" << endl;

    return (error_count == 0) ? 0 : 1;
}