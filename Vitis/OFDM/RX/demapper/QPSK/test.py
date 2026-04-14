import numpy as np
import math

def generate_qpsk_adaptive_data(snr_db=6.0, num_symbols=1632):
    in_file = "demap_in_symbols.txt"
    out_file = "demap_golden_out.txt"
    
    # -----------------------------------------------------
    # 1. 核心常數 (強制使用 float32)
    # -----------------------------------------------------
    QPSK_REF_VAL = np.float32(0.70710678)
    DIST_QPSK = np.float32(1.41421356)
    MAX_CSI_FACTOR = np.float32(128.0) 
    
    qpsk_points = [
        (QPSK_REF_VAL, QPSK_REF_VAL), (-QPSK_REF_VAL, QPSK_REF_VAL),
        (QPSK_REF_VAL, -QPSK_REF_VAL), (-QPSK_REF_VAL, -QPSK_REF_VAL)
    ]
    
    snr_linear = 10**(snr_db / 10.0)
    sigma = math.sqrt(1.0 / (2.0 * snr_linear))

    rx_samples = []
    for i in range(num_symbols):
        # 使用固定種子確保每次產生的雜訊一致，方便除錯
        ideal_I, ideal_Q = qpsk_points[i % 4]
        # 強制轉成 float16 再轉回 float32，模擬硬體從 I/Q Buffer 讀出的精度
        rx_I = np.float32(np.float16(ideal_I + np.random.normal(0, sigma)))
        rx_Q = np.float32(np.float16(ideal_Q + np.random.normal(0, sigma)))
        rx_samples.append((rx_I, rx_Q))

    # -----------------------------------------------------
    # 2. [關鍵修正] 模擬硬體 Phase 3: 4路 Partial Sum
    # -----------------------------------------------------
    p_sum_err = np.zeros(4, dtype=np.float32)
    p_sum_sig = np.zeros(4, dtype=np.float32)

    for i, (rx_I, rx_Q) in enumerate(rx_samples):
        ref_I = QPSK_REF_VAL if rx_I > 0 else -QPSK_REF_VAL
        ref_Q = QPSK_REF_VAL if rx_Q > 0 else -QPSK_REF_VAL
        
        err_sq = (rx_I - ref_I)**2 + (rx_Q - ref_Q)**2
        sig_sq = ref_I**2 + ref_Q**2
        
        # 模擬 HLS: idx = i & 0x3
        idx = i & 0x3
        p_sum_err[idx] += err_sq
        p_sum_sig[idx] += sig_sq
    
    # 總結 Partial Sum
    sum_err = np.float32(sum(p_sum_err))
    sum_sig = np.float32(sum(p_sum_sig))
    
    noise_pwr = max(sum_err, np.float32(1e-6))
    calculated_csi = (np.float32(2.0) * DIST_QPSK) * (sum_sig / noise_pwr)
    csi_factor = min(calculated_csi, MAX_CSI_FACTOR)
    
    print(f"[PYTHON] Calculated CSI: {calculated_csi:.6f}, Factor: {csi_factor:.6f}")

    # -----------------------------------------------------
    # 3. 輸出測資 (對齊 quantize_llr 與 0.01 偏移)
    # -----------------------------------------------------
    with open(in_file, 'w') as f_in, open(out_file, 'w') as f_out:
        for rx_I, rx_Q in rx_samples:
            # 寫入輸入檔
            I_raw = np.float16(rx_I).view(np.uint16)
            Q_raw = np.float16(rx_Q).view(np.uint16)
            f_in.write(f"{(int(Q_raw) << 16) | int(I_raw):08X}\n")
            
            # 生成 Golden LLR
            for val in [rx_I, rx_Q]:
                raw_llr = np.float32(val * csi_factor)
                # 模擬 C++: (val >= 0) ? (val + 0.01f) : (val - 0.01f)
                rounded = (raw_llr + 0.01) if raw_llr >= 0 else (raw_llr - 0.01)
                
                # 飽和與向零取整
                val_clamp = max(-7.0, min(7.0, rounded))
                llr_int = int(val_clamp)
                f_out.write(f"{(llr_int & 0xFF):02X}\n")

if __name__ == "__main__":
    # 使用 6.0 dB 進行嚴格驗證
    generate_qpsk_adaptive_data(snr_db=3.0)