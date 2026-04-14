#include "energy_descramble_hls.h"

// 共用同一張 PRBS 表
static const uint8_t prbs_rom[MAX_PRBS_SIZE] = {
#include "prbs_table_400.h"
};

void energy_descramble(
    hls::stream<axis_word> &input_stream,
    hls::stream<axis_word> &output_stream)
{
#pragma HLS INTERFACE axis port = input_stream
#pragma HLS INTERFACE axis port = output_stream
#pragma HLS INTERFACE ap_ctrl_none port = return

    // 使用 Single Port ROM
#pragma HLS BIND_STORAGE variable = prbs_rom type = rom_1p impl = auto

    while (1)
    {
        // C-Sim 防護
#ifndef __SYNTHESIS__
        if (input_stream.empty())
            break;
#endif

        // 每個封包開始時，索引歸零
        // 這對於 Descrambler 至關重要，必須與 Scrambler 同步
        ap_uint<10> byte_idx = 0;
        bool is_last = false;

        do
        {
#pragma HLS PIPELINE II = 1

            // 1. 讀取 (此時進來的是 Scrambled Data)
            axis_word in_word = input_stream.read();

            // 2. 捕捉 TLAST
            is_last = (in_word.last == 1);

            // 3. XOR 解碼 (Scrambled ^ PRBS = Original)
            // 不需要 % 保護，因為封包長度保證 <= 400
            uint8_t descramble_val = prbs_rom[byte_idx];

            axis_word out_word;
            out_word.data = in_word.data ^ descramble_val;

            // Side-channels 透傳
            out_word.keep = in_word.keep;
            out_word.strb = in_word.strb;
            out_word.last = in_word.last;

            // 4. 寫出
            output_stream.write(out_word);

            // 5. 索引遞增
            byte_idx++;

        } while (!is_last);
        // 遇到 TLAST 後回到開頭，重置 byte_idx，準備解下一個封包
    }
}