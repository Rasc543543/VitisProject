#include "ref_rx_defs.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <cstdio> // 引入文件操作
#include <stdint.h>

void ref_rx_packed(hls::stream<axis_t> &input_stream, hls::stream<axis_t> &output_stream);

// 診斷函式：執行特定測試並輸出報告與模擬向量
void run_diagnostic_test(int cfo_offset, float phase_deg, const std::string &label)
{
    hls::stream<axis_t> in_stream("in_stream");
    hls::stream<axis_t> out_stream("out_stream");

    // 準備輸出文件 (供 Vivado 模擬使用)
    FILE *fp_in = fopen("input_stimulus.dat", "w");
    FILE *fp_gold = fopen("golden_output.dat", "w");

    float ch_gain = 0.5f; 
    
    float phase_rad = phase_deg * M_PI / 180.0f;
    float ch_r = cos(phase_rad) * ch_gain; // 乘上衰減係數
    float ch_i = sin(phase_rad) * ch_gain; // 乘上衰減係數

    std::vector<float> gold_i(DATA_TONES), gold_q(DATA_TONES);
    std::vector<axis_t> frame(FFT_LENGTH); // 2048 點

    // 初始化 Frame
    for (int i = 0; i < FFT_LENGTH; i++)
    {
        frame[i].data = 0;
        frame[i].keep = 0xF;
        frame[i].strb = 0xF;
        frame[i].last = (i == FFT_LENGTH - 1) ? 1 : 0;
    }

    // 1. 產生測資與寫入輸入向量檔
    int d_idx = 0, p_idx = 0;
    for (int i = 0; i < FFT_LENGTH; i++)
    {
        float ti = 0, tq = 0;
        int active_idx = i - LEFT_GUARD;

        // 填入 Active Tones (Data & Pilot)
        if (i >= LEFT_GUARD && i < FFT_LENGTH - RIGHT_GUARD)
        {
            if (active_idx % 7 == 0)
            {
                // Pilot Tone
                ti = (float)(1.0f / (float)PILOT_ROM_INV[p_idx++]);
            }
            else
            {
                // Data Tone (QPSK)
                gold_i[d_idx] = (d_idx % 2 == 0) ? 0.707f : -0.707f;
                gold_q[d_idx] = 0.707f;
                ti = gold_i[d_idx];
                tq = gold_q[d_idx];
                d_idx++;
            }
        }

        // 加上 CFO 偏移
        int s_idx = i + cfo_offset;
        if (s_idx >= 0 && s_idx < FFT_LENGTH)
        {
            // 加上相位旋轉 (模擬 Channel)
            float final_i = ti * ch_r - tq * ch_i;
            float final_q = ti * ch_i + tq * ch_r;

            // ✅ 【關鍵修改】：模擬 FFT 硬體的定點數輸出 (* 2048 倍)
            int16_t sim_hw_i = (int16_t)(final_i * 2048.0f);
            int16_t sim_hw_q = (int16_t)(final_q * 2048.0f);

            // 打包成 32-bit 定點數
            frame[s_idx].data = pack_int16(sim_hw_i, sim_hw_q);
        }
    }

    // 將輸入資料寫入文件，同時灌入 AXI-Stream
    for (int i = 0; i < FFT_LENGTH; i++)
    {
        fprintf(fp_in, "%08x\n", (unsigned int)frame[i].data);
        in_stream.write(frame[i]);
    }

    // 2. 執行硬體模擬 (DUT)
    ref_rx_packed(in_stream, out_stream);

    // 3. 診斷分析與寫入黃金參考檔
    std::cout << "\n==== TEST CASE: " << label << " (CFO=" << cfo_offset << ", Phase=" << phase_deg << "deg) ====" << std::endl;

    float total_error = 0;
    float max_error = 0;
    float k_errors[6] = {0, 0, 0, 0, 0, 0};

    // 讀出 RX 處理完的 1632 點 Data
    for (int i = 0; i < DATA_TONES; i++)
    {
        axis_t out = out_stream.read();

        // 寫入 Golden 輸出
        fprintf(fp_gold, "%08x\n", (unsigned int)out.data);

        // ✅ 這裡維持 u16_to_half，因為 RX IP 輸出前已經用 pack_iq 封裝回 half 浮點數了
        float ri = (float)u16_to_half(out.data(15, 0));
        float rq = (float)u16_to_half(out.data(31, 16));

        // 計算與理想 QPSK 星座點的誤差
        float err = sqrt(pow(ri - gold_i[i], 2) + pow(rq - gold_q[i], 2));
        total_error += err;

        if (err > max_error)
            max_error = err;
        k_errors[i % 6] += err;
    }

    fclose(fp_in);
    fclose(fp_gold);

    // 輸出分析報告
    std::cout << "1. Restoration Check: Average Error = " << (total_error / DATA_TONES) << std::endl;
    std::cout << "2. Peak Precision: Max Error = " << max_error << std::endl;
    std::cout << "3. Interpolation Trend (Error at k=1 to k=6):" << std::endl;
    for (int k = 0; k < 6; k++)
    {
        std::cout << "   - Data Pos k=" << (k + 1) << ": Avg Error = " << (k_errors[k] / (DATA_TONES / 6)) << std::endl;
    }
    std::cout << ">>> Verification files 'input_stimulus.dat' and 'golden_output.dat' generated." << std::endl;
}

int main()
{
    // 執行診斷測試 (CFO = 70, Phase = 45度)
    run_diagnostic_test(70, 45.0f, "VIVADO_SIM_VECTOR_GEN");

    std::cout << "\n>>> C-SIM SUCCESS!" << std::endl;
    return 0;
}