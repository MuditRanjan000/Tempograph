#!/usr/bin/env python3
"""
plot_results.py - Generate comparison graphs from simulation results
CSD 204 - Operating Systems Project

Reads results/results.csv and produces:
  1. Fault rate vs Frame size (one plot per trace type)
  2. Hit rate comparison bar chart (all algorithms, frame=64)
  3. Algorithm overhead (time_ms) comparison

Usage:
    python3 plot_results.py

Output: results/plots/ directory with PNG files
"""

import csv
import os
import collections
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

# ── Config ───────────────────────────────────────────────────────────
RESULTS_FILE = "results/results.csv"
PLOTS_DIR    = "results/plots"
FRAME_SIZES  = [16, 32, 64, 128, 256]
ALGOS        = ["LRU", "LFU", "ARC", "TempoGraph"]
TRACE_TYPES  = ["correlated", "uniform", "scan", "mixed"]

COLORS = {
    "LRU":        "#4878CF",
    "LFU":        "#6ACC65",
    "ARC":        "#D65F5F",
    "TempoGraph": "#B47CC7",
}
MARKERS = {"LRU": "o", "LFU": "s", "ARC": "^", "TempoGraph": "D"}

os.makedirs(PLOTS_DIR, exist_ok=True)

# ── Load data ────────────────────────────────────────────────────────
data = []
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

def get(trace, algo, frames, metric):
    for d in data:
        if d["trace"] == trace and d["algo"] == algo and d["frames"] == frames:
            return d[metric]
    return None

# ── Plot 1: Fault rate vs Frame size (one subplot per trace type) ────
fig, axes = plt.subplots(2, 2, figsize=(12, 9))
fig.suptitle("Page Fault Rate vs Frame Size\n(Lower is Better)",
             fontsize=14, fontweight="bold")

for ax, trace in zip(axes.flat, TRACE_TYPES):
    for algo in ALGOS:
        rates = [get(trace, algo, f, "fault_rate") for f in FRAME_SIZES]
        rates = [r for r in rates if r is not None]
        frames_used = [f for f, r in zip(FRAME_SIZES,
                        [get(trace, algo, f, "fault_rate") for f in FRAME_SIZES])
                       if r is not None]
        ax.plot(frames_used, rates,
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
fig, ax = plt.subplots(figsize=(10, 5))
x = range(len(TRACE_TYPES))
bar_width = 0.18
offsets = [-1.5, -0.5, 0.5, 1.5]

for i, algo in enumerate(ALGOS):
    hit_rates = [get(t, algo, 64, "hit_rate") or 0 for t in TRACE_TYPES]
    bars = ax.bar([xi + offsets[i] * bar_width for xi in x],
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

# ── Plot 3: TempoGraph vs LRU improvement ───────────────────────────
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

print("\nAll plots saved to results/plots/")
print("Use these in your final IEEE report.")
