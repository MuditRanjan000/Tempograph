#!/usr/bin/env python3
"""
preprocess_trace.py - Convert UMass trace files to page-number format
CSD 204 - Operating Systems Project

Converts UMass trace files (with memory addresses) to the format expected
by tempograph: one page number per line.

Usage:
    python preprocess_trace.py <input_trace> <output_file> [--page-size <size>]

Arguments:
    input_trace  - Path to UMass trace file (e.g., from trace.cs.umass.edu)
    output_file  - Path to write processed trace (one page ID per line)
    --page-size  - Page size in bytes (default: 4096)

UMass trace format assumed:
    Lines with 3+ comma-separated fields: timestamp,address,size,...
    Address is in hex or decimal, size is ignored.
    Lines starting with '#' or blank are skipped.
"""

import sys
import os

def preprocess_trace(input_path, output_path, page_size=4096):
    """Convert address trace to page-number trace."""
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
            if len(parts) < 2:
                print(f"WARNING: Line {line_num} has <2 fields, skipping: {line}")
                continue

            try:
                # Assume address is in second field (index 1)
                addr_str = parts[1].strip()
                if addr_str.startswith('0x'):
                    address = int(addr_str, 16)
                else:
                    address = int(addr_str)

                page_id = address // page_size
                outfile.write(f"{page_id}\n")
                processed += 1
            except ValueError as e:
                print(f"WARNING: Invalid address on line {line_num}: {parts[1]} ({e})")
                continue

    print(f"Processed {processed} accesses from {input_path} -> {output_path}")
    return True

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    input_trace = sys.argv[1]
    output_file = sys.argv[2]
    page_size = 4096

    # Parse optional --page-size
    if len(sys.argv) > 3 and sys.argv[3] == '--page-size':
        try:
            page_size = int(sys.argv[4])
        except (IndexError, ValueError):
            print("ERROR: Invalid page size.")
            sys.exit(1)

    if not preprocess_trace(input_trace, output_file, page_size):
        sys.exit(1)

if __name__ == "__main__":
    main()