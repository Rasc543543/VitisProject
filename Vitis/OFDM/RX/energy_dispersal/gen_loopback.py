#!/usr/bin/env python3
"""
Generate Test Vectors for Loopback Test (Scrambler + Descrambler)
Logic: 
  Input = Raw Data
  Expected Output = Raw Data (Because Scramble + Descramble = Identity)
"""

def main():
    # 定義封包 (與之前一致)
    packets_config = [
        (188, 0), # Packet 0: Ramp
        (282, 1), # Packet 1: 0xAA
        (376, 2)  # Packet 2: 0xFF
    ]
    
    raw_data = [] 
    
    print("Generating Loopback Test Vectors (Raw Data)...")
    
    for pkt_idx, (length, p_type) in enumerate(packets_config):
        print(f"  Pkt {pkt_idx}: Len={length}, Type={p_type}")
        
        for i in range(length):
            # 生成原始資料
            if p_type == 0:
                val = i & 0xFF
            elif p_type == 1:
                val = 0xAA
            else:
                val = 0xFF
            
            raw_data.append(val)

    # 寫入檔案
    # 對於 Loopback 測試，Input 和 Expected Output 是同一個檔案
    with open('tb_data_loopback.txt', 'w') as f:
        for i in range(0, len(raw_data), 16):
            chunk = raw_data[i:i+16]
            f.write(' '.join(f'{b:02X}' for b in chunk) + '\n')

    print(f"\nDone! Total {len(raw_data)} bytes.")
    print("File created: tb_data_loopback.txt")

if __name__ == '__main__':
    main()