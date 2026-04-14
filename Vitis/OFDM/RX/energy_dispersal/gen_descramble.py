#!/usr/bin/env python3
"""
Generate Test Vectors for Energy Descrambler
Logic: 
  1. Generate Raw Data (Ramp/0xAA/0xFF)
  2. Scramble it (XOR with PRBS)
  3. Save Scrambled -> Input File
  4. Save Raw -> Output File (Expected)
"""

MAX_PRBS_SIZE = 400 

def generate_prbs_golden(length):
    prbs = []
    d_reg = 0xa9
    for i in range(length):
        res = 0
        for j in range(8):
            feedback = ((d_reg >> 13) ^ (d_reg >> 14)) & 0x1
            d_reg = ((d_reg << 1) | feedback) & 0x7fff
            res = (res << 1) | feedback
        prbs.append(res & 0xFF)
    return prbs

def main():
    prbs_golden = generate_prbs_golden(MAX_PRBS_SIZE)
    
    # 定義封包 (長度必須 <= 400)
    packets_config = [
        (188, 0), # Packet 0: Ramp
        (282, 1), # Packet 1: 0xAA
        (376, 2)  # Packet 2: 0xFF
    ]
    
    scrambled_input = []  # 這是要給 DUT 的輸入
    raw_expected = []     # 這是 DUT 應該輸出的結果
    
    print("Generating Descrambler Test Vectors...")
    
    for pkt_idx, (length, p_type) in enumerate(packets_config):
        print(f"  Pkt {pkt_idx}: Len={length}, Type={p_type}")
        
        for i in range(length):
            # 1. 生成原始資料 (Raw Data)
            if p_type == 0:
                raw_val = i & 0xFF
            elif p_type == 1:
                raw_val = 0xAA
            else:
                raw_val = 0xFF
            
            # 2. 進行擾碼 (XOR)
            # 因為 DUT 是解碼器，我們必須餵給它亂碼
            scrambled_val = raw_val ^ prbs_golden[i]
            
            scrambled_input.append(scrambled_val)
            raw_expected.append(raw_val)

    # 3. 寫入檔案 (命名區分開來)
    with open('tb_input_descramble.txt', 'w') as f:
        for i in range(0, len(scrambled_input), 16):
            chunk = scrambled_input[i:i+16]
            f.write(' '.join(f'{b:02X}' for b in chunk) + '\n')
            
    with open('tb_output_descramble.txt', 'w') as f:
        for i in range(0, len(raw_expected), 16):
            chunk = raw_expected[i:i+16]
            f.write(' '.join(f'{b:02X}' for b in chunk) + '\n')

    print(f"\nDone! Total {len(scrambled_input)} bytes.")
    print("Files created: tb_input_descramble.txt, tb_output_descramble.txt")

if __name__ == '__main__':
    main()