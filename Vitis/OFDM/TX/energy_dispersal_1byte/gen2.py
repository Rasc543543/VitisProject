#!/usr/bin/env python3
"""
Generate Variable Length Multi-Packet Test Vectors (Fixed PRBS Size)
Scenario:
  Packet 0: 188 bytes (Ramp)
  Packet 1: 282 bytes (0xAA)
  Packet 2: 376 bytes (0xFF)
Total: 846 bytes

NOTE: MAX_PRBS_SIZE set to 400 to match HLS definition.
"""

# 修正為 400，與 HLS Header 一致
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
    # 1. 準備 Golden PRBS (長度 400)
    # HLS 硬體會只儲存這 400 個值，並在每個封包開始時重置索引
    prbs_golden = generate_prbs_golden(MAX_PRBS_SIZE)
    
    # 2. 定義封包結構
    # (Length, PatternType: 0=Ramp, 1=0xAA, 2=0xFF)
    # 注意：所有 Length 必須 <= MAX_PRBS_SIZE (400)
    packets_config = [
        (188, 0),
        (282, 1),
        (376, 2)
    ]
    
    all_input = []
    all_output = []
    
    print(f"Generating Variable Length Packets (PRBS Limit: {MAX_PRBS_SIZE})...")
    
    for pkt_idx, (length, p_type) in enumerate(packets_config):
        print(f"  Pkt {pkt_idx}: Len={length}, Type={p_type}")
        
        if length > MAX_PRBS_SIZE:
             print(f"Error: Packet length {length} exceeds PRBS size {MAX_PRBS_SIZE}!")
             return

        for i in range(length):
            # 生成 Input Byte
            if p_type == 0:
                val = i & 0xFF
            elif p_type == 1:
                val = 0xAA
            else:
                val = 0xFF
                
            all_input.append(val)
            
            # 計算 Output Byte
            # 關鍵：使用 prbs_golden[i]
            # 因為 HLS 設計是在 TLAST 後重置 index，所以每個封包都對應 PRBS[0]~PRBS[Len-1]
            scrambled = val ^ prbs_golden[i]
            all_output.append(scrambled)

    # 3. 寫入檔案
    with open('tb_input_var.txt', 'w') as f:
        for i in range(0, len(all_input), 16):
            chunk = all_input[i:i+16]
            f.write(' '.join(f'{b:02X}' for b in chunk) + '\n')
            
    with open('tb_output_var.txt', 'w') as f:
        for i in range(0, len(all_output), 16):
            chunk = all_output[i:i+16]
            f.write(' '.join(f'{b:02X}' for b in chunk) + '\n')

    print(f"\nDone! Total {len(all_input)} bytes.")
    print("Files created: tb_input_var.txt, tb_output_var.txt")

if __name__ == '__main__':
    main()