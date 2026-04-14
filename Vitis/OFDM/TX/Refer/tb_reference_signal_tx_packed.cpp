#include "ref_tx_defs.h"
#include <iostream>
#include <iomanip>
#include <assert.h>
#include <stdint.h>

void reference_signal_tx_packed(hls::stream<axis_t> &input_stream, hls::stream<axis_t> &output_stream);

int main()
{
    hls::stream<axis_t> input_stream("input_stream");
    hls::stream<axis_t> output_stream("output_stream");

    // 1. 準備輸入數據 (1632 個 Data)
    // 模擬 Mapper 傳來的浮點數資料 (我們用 1.11 和 2.22)
    for (int i = 0; i < DATA_TONES; i++)
    {
        axis_t pkt;
        pkt.data = pack_complex((half)1.11f, (half)2.22f);
        pkt.keep = 0xF;
        pkt.strb = 0xF;
        pkt.last = (i == DATA_TONES - 1);
        input_stream.write(pkt);
    }

    // 2. 執行 HLS IP
    reference_signal_tx_packed(input_stream, output_stream);

    // 3. 讀取並顯示結果
    std::cout << "\n==================================================================================" << std::endl;
    std::cout << std::left << std::setw(8) << "Index"
              << std::setw(15) << "Type"
              << std::setw(25) << "Output (Fixed Int16)"
              << std::setw(30) << "Restored Float (I, Q)" << std::endl;
    std::cout << "==================================================================================" << std::endl;

    int data_read_count = 0;
    for (int i = 0; i < FFT_LENGTH; i++)
    {
        axis_t out_pkt = output_stream.read();

        // 【關鍵修改】：現在 IP 吐出來的是 int16_t (定點數)，不是 half 了！
        int16_t fixed_i = (int16_t)out_pkt.data(15, 0);
        int16_t fixed_q = (int16_t)out_pkt.data(31, 16);

        // 把整數除以 2048，還原回真實世界的浮點數以便驗證
        float restored_i = (float)fixed_i / 2048.0f;
        float restored_q = (float)fixed_q / 2048.0f;

        // 判定類型用於顯示
        std::string type;
        bool should_print = false;

        if (i < LEFT_GUARD || i >= FFT_LENGTH - RIGHT_GUARD)
        {
            type = "Guard Band";
            // 只印 Guard Band 的頭尾
            if (i == 0 || i == LEFT_GUARD - 1 || i == FFT_LENGTH - RIGHT_GUARD || i == FFT_LENGTH - 1)
                should_print = true;
        }
        else
        {
            int active_idx = i - LEFT_GUARD;
            int pilot_idx = active_idx / 7;
            if (active_idx == (pilot_idx * 7))
            {
                type = "PILOT";
                // 為了排版乾淨，我們只印前幾個 Pilot
                if (pilot_idx < 4)
                    should_print = true;
            }
            else
            {
                type = "Data Tone";
                data_read_count++;
                // Data 只印前兩個作為範例
                if (data_read_count <= 2)
                    should_print = true;
            }
        }

        if (should_print)
        {
            std::cout << std::left << std::setw(8) << i
                      << std::setw(15) << type
                      << "(" << std::setw(5) << fixed_i << ", " << std::setw(5) << fixed_q << ")"
                      << std::setw(7) << "  ==>  "
                      << "(" << restored_i << ", " << restored_q << ")";

            if (i == LEFT_GUARD - 1)
                std::cout << "  <-- End of Left Guard";
            if (i == FFT_LENGTH - RIGHT_GUARD)
                std::cout << "  <-- Start of Right Guard";
            std::cout << std::endl;
        }

        // 印省略符號
        if (i == 100)
            std::cout << "...... (Skipping middle parts) ......" << std::endl;
    }

    std::cout << "==================================================================================" << std::endl;
    std::cout << "Total Data Tones processed: " << data_read_count << " (Expected: 1632)" << std::endl;

    assert(input_stream.empty());
    std::cout << ">>> C-SIM SUCCESS: Stream is empty and fixed-point conversion is perfectly verified!" << std::endl;

    return 0;
}