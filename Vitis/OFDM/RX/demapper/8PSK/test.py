import numpy as np
import math

def generate_8psk_strict_lut_data(snr_db=3.0, num_symbols=1632):
    in_file = "demap_in_symbols.txt"
    out_file = "demap_golden_out.txt"
    
    # -----------------------------------------------------
    # 1. 核心常數 (精確對齊 8psk_final_table.png)
    # -----------------------------------------------------
    C1 = np.float32(0.92387953)  # I(000) or Q(001)
    S1 = np.float32(0.38268343)  # Q(000) or I(001)
    SQRT2_INV = np.float32(0.7071)
    DIST_8PSK = np.float32(0.76537)
    MAX_CSI_FACTOR = np.float32(128.0)
    
    # 依照圖表 LUT 嚴格定義映射 (鍵值順序為 b2, b1, b0)
    mapping = {
        (0,0,0): complex( C1,  S1), #  22.5 deg
        (0,0,1): complex( S1,  C1), #  67.5 deg
        (0,1,0): complex(-C1,  S1), # 157.5 deg
        (0,1,1): complex(-S1,  C1), # 112.5 deg
        (1,0,0): complex( C1, -S1), # 337.5 deg
        (1,0,1): complex( S1, -C1), # 292.5 deg
        (1,1,0): complex(-C1, -S1), # 202.5 deg
        (1,1,1): complex(-S1, -C1)  # 247.5 deg
    }
    
    snr_linear = 10**(snr_db / 10.0)
    sigma = math.sqrt(1.0 / (2.0 * snr_linear))

    rx_samples = []
    for i in range(num_symbols):
        # 修正位元提取：確保 i=1 產生 (0, 0, 1)
        b2 = (i >> 2) & 1
        b1 = (i >> 1) & 1
        b0 = (i >> 0) & 1
        
        bits_tuple = (b2, b1, b0)
        ideal = mapping[bits_tuple]
        
        rx_I = np.float32(np.float16(ideal.real + np.random.normal(0, sigma)))
        rx_Q = np.float32(np.float16(ideal.imag + np.random.normal(0, sigma)))
        rx_samples.append((rx_I, rx_Q))

    # -----------------------------------------------------
    # 2. 模擬硬體統計 (Phase 2)
    # -----------------------------------------------------
    p_sum_err = np.zeros(4, dtype=np.float32)
    p_sum_sig = np.zeros(4, dtype=np.float32)

    for i, (rx_I, rx_Q) in enumerate(rx_samples):
        # 硬體內部判定邏輯
        avI, avQ = abs(rx_I), abs(rx_Q)
        sel_I = C1 if avQ < avI else S1
        sel_Q = S1 if avQ < avI else C1
        ref_I = sel_I if rx_I > 0 else -sel_I
        ref_Q = sel_Q if rx_Q > 0 else -sel_Q
        
        err_sq = np.float32((rx_I - ref_I)**2 + (rx_Q - ref_Q)**2)
        sig_sq = np.float32(ref_I**2 + ref_Q**2)
        
        idx = i & 0x3
        p_sum_err[idx] += err_sq
        p_sum_sig[idx] += sig_sq
    
    sum_err = np.float32(sum(p_sum_err))
    sum_sig = np.float32(sum(p_sum_sig))
    csi_factor = min((np.float32(4.0) * DIST_8PSK) * (sum_sig / max(sum_err, 1e-6)), MAX_CSI_FACTOR)
    
    print(f"--- Updated Bit-True Report (SNR={snr_db}dB) ---")
    print(f"  P_sig (Theory 1632): {sum_sig:.4f}")
    print(f"  CSI Factor: {csi_factor:.6f}")

    # -----------------------------------------------------
    # 3. 輸出測資
    # -----------------------------------------------------
    with open(in_file, 'w') as f_in, open(out_file, 'w') as f_out:
        for rx_I, rx_Q in rx_samples:
            # Packed Hex 輸出
            I_raw = np.float16(rx_I).view(np.uint16)
            Q_raw = np.float16(rx_Q).view(np.uint16)
            f_in.write(f"{(int(Q_raw) << 16) | int(I_raw):08X}\n")
            
            # LLR 映射：對齊 Planar Projection 邏輯
            # Bit 0(D): (|I|-|Q|)  Bit 1(I): I  Bit 2(Q): Q
            m0 = (abs(rx_I) - abs(rx_Q)) * SQRT2_INV
            for m in [m0, rx_I, rx_Q]:
                raw_llr = np.float32(m * csi_factor)
                rounded = (raw_llr + 0.01) if raw_llr >= 0 else (raw_llr - 0.01)
                f_out.write(f"{(int(np.clip(rounded, -7.0, 7.0)) & 0xFF):02X}\n")

if __name__ == "__main__":
    generate_8psk_strict_lut_data(snr_db=100.0)
    print(">> Corrected 8PSK test data (Matching 8psk_final_table.png) generated.")