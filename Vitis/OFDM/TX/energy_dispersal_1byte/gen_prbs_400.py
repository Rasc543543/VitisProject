#!/usr/bin/env python3
"""
Generate PRBS table for Energy Dispersal IP
Output: C header format for HLS inclusion

Usage:
    python gen_prbs_400.py

Generates:
    - prbs_table_400.h (C header for HLS)
    - prbs_table_400.txt (Verilog $readmemh format)
    - prbs_golden_400.py (Python validation)
"""

def generate_prbs_extended(size=400):
    """
    Generate extended PRBS sequence
    
    Polynomial: x^15 + x^14 + 1
    Initial State: 0xA9 (10101001)
    
    Args:
        size: Number of bytes to generate
    
    Returns:
        List of bytes (0-255)
    """
    d_reg = 0xa9  # Initial state: 10101001
    prbs = []
    
    for i in range(size):
        res = 0
        for j in range(8):
            # Feedback polynomial: x^15 + x^14 + 1
            feedback = ((d_reg >> 13) ^ (d_reg >> 14)) & 0x1
            d_reg = ((d_reg << 1) | feedback) & 0x7fff
            res = (res << 1) | feedback
        prbs.append(res & 0xFF)
    
    return prbs


def write_c_header(filename, prbs, size):
    """
    Write PRBS as C header for HLS inclusion
    
    Format:
        0x03, 0xF6, 0x08, 0x34, ...
    """
    with open(filename, 'w') as f:
        f.write(f"/* Auto-generated PRBS table ({size} bytes) */\n")
        f.write(f"/* Polynomial: x^15 + x^14 + 1 */\n")
        f.write(f"/* Initial State: 0xA9 */\n\n")
        
        for i in range(0, size, 16):
            # Write 16 bytes per line
            line_bytes = prbs[i:min(i+16, size)]
            hex_str = ', '.join(f'0x{b:02X}' for b in line_bytes)
            
            # Add comma at end of line (except last line)
            if i + 16 < size:
                f.write(f'    {hex_str},\n')
            else:
                f.write(f'    {hex_str}\n')
    
    print(f"✓ Generated {filename} ({size} bytes)")


def write_verilog_readmemh(filename, prbs, size):
    """
    Write PRBS as Verilog $readmemh format (for testbench)
    
    Format (one byte per line):
        03
        F6
        08
        ...
    """
    with open(filename, 'w') as f:
        f.write(f"// PRBS table for Verilog testbench ({size} bytes)\n")
        f.write(f"// Usage: $readmemh(\"prbs_table_400.txt\", prbs_rom);\n")
        
        for byte in prbs:
            f.write(f'{byte:02X}\n')
    
    print(f"✓ Generated {filename} ({size} bytes)")


def write_python_array(filename, prbs, size):
    """
    Write PRBS as Python array (for validation scripts)
    """
    with open(filename, 'w') as f:
        f.write(f"# Auto-generated PRBS table ({size} bytes)\n")
        f.write(f"# Polynomial: x^15 + x^14 + 1\n")
        f.write(f"# Initial State: 0xA9\n\n")
        f.write(f"prbs_golden = [\n")
        
        for i in range(0, size, 16):
            line_bytes = prbs[i:min(i+16, size)]
            hex_str = ', '.join(f'0x{b:02X}' for b in line_bytes)
            f.write(f'    {hex_str},\n')
        
        f.write(f"]\n")
    
    print(f"✓ Generated {filename} (Python format)")


def verify_prbs(prbs, size):
    """
    Verify PRBS generation against known values
    
    First 16 bytes should be:
    0x03, 0xF6, 0x08, 0x34, 0x30, 0xB8, 0xA3, 0x93,
    0xC9, 0x68, 0xB7, 0x73, 0xB3, 0x29, 0xAA, 0xF5
    """
    expected_first_16 = [
        0x03, 0xF6, 0x08, 0x34, 0x30, 0xB8, 0xA3, 0x93,
        0xC9, 0x68, 0xB7, 0x73, 0xB3, 0x29, 0xAA, 0xF5
    ]
    
    if prbs[:16] == expected_first_16:
        print("✓ PRBS verification PASSED (first 16 bytes match)")
        return True
    else:
        print("✗ PRBS verification FAILED!")
        print(f"Expected: {[f'0x{b:02X}' for b in expected_first_16]}")
        print(f"Got:      {[f'0x{b:02X}' for b in prbs[:16]]}")
        return False


if __name__ == "__main__":
    import sys
    
    # Parse command line arguments
    size = 400  # Default size changed to 400
    if len(sys.argv) > 1:
        size = int(sys.argv[1])
        print(f"Generating PRBS table with {size} bytes...")
    else:
        print(f"Generating PRBS table with default size ({size} bytes)")
    
    # Generate PRBS
    prbs = generate_prbs_extended(size)
    
    # Verify first 16 bytes
    verify_prbs(prbs, size)
    
    # Write files
    write_c_header("prbs_table_400.h", prbs, size)
    write_verilog_readmemh("prbs_table_400.txt", prbs, size)
    write_python_array("prbs_golden_400.py", prbs, size)
    
    # Summary
    print(f"\n{'='*60}")
    print(f"PRBS Generation Complete")
    print(f"{'='*60}")
    print(f"Total bytes:    {size}")
    print(f"Polynomial:     x^15 + x^14 + 1")
    print(f"Initial state:  0xA9")
    print(f"\nGenerated files:")
    print(f"  1. prbs_table_400.h       - HLS C header")
    print(f"  2. prbs_table_400.txt     - Verilog $readmemh")
    print(f"  3. prbs_golden_400.py     - Python validation")
    print(f"{'='*60}")