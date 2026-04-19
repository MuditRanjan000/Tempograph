#!/usr/bin/env python3
"""
fix_csv.py - Run ALL experiments and generate results.csv
CSD 204 - Operating Systems Project
Group 12: Mudit Ranjan, Mugdh Mittal, Aayush Trivedi

Runs every algorithm on every trace at every frame size.
Also runs TempoGraph window-size sensitivity analysis (W=5..25).
Takes 5-10 minutes total.

Usage:
    python fix_csv.py

Output:
    results/results.csv          - main results (all algorithms)
    results/sensitivity_W.csv    - window size sensitivity (TempoGraph only)
"""

import subprocess
import os
import sys
import glob

# ── Config ────────────────────────────────────────────────────────────
exe = "tempograph.exe"
TRACE_EXTENSIONS = ("*.txt", "*.csv")
trace_paths = sorted(
    p for ext in TRACE_EXTENSIONS for p in glob.glob(os.path.join("traces", ext))
)
spc_files = sorted(glob.glob(os.path.join("traces", "*.spc")))
traces = [os.path.basename(p) for p in trace_paths]
frames = [16, 32, 64, 128, 256]
algos  = ["lru", "lfu", "arc", "tempograph", "sieve"]

# Sensitivity analysis: vary W for TempoGraph on scan trace
window_sizes = [5, 10, 15, 20, 25, 30]

# ── Pre-flight checks ─────────────────────────────────────────────────
if not os.path.exists(exe):
    print(f"ERROR: Executable '{exe}' not found. Run 'make' first.")
    sys.exit(1)

if spc_files:
    print("NOTE: .spc files are not directly supported by the simulator.")
    print("      Convert them with preprocess_trace.py to a .txt file first.")

if not traces:
    print("ERROR: No .txt or .csv trace files found in 'traces/' directory.")
    sys.exit(1)

os.makedirs("results",        exist_ok=True)
os.makedirs("results/plots", exist_ok=True)
print(f"Detected trace files: {', '.join(traces)}")

# ── Helper ────────────────────────────────────────────────────────────
def run_cmd(cmd):
    """Run command, return last data line of CSV output or None on error."""
    try:
        out = subprocess.check_output(cmd, stderr=subprocess.DEVNULL).decode()
        lines = [l.strip() for l in out.strip().splitlines() if l.strip()]
        for line in reversed(lines):
            if not line.startswith("algorithm"):
                return line
    except Exception as e:
        print(f"  ERROR: {' '.join(cmd)}: {e}")
    return None

print(f"Found {len(traces)} trace files: {', '.join(traces)}")
print(f"Running {len(algos)} algorithms across {len(frames)} frame sizes...")
print("=" * 60)

rows = ["trace_type,algorithm,frames,accesses,faults,fault_rate,hit_rate,time_ms"]

for trace_file in traces:
    trace_path = os.path.join("traces", trace_file)
    trace_type = os.path.splitext(trace_file)[0]
    if not os.path.exists(trace_path):
        print(f"WARNING: trace file not found: {trace_path} — skipping")
        continue
    print(f"\nTrace: {trace_type} ({trace_file})")
    for f in frames:
        for algo in algos:
            cmd = [exe, "-a", algo, "-f", str(f)]
            if algo == "tempograph":
                cmd += ["-w", "10", "-d", "0.98"]
            cmd.append(trace_path)
            line = run_cmd(cmd)
            if line:
                rows.append(f"{trace_type},{line}")
                print(f"  {algo:<12} f={f:<4} done")

with open("results/results.csv", "w") as f:
    f.write("\n".join(rows))

print(f"\nMain CSV saved: results\\results.csv  ({len(rows)-1} rows)")

# ── Sensitivity analysis: W size on scan trace ────────────────────────
print("\n" + "=" * 60)
print("Sensitivity Analysis: Window Size W (TempoGraph on scan)")
print("=" * 60)

sens_rows = ["window_size,frames,accesses,faults,fault_rate,hit_rate,time_ms"]
scan_path = "traces/scan.txt"

if os.path.exists(scan_path):
    for W in window_sizes:
        for f in frames:
            cmd = [exe, "-a", "tempograph", "-f", str(f), "-w", str(W), "-d", "0.98", scan_path]
            line = run_cmd(cmd)
            if line:
                sens_rows.append(f"{W},{line.split(',', 1)[1]}")   # drop algo name column
                print(f"  W={W:<3} f={f:<4} done")

    with open("results/sensitivity_W.csv", "w") as f:
        f.write("\n".join(sens_rows))
    print(f"\nSensitivity CSV saved: results\\sensitivity_W.csv")
else:
    print(f"WARNING: {scan_path} not found — skipping sensitivity analysis")

print("\nDone! Run 'python plot_results.py' to generate plots.")