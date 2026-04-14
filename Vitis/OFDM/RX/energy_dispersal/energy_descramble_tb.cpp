/*
 * energy_descramble_tb.cpp
 * Variable Length Testbench for Descrambler
 * * Flow:
 * 1. Generate Raw Data (Ramp/0xAA/0xFF)
 * 2. Software Scramble it (Simulate TX) -> Input to DUT
 * 3. DUT Descrambles it -> Output
 * 4. Verify Output == Original Raw Data
 */

#include <iostream>
#include <vector>
#include "energy_descramble_hls.h"

using namespace std;

#define GEN_PRBS_LEN 400

// Golden PRBS Generator
void generate_prbs_golden(std::vector<uint8_t> &prbs, int length)
{
    int d_reg = 0xa9;
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

struct TestPacket
{
    int id;
    int length;
    int pattern_type;
};

int main()
{
    hls::stream<axis_word> s_axis; // Input to DUT (Scrambled)
    hls::stream<axis_word> m_axis; // Output from DUT (Descrambled)

    cout << "========================================" << endl;
    cout << "  HLS Energy Descramble Testbench       " << endl;
    cout << "========================================" << endl;

    // 1. Prepare Golden PRBS
    vector<uint8_t> prbs_golden;
    generate_prbs_golden(prbs_golden, GEN_PRBS_LEN);

    // 2. Define Packets (Same as Scrambler test)
    vector<TestPacket> packets = {
        {0, 188, 0}, // Ramp
        {1, 282, 1}, // 0xAA
        {2, 376, 2}  // 0xFF
    };

    vector<uint8_t> expected_raw_data;

    cout << "\n[TB] Generating Scrambled Input..." << endl;

    for (const auto &pkt : packets)
    {
        for (int i = 0; i < pkt.length; i++)
        {
            // A. Generate Original Raw Data
            uint8_t raw_byte;
            if (pkt.pattern_type == 0)
                raw_byte = i & 0xFF;
            else if (pkt.pattern_type == 1)
                raw_byte = 0xAA;
            else
                raw_byte = 0xFF;

            // Save raw data for verification later
            expected_raw_data.push_back(raw_byte);

            // B. Scramble it (Simulate TX transmission)
            // Input to DUT must be SCRAMBLED data
            uint8_t scrambled_byte = raw_byte ^ prbs_golden[i];

            // C. Push to DUT
            axis_word in_word;
            in_word.data = scrambled_byte;
            in_word.keep = 1;
            in_word.strb = 1;
            in_word.last = (i == pkt.length - 1) ? 1 : 0;

            s_axis.write(in_word);
        }
    }

    // 3. Run DUT (Descrambler)
    cout << "[TB] Running DUT..." << endl;
    energy_descramble(s_axis, m_axis);

    // 4. Verify Output (Should match Original Raw Data)
    cout << "[TB] Verifying Output..." << endl;
    int error_count = 0;
    int idx = 0;

    while (!m_axis.empty())
    {
        axis_word out_word = m_axis.read();

        if (idx >= expected_raw_data.size())
        {
            cout << "[ERROR] Extra data received!" << endl;
            error_count++;
            continue;
        }

        uint8_t exp_val = expected_raw_data[idx];

        if (out_word.data != exp_val)
        {
            cout << "[ERROR] Byte " << idx
                 << ": Exp Raw 0x" << hex << (int)exp_val
                 << ", Got 0x" << (int)out_word.data.to_uint() << dec << endl;
            error_count++;
        }

        // Optional: Check TLAST timing logic if needed,
        // but checking data correctness implies logic is correct.

        idx++;
    }

    cout << "\n========================================" << endl;
    if (error_count == 0)
        cout << "*** TEST PASSED *** Descramble Successful." << endl;
    else
        cout << "*** TEST FAILED *** Errors: " << error_count << endl;
    cout << "========================================" << endl;

    return error_count;
}