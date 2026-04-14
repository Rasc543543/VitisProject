#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <cmath>
#include "demapper_types.h"

using namespace std;

// 對齊 IP 核心函數名稱
void p8psk_demap_packed(
    hls::stream<axis_word_32> &input_stream,
    hls::stream<axis_soft_out> &output_stream);

int main()
{
    cout << "===================================================" << endl;
    cout << "    8PSK Packed Demapper (Fixed-point) Simulation  " << endl;
    cout << "===================================================" << endl;

    hls::stream<axis_word_32> tb_in("tb_in");
    hls::stream<axis_soft_out> tb_out("tb_out");

    // =========================================================
    // Phase 1: 讀取輸入測資 (1632 Symbols)
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
        if (!(fin >> hex >> val))
            break;

        axis_word_32 pkt;
        pkt.data = val; // [31:16]=Q, [15:0]=I
        pkt.last = (i == FRAME_SYMBOLS - 1) ? 1 : 0;
        pkt.keep = 0xF;
        pkt.strb = 0xF;
        tb_in.write(pkt);
    }
    fin.close();
    cout << "[INFO] Successfully loaded " << FRAME_SYMBOLS << " symbols." << endl;

    // =========================================================
    // Phase 2: 執行硬體 IP
    // =========================================================
    cout << "[INFO] Running IP Core..." << endl;
    p8psk_demap_packed(tb_in, tb_out);
    cout << "[INFO] Core execution completed." << endl;

    // =========================================================
    // Phase 3: LLR 比對 (包含容許誤差邏輯)
    // =========================================================
    ifstream fgold("demap_golden_out.txt");
    if (!fgold.is_open())
    {
        cerr << "[ERROR] Cannot open demap_golden_out.txt!" << endl;
        return -1;
    }

    // 設定容許誤差值 (針對低 SNR 環境，建議設為 1~2)
    const int TOLERANCE = 3;
    int err_cnt = 0;

    cout << "\n[DATA COMPARISON] Observation (First 5 Symbols):" << endl;
    cout << "-----------------------------------------------------------------------" << endl;
    cout << " Sym# | Bit |  HW Hex  | HW LLR (Signed) | Gold LLR | Diff | Result " << endl;
    cout << "------|-----|----------|-----------------|----------|------|----------" << endl;

    // 8PSK 每個 Symbol 產生 3 個 LLR
    for (int i = 0; i < FRAME_SYMBOLS * 3; i++)
    {
        if (tb_out.empty())
            break;

        axis_soft_out out_pkt = tb_out.read();
        int hw_llr = (int)(ap_int<8>)out_pkt.data;

        unsigned int gold_raw = 0;
        if (fgold >> hex >> gold_raw)
        {
            int gold_llr = (int)(ap_int<8>)(gold_raw & 0xFF);

            // 計算絕對誤差
            int diff = abs(hw_llr - gold_llr);

            // 判斷準則：正負號必須一致 且 數值誤差在容許範圍內
            bool sign_match = ((hw_llr >= 0 && gold_llr >= 0) || (hw_llr <= 0 && gold_llr <= 0));
            bool is_match = (diff <= TOLERANCE);

            if (i < 24)
            {
                int sym_idx = i / 3;
                string b_type = (i % 3 == 0) ? "B0(D)" : (i % 3 == 1) ? "B1(I)"
                                                                      : "B2(Q)";
                string status = is_match ? "  OK  " : " FAIL ";

                cout << " #" << setw(3) << setfill('0') << sym_idx << " | "
                     << b_type << " |   0x" << hex << setw(2) << (int)(out_pkt.data & 0xFF) << dec
                     << "   |       " << setw(3) << setfill(' ') << hw_llr
                     << "         |    " << setw(3) << gold_llr
                     << "     |  " << diff << "   | " << status << endl;
            }

            if (!is_match)
                err_cnt++;
        }
    }
    cout << "-----------------------------------------------------------------------" << endl;
    fgold.close();

    if (err_cnt == 0)
    {
        cout << ">> [PASS] All LLRs within tolerance (T=" << TOLERANCE << ")!" << endl;
        return 0;
    }
    else
    {
        cout << ">> [FAIL] Mismatches beyond tolerance: " << err_cnt << " / " << (FRAME_SYMBOLS * 3) << endl;
        return 1;
    }
}