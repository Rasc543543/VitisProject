import numpy as np

def verify_rtl_output(golden_file, rtl_file):
    # 讀取數據
    with open(golden_file, 'r') as f:
        golden = [int(line.strip(), 16) for line in f]
    with open(rtl_file, 'r') as f:
        rtl = [int(line.strip(), 16) for line in f]
    
    # 轉為有號數 (8-bit signed)
    golden = np.array([val if val < 128 else val - 256 for val in golden])
    rtl = np.array([val if val < 128 else val - 256 for val in rtl])

    # 1. 直接比對
    matches = np.sum(golden == rtl)
    print(f"Direct Match: {matches}/{len(golden)}")

    # 2. 嘗試循環位移比對 (考慮 1-Symbol = 2 LLRs)
    # 將 RTL 序列向右循環位移 2 個位置 (即 1 個 Symbol)
    rtl_shifted = np.roll(rtl, 2)
    shift_matches = np.sum(golden == rtl_shifted)
    print(f"Shifted Match (1 Sym): {shift_matches}/{len(golden)}")

if __name__ == "__main__":
    verify_rtl_output("demap_golden_out.txt", "rtl_out_llr.txt")