#include "energy_dispersal_hls.h"

static const uint8_t prbs_rom[MAX_PRBS_SIZE] = {
#include "prbs_table_400.h"
};

void energy_dispersal(
    hls::stream<axis_word> &input_stream,
    hls::stream<axis_word> &output_stream)
{
#pragma HLS INTERFACE axis port = input_stream
#pragma HLS INTERFACE axis port = output_stream
#pragma HLS INTERFACE ap_ctrl_none port = return
    // 加上 style=flp 作為底層雙重保險 (如果 2022.2 報錯，可改為 enable_flush)
#pragma HLS PIPELINE II = 1 style = flp

    static ap_uint<10> byte_idx = 0;

#ifndef __SYNTHESIS__
    // C-Sim 防護：用 while 讓軟體測試能把資料跑完
    while (!input_stream.empty())
    {
#endif

        axis_word in_word;

        // 💡 終極殺招：read_nb() 非阻塞讀取
        // 只有當 input_stream 真的有資料 (TVALID=1) 的時候，才執行大括號裡的運算
        // 沒資料時，直接 Pass，硬體絕對不會凍結！
        if (input_stream.read_nb(in_word))
        {

            uint8_t scramble_val = prbs_rom[byte_idx];

            axis_word out_word;
            out_word.data = in_word.data ^ scramble_val;
            out_word.keep = in_word.keep;
            out_word.strb = in_word.strb;
            out_word.last = in_word.last;

            // 寫出資料
            output_stream.write(out_word);

            // 狀態更新
            if (in_word.last == 1)
            {
                byte_idx = 0;
            }
            else
            {
                byte_idx++;
            }
        }

#ifndef __SYNTHESIS__
    }
#endif
}