#include "interleaver.h"

//=============================================================================
// === 1. 全局常數 (Compile-Time Constants) ===
const int DATA_LEN = 204;
const int COL_NUM = 16;
const int ROW_NUM = 12;             // (204 / 16)
const int RESIDUE = 12;             // (204 % 16)
const int TOTAL_ROWS = ROW_NUM + 1; // 13

void tx_interleaver(
    hls::stream<axis_word> &input_stream,
    hls::stream<axis_word> &output_stream)
{
// 介面 pragma
#pragma HLS INTERFACE axis port = input_stream
#pragma HLS INTERFACE axis port = output_stream
#pragma HLS INTERFACE ap_ctrl_none port = return

    // 緩衝區宣告
    // 我們的 buffer 必須儲存 13 列 (rows 0-12) 和 16 欄 (cols 0-15)
    unsigned char buffer[TOTAL_ROWS][COL_NUM]; // 大小: [13][16]

// 關鍵的陣列分割 pragma
// 這會將 buffer[13][16] 分割為 16 個獨立的 RAM (BRAMs)
// 每個 RAM 的大小為 [13] (儲存一整欄)
#pragma HLS ARRAY_PARTITION variable = buffer complete dim = 2

    // 宣告在迴圈外的變數
    axis_word out_word;
    int k = 0;

    //
    // === 階段 1: 儲存 (Fill Phase) ===
    // (逐行 Row 寫入)
    //
READ_LOOP:
    for (int i = 0; i < DATA_LEN; i++)
    {
// 讀取迴圈管線化 (Initiation Interval = 1)
#pragma HLS PIPELINE II = 1

        // 計算 (r, c) 座標
        int r = i / COL_NUM; // Row index
        int c = i % COL_NUM; // Column index

        // 讀取並儲存
        axis_word in_word = input_stream.read();
        buffer[r][c] = in_word.data;
    }

    //
    // === 階段 2: 轉發 (Drain Phase) ===
    // (逐欄 Column 讀出)
    //

    // 重置 k 計數器 (用於 TLAST)
    k = 0;

    // 外層迴圈：遍歷 16 欄 (Columns 0 to 15)
WRITE_COL_LOOP:
    for (int c = 0; c < COL_NUM; c++)
    {

        // HLS 在編譯時會解譯這個常數
        // (c < 12) ? 13 : 12
        int rows_in_this_col = (c < RESIDUE) ? (TOTAL_ROWS) : (ROW_NUM);

        // 內層迴圈：讀取該欄中的所有資料
    WRITE_ROW_LOOP:
        for (int r = 0; r < rows_in_this_col; r++)
        {
#pragma HLS PIPELINE II = 1

            int current_col = c;
            int current_row = r;

            // 1. 填入資料
            out_word.data = buffer[current_row][current_col];

            // 2. [關鍵修正] 必須初始化 Side-Channels
            // -1 (或 0xFF) 代表所有 Byte 均有效
            out_word.keep = -1;
            out_word.strb = -1;

            // 3. 處理 TLAST
            out_word.last = (k == DATA_LEN - 1) ? 1 : 0;

            output_stream.write(out_word);
            k++;
        }
    }
}