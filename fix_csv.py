#!/usr/bin/env python3
"""
fix_csv.py - Fixes the malformed results.csv and regenerates plots
Run: python fix_csv.py
"""
import subprocess, os, sys

exe = ".\\tempograph.exe"
traces = ["correlated","uniform","scan","mixed"]
frames = [16,32,64,128,256]
algos  = ["lru","lfu","arc","tempograph"]

os.makedirs("results", exist_ok=True)
os.makedirs("results\\plots", exist_ok=True)

rows = ["trace_type,algorithm,frames,accesses,faults,fault_rate,hit_rate,time_ms"]

for trace in traces:
    print(f"Running trace: {trace}")
    for f in frames:
        for algo in algos:
            cmd = [exe, "-a", algo, "-f", str(f)]
            if algo == "tempograph":
                cmd += ["-w", "10", "-d", "0.98"]
            cmd.append(f"traces\\{trace}.txt")
            try:
                out = subprocess.check_output(cmd, stderr=subprocess.DEVNULL).decode()
                # grab last non-empty line (skip header)
                lines = [l.strip() for l in out.strip().splitlines() if l.strip()]
                data_line = lines[-1]
                if data_line.startswith("algorithm"):
                    continue  # skip header lines
                rows.append(f"{trace},{data_line}")
                print(f"  {algo} f={f} done")
            except Exception as e:
                print(f"  ERROR: {algo} f={f}: {e}")

with open("results\\results.csv","w") as f:
    f.write("\n".join(rows))

print("\nCSV fixed! Saved to results\\results.csv")
print("Now run: python plot_results.py")