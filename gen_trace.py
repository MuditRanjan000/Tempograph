#!/usr/bin/env python3
"""
gen_trace.py - Synthetic memory trace generator
CSD 204 - Operating Systems Project

Generates traces with realistic correlated access patterns.

Usage:
    python3 gen_trace.py <n_accesses> <n_pages> [--type <type>]

Types:
    correlated  (default) - clustered access groups (best for TempoGraph)
    uniform     - fully random (baseline, no correlation)
    scan        - cyclic sequential scan (LRU worst case)
    mixed       - mix of all three
"""

import random
import sys

def gen_correlated(n, n_pages, cluster_size=10, jump_prob=0.1):
    """
    Clustered access: stay in a group of cluster_size pages, occasionally
    jump to a new group. Creates strong inter-page correlations.
    """
    pages = list(range(n_pages))
    cluster_start = 0
    for _ in range(n):
        if random.random() < jump_prob:
            cluster_start = random.randint(0, n_pages - cluster_size)
        page = random.randint(cluster_start,
                              min(cluster_start + cluster_size - 1, n_pages - 1))
        print(page)

def gen_uniform(n, n_pages):
    """Fully random - no correlation."""
    for _ in range(n):
        print(random.randint(0, n_pages - 1))

def gen_scan(n, n_pages):
    """Cyclic sequential scan - LRU worst case."""
    for i in range(n):
        print(i % n_pages)

def gen_mixed(n, n_pages):
    """Equal mix of correlated, uniform, and scan workloads."""
    third = n // 3
    gen_correlated(third, n_pages)
    gen_uniform(third, n_pages)
    gen_scan(n - 2 * third, n_pages)

if __name__ == "__main__":
    n        = int(sys.argv[1]) if len(sys.argv) > 1 else 10000
    n_pages  = int(sys.argv[2]) if len(sys.argv) > 2 else 500
    trace_type = "correlated"
    out_file   = None
    for i, arg in enumerate(sys.argv):
        if arg == "--type" and i + 1 < len(sys.argv):
            trace_type = sys.argv[i + 1]
        if arg == "--out" and i + 1 < len(sys.argv):
            out_file = sys.argv[i + 1]

    random.seed(42)

    import sys as _sys
    if out_file:
        import io
        _sys.stdout = io.TextIOWrapper(open(out_file, 'wb'), encoding='ascii', newline='\n')

    if   trace_type == "correlated": gen_correlated(n, n_pages)
    elif trace_type == "uniform":    gen_uniform(n, n_pages)
    elif trace_type == "scan":       gen_scan(n, n_pages)
    elif trace_type == "mixed":      gen_mixed(n, n_pages)
