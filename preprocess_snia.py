#!/usr/bin/env python3
import sys
import os

def preprocess_trace(input_path, output_path, page_size=4096):
    if not os.path.exists(input_path):
        print(f"ERROR: Input file '{input_path}' not found.")
        return False

    processed = 0
    with open(input_path, 'r') as infile, open(output_path, 'w') as outfile:
        for line_num, line in enumerate(infile, 1):
            line = line.strip()
            if not line or line.startswith('#'):
                continue

            parts = line.split(',')
            if len(parts) < 5:  # Changed to ensure we have at least 5 columns
                continue

            try:
                # SNIA Format: Timestamp, Host, Workload, Type, Offset(4), Size(5)
                addr_str = parts[4].strip() 
                if addr_str.startswith('0x'):
                    address = int(addr_str, 16)
                else:
                    address = int(addr_str)

                page_id = address // page_size
                outfile.write(f"{page_id}\n")
                processed += 1
            except ValueError as e:
                continue

    print(f"Processed {processed} accesses from {input_path} -> {output_path}")
    return True

def main():
    if len(sys.argv) < 3:
        print("Usage: python preprocess_snia.py <input.csv> <output.txt>")
        sys.exit(1)
    
    preprocess_trace(sys.argv[1], sys.argv[2])

if __name__ == "__main__":
    main()