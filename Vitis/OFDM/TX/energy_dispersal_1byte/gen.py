#!/usr/bin/env python3
"""
Generate multi-packet test vectors for Energy Dispersal

Packet 0: Ramp pattern (0x00, 0x01, ..., 0xFF, 0x00, ...)
Packet 1: 0xAA pattern (all 188 bytes = 0xAA)
Packet 2: 0xFF pattern (all 188 bytes = 0xFF)

Each packet: 188 bytes input → 188 bytes output (XOR PRBS)
"""

def generate_prbs():
    """Generate DVB-T2 PRBS sequence (188 bytes)"""
    prbs = []
    d_reg = 0xa9  # Initial state
    
    for i in range(188):
        res = 0
        for j in range(8):
            feedback = ((d_reg >> 13) ^ (d_reg >> 14)) & 0x1
            d_reg = ((d_reg << 1) | feedback) & 0x7fff
            res = (res << 1) | feedback
        prbs.append(res & 0xFF)
    
    return prbs

def generate_packet(pattern_type):
    """
    Generate input packet based on pattern type
    pattern_type: 'ramp', '0xAA', '0xFF'
    """
    if pattern_type == 'ramp':
        return [(i & 0xFF) for i in range(188)]
    elif pattern_type == '0xAA':
        return [0xAA] * 188
    elif pattern_type == '0xFF':
        return [0xFF] * 188
    else:
        raise ValueError(f"Unknown pattern: {pattern_type}")

def main():
    # Generate PRBS once (188 bytes)
    prbs = generate_prbs()
    
    # Generate 3 packets
    packets = [
        ('ramp', 'Packet 0: Ramp Pattern'),
        ('0xAA', 'Packet 1: 0xAA Pattern'),
        ('0xFF', 'Packet 2: 0xFF Pattern')
    ]
    
    all_input = []
    all_output = []
    
    print("=" * 60)
    print("Multi-Packet Test Vector Generator")
    print("=" * 60)
    print("IMPORTANT: Each packet uses THE SAME PRBS[0-187]")
    print("           (matches HLS fixed ROM design)")
    print("=" * 60)
    
    for idx, (pattern, desc) in enumerate(packets):
        print(f"\n{desc}")
        print("-" * 60)
        
        # Generate input
        input_data = generate_packet(pattern)
        
        # Calculate output using THE SAME PRBS for each packet
        # This matches the HLS design which uses a fixed 188-byte ROM
        output_data = [(input_data[i] ^ prbs[i]) for i in range(188)]
        
        # Accumulate
        all_input.extend(input_data)
        all_output.extend(output_data)
        
        # Display first 16 bytes
        print(f"Input  (first 16): {' '.join(f'{b:02X}' for b in input_data[:16])}")
        print(f"PRBS   (first 16): {' '.join(f'{b:02X}' for b in prbs[:16])}")
        print(f"Output (first 16): {' '.join(f'{b:02X}' for b in output_data[:16])}")
        print(f"Note: Packet {idx} uses PRBS[0-187] (same as all packets)")
    
    # Write to files (3 packets × 188 bytes = 564 bytes total)
    print(f"\n{'=' * 60}")
    print(f"Writing test vectors...")
    print(f"{'=' * 60}")
    
    # Input file
    with open('tb_input_3pkt.txt', 'w') as f:
        for i in range(0, len(all_input), 16):
            line = ' '.join(f'{b:02X}' for b in all_input[i:i+16])
            f.write(line + '\n')
    
    # Output file
    with open('tb_output_3pkt.txt', 'w') as f:
        for i in range(0, len(all_output), 16):
            line = ' '.join(f'{b:02X}' for b in all_output[i:i+16])
            f.write(line + '\n')
    
    print(f"✅ tb_input_3pkt.txt:  {len(all_input)} bytes (3 packets)")
    print(f"✅ tb_output_3pkt.txt: {len(all_output)} bytes (3 packets)")
    
    # Summary
    print(f"\n{'=' * 60}")
    print("Summary:")
    print(f"{'=' * 60}")
    print(f"Total packets: 3")
    print(f"Bytes per packet: 188")
    print(f"Total bytes: {len(all_input)}")
    print(f"\nPacket breakdown:")
    for idx, (pattern, desc) in enumerate(packets):
        start = idx * 188
        end = start + 188
        print(f"  Packet {idx}: bytes {start:3d}-{end:3d} ({desc})")

if __name__ == '__main__':
    main()