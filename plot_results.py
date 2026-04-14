#!/usr/bin/env python3
"""
plot_results.py - Generate comparison graphs from simulation results
CSD 204 - Operating Systems Project
Group 12: Mudit Ranjan, Mugdh Mittal, Aayush Trivedi

Reads results/results.csv and sensitivity_W.csv, produces:
  1. fault_rate_vs_frames.png    - Fault rate vs Frame size (4 trace types)
  2. hit_rate_bar.png            - Hit rate bar chart at frame=64
  3. tempograph_vs_lru.png       - TempoGraph improvement over LRU
  4. sensitivity_window.png      - Fault rate vs W (window size) on scan trace

Usage:
    python plot_results.py

Output: results/plots/ directory
"""

import csv
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

# ── Config ────────────────────────────────────────────────────────────
RESULTS_FILE    = "results/results.csv"
SENSITIVITY_FILE = "results/sensitivity_W.csv"
PLOTS_DIR       = "results/plots"
FRAME_SIZES     = [16, 32, 64, 128, 256]
ALGOS           = ["LRU", "LFU", "ARC", "TempoGraph", "SIEVE"]
TRACE_TYPES     = ["correlated", "uniform", "scan", "mixed"]

COLORS = {
    "LRU":        "#4878CF",
    "LFU":        "#6ACC65",
    "ARC":        "#D65F5F",
    "TempoGraph": "#B47CC7",
    "SIEVE":      "#FF8C00",   # orange — clearly distinct from others
}
MARKERS = {
    "LRU":        "o",
    "LFU":        "s",
    "ARC":        "^",
    "TempoGraph": "D",
    "SIEVE":      "P",         # plus/star marker
}

os.makedirs(PLOTS_DIR, exist_ok=True)

# ── Load main data ────────────────────────────────────────────────────
data = []
try:
    with open(RESULTS_FILE) as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append({
                "trace":      row["trace_type"],
                "algo":       row["algorithm"],
                "frames":     int(row["frames"]),
                "fault_rate": float(row["fault_rate"]),
                "hit_rate":   float(row["hit_rate"]),
                "time_ms":    float(row["time_ms"]),
            })
    print(f"Loaded {len(data)} rows from {RESULTS_FILE}")
except FileNotFoundError:
    print(f"ERROR: {RESULTS_FILE} not found. Run fix_csv.py first.")
    exit(1)

def get(trace, algo, frames, metric):
    for d in data:
        if d["trace"] == trace and d["algo"] == algo and d["frames"] == frames:
            return d[metric]
    return None

# ── Plot 1: Fault rate vs Frame size (2x2 subplots) ──────────────────
fig, axes = plt.subplots(2, 2, figsize=(13, 9))
fig.suptitle("Page Fault Rate vs Frame Size\n(Lower is Better)",
             fontsize=14, fontweight="bold")

for ax, trace in zip(axes.flat, TRACE_TYPES):
    for algo in ALGOS:
        rates        = [get(trace, algo, f, "fault_rate") for f in FRAME_SIZES]
        frames_used  = [f for f, r in zip(FRAME_SIZES, rates) if r is not None]
        rates_clean  = [r for r in rates if r is not None]
        if not rates_clean:
            continue
        ax.plot(frames_used, rates_clean,
                label=algo, color=COLORS[algo],
                marker=MARKERS[algo], linewidth=2, markersize=7)

    ax.set_title(f"Trace: {trace.capitalize()}", fontweight="bold")
    ax.set_xlabel("Physical Frames")
    ax.set_ylabel("Page Fault Rate")
    ax.legend(fontsize=8)
    ax.grid(True, alpha=0.3)
    ax.yaxis.set_major_formatter(ticker.PercentFormatter(xmax=1))

plt.tight_layout()
plt.savefig(f"{PLOTS_DIR}/fault_rate_vs_frames.png", dpi=150, bbox_inches="tight")
plt.close()
print(f"Saved: {PLOTS_DIR}/fault_rate_vs_frames.png")

# ── Plot 2: Hit rate bar chart at frame=64 ───────────────────────────
fig, ax = plt.subplots(figsize=(12, 5))
x         = range(len(TRACE_TYPES))
bar_width = 0.15
offsets   = [-2, -1, 0, 1, 2]   # 5 algorithms

for i, algo in enumerate(ALGOS):
    hit_rates = [get(t, algo, 64, "hit_rate") or 0 for t in TRACE_TYPES]
    ax.bar([xi + offsets[i] * bar_width for xi in x],
           hit_rates, bar_width,
           label=algo, color=COLORS[algo], edgecolor="white")

ax.set_title("Cache Hit Rate at 64 Frames (Higher is Better)",
             fontsize=13, fontweight="bold")
ax.set_xticks(list(x))
ax.set_xticklabels([t.capitalize() for t in TRACE_TYPES])
ax.set_ylabel("Hit Rate")
ax.yaxis.set_major_formatter(ticker.PercentFormatter(xmax=1))
ax.legend()
ax.grid(True, axis="y", alpha=0.3)
ax.set_ylim(0, 1.0)

plt.tight_layout()
plt.savefig(f"{PLOTS_DIR}/hit_rate_bar.png", dpi=150, bbox_inches="tight")
plt.close()
print(f"Saved: {PLOTS_DIR}/hit_rate_bar.png")

# ── Plot 3: TempoGraph vs LRU improvement ────────────────────────────
fig, ax = plt.subplots(figsize=(10, 5))

for trace in TRACE_TYPES:
    improvements = []
    for f in FRAME_SIZES:
        lru_fr = get(trace, "LRU",        f, "fault_rate")
        tg_fr  = get(trace, "TempoGraph", f, "fault_rate")
        if lru_fr and tg_fr and lru_fr > 0:
            improvements.append((lru_fr - tg_fr) / lru_fr * 100)
        else:
            improvements.append(0)
    ax.plot(FRAME_SIZES, improvements, marker="o", linewidth=2, markersize=7,
            label=trace.capitalize())

ax.axhline(0, color="black", linewidth=0.8, linestyle="--")
ax.set_title("TempoGraph Fault Rate Reduction vs LRU (%)\n(Positive = TempoGraph is Better)",
             fontsize=12, fontweight="bold")
ax.set_xlabel("Physical Frames")
ax.set_ylabel("Fault Rate Reduction (%)")
ax.legend()
ax.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig(f"{PLOTS_DIR}/tempograph_vs_lru.png", dpi=150, bbox_inches="tight")
plt.close()
print(f"Saved: {PLOTS_DIR}/tempograph_vs_lru.png")

# ── Plot 4: Window size sensitivity on scan trace ─────────────────────
try:
    sens_data = []
    with open(SENSITIVITY_FILE) as f:
        reader = csv.DictReader(f)
        for row in reader:
            sens_data.append({
                "W":          int(row["window_size"]),
                "frames":     int(row["frames"]),
                "fault_rate": float(row["fault_rate"]),
            })

    fig, ax = plt.subplots(figsize=(9, 5))
    window_sizes = sorted(set(d["W"] for d in sens_data))

    for f in FRAME_SIZES:
        rates = []
        for W in window_sizes:
            match = [d["fault_rate"] for d in sens_data if d["W"] == W and d["frames"] == f]
            rates.append(match[0] if match else None)
        rates_clean = [r for r in rates if r is not None]
        W_clean     = [W for W, r in zip(window_sizes, rates) if r is not None]
        ax.plot(W_clean, rates_clean, marker="o", linewidth=2, markersize=6,
                label=f"{f} frames")

    ax.set_title("TempoGraph: Window Size W vs Fault Rate (Scan Trace)\n(Sensitivity Analysis)",
                 fontsize=12, fontweight="bold")
    ax.set_xlabel("Window Size W")
    ax.set_ylabel("Page Fault Rate")
    ax.yaxis.set_major_formatter(ticker.PercentFormatter(xmax=1))
    ax.legend(title="Frame Count", fontsize=8)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(f"{PLOTS_DIR}/sensitivity_window.png", dpi=150, bbox_inches="tight")
    plt.close()
    print(f"Saved: {PLOTS_DIR}/sensitivity_window.png")
except FileNotFoundError:
    print(f"INFO: {SENSITIVITY_FILE} not found — skipping sensitivity plot")

print(f"\nAll plots saved to {PLOTS_DIR}/")
print("Use these PNG files in your final IEEE report.")