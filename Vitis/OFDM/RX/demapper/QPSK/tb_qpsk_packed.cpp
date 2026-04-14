#include <iostream>
#include <fstream>
#include <iomanip>
#include "demapper_types.h"

using namespace std;

// 宣告你的 IP 核心
void qpsk_demap_packed(
    hls::stream<axis_word_32> &input_stream,
    hls::stream<axis_soft_out> &output_stream);

int main()
{
    cout << "===================================================" << endl;
    cout << "  Starting 32-bit Packed QPSK Demapper Simulation  " << endl;
    cout << "===================================================" << endl;

    hls::stream<axis_word_32> tb_input_stream("tb_input_stream");
    hls::stream<axis_soft_out> tb_output_stream("tb_output_stream");

    // =========================================================
    // Phase 1: 讀取全新的 32-bit Packed 測試資料
    // =========================================================
    ifstream fin("demap_in_symbols.txt");
    if (!fin.is_open())
    {
        cerr << "[ERROR] Cannot open demap_in_symbols.txt!" << endl;
        return -1;
    }

    for (int i = 0; i < FRAME_SYMBOLS; i++)
    {
        unsigned int val;
        fin >> hex >> val;

        axis_word_32 pkt;
        pkt.data = val; // 直接塞入 32-bit Hex (已包含 Q 與 I)
        pkt.keep = 0xF;
        pkt.strb = 0xF;
        pkt.last = (i == FRAME_SYMBOLS - 1) ? 1 : 0;

        tb_input_stream.write(pkt);
    }
    fin.close();
    cout << "[INFO] Successfully loaded and streamed " << FRAME_SYMBOLS << " 32-bit words." << endl;

    // =========================================================
    // Phase 2: 執行 Hardware IP
    // =========================================================
    cout << "[INFO] Running IP Core..." << endl;
    qpsk_demap_packed(tb_input_stream, tb_output_stream);
    cout << "[INFO] IP Core Execution Completed." << endl;

    // =========================================================
    // Phase 3: 讀取輸出、顯示前 8 個 LLR 並與 Golden 比對
    // =========================================================
    ifstream fgold("demap_golden_out.txt");
    if (!fgold.is_open())
    {
        cerr << "[ERROR] Cannot open demap_golden_out.txt!" << endl;
    }

    cout << "\n[DISPLAY] First 4 Symbols (8 LLRs) Observation:" << endl;
    cout << "-------------------------------------------------------------" << endl;
    cout << " Sym# | CH | Raw Hex | LLR (Soft Bit) | Expected (Golden) " << endl;
    cout << "------|----|---------|----------------|----------------------" << endl;

    int err_cnt = 0;
    // 總共讀取 FRAME_SYMBOLS * 2 個點
    for (int i = 0; i < FRAME_SYMBOLS * 2; i++)
    {
        if (tb_output_stream.empty())
        {
            cerr << "[ERROR] Stream empty at index " << i << endl;
            break;
        }

        axis_soft_out out_pkt = tb_output_stream.read();

        // 1. 取得硬體輸出的 LLR (透過 ap_int<8> 處理符號延伸)
        int hw_llr = (int)(ap_int<8>)out_pkt.data;

        // 2. 讀取 Golden 檔案內容
        unsigned int gold_val_raw = 0;
        if (fgold >> hex >> gold_val_raw)
        {
            int gold_llr = (int)(ap_int<8>)(gold_val_raw & 0xFF);

            // 3. 只針對前 8 個 LLR (前 4 個 Symbol) 進行詳細列印
            if (i < 8)
            {
                int sym_idx = i / 2;
                string ch_label = (i % 2 == 0) ? "I " : "Q ";

                cout << "  #" << sym_idx << "  | " << ch_label << " |   0x"
                     << hex << setw(2) << setfill('0') << (int)(out_pkt.data & 0xFF) << dec
                     << "  |      " << setw(2) << setfill(' ') << hw_llr
                     << "      |      " << setw(2) << gold_llr << endl;
            }

            // 4. 全域比對邏輯
            if (hw_llr != gold_llr)
            {
                err_cnt++;
            }
        }
    }
    cout << "-------------------------------------------------------------" << endl;
    if (fgold.is_open())
        fgold.close();

    // 總結報告
    if (err_cnt == 0)
        cout << "  [PASS] All " << FRAME_SYMBOLS * 2 << " LLRs matched!" << endl;
    else
        cout << "  [FAIL] Total Mismatches: " << err_cnt << endl;

    return err_cnt;
}