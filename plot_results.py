#!/usr/bin/env python3
"""
plot_results.py - Generate comparison graphs from simulation results
CSD 204 - Operating Systems Project
Group 12: Mudit Ranjan, Mugdh Mittal, Aayush Trivedi

Reads results/results.csv and sensitivity_W.csv, produces:
  1. fault_rate_vs_frames.png     - Fault rate vs Frame size (synthetic traces, 2x2)
  2. financial1_comparison.png    - Financial1 real trace: all algorithms (FIXED)
  3. hit_rate_bar.png             - Hit rate bar chart at frame=64 (ALL 5 traces)
  4. tempograph_vs_lru.png        - TempoGraph fault-rate reduction vs LRU (all traces)
  5. sensitivity_window.png       - Fault rate vs W (window size) on scan trace
  6. execution_time.png           - Execution time comparison

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
import numpy as np

# ── Config ────────────────────────────────────────────────────────────
RESULTS_FILE     = "results/results.csv"
SENSITIVITY_FILE = "results/sensitivity_W.csv"
PLOTS_DIR        = "results/plots"
FRAME_SIZES      = [16, 32, 64, 128, 256]
ALGOS            = ["LRU", "LFU", "ARC", "TempoGraph", "SIEVE"]

SYNTH_TRACES = ["correlated", "uniform", "scan", "mixed"]
ALL_TRACES   = ["correlated", "uniform", "scan", "mixed", "Financial1"]

TRACE_LABELS = {
    "correlated": "Correlated",
    "uniform":    "Uniform",
    "scan":       "Scan (Cyclic)",
    "mixed":      "Mixed",
    "Financial1": "Financial1 (Real)",
}

COLORS = {
    "LRU":        "#4878CF",
    "LFU":        "#6ACC65",
    "ARC":        "#D65F5F",
    "TempoGraph": "#B47CC7",
    "SIEVE":      "#FF8C00",
}
MARKERS = {
    "LRU":        "o",
    "LFU":        "s",
    "ARC":        "^",
    "TempoGraph": "D",
    "SIEVE":      "P",
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
    found_traces = sorted(set(d["trace"] for d in data))
    print(f"Traces in CSV: {found_traces}")
except FileNotFoundError:
    print(f"ERROR: {RESULTS_FILE} not found. Run fix_csv.py first.")
    exit(1)

def get(trace, algo, frames, metric):
    for d in data:
        if d["trace"] == trace and d["algo"] == algo and d["frames"] == frames:
            return d[metric]
    return None


# ── Plot 1: Fault rate vs Frame size — Synthetic traces (2x2) ────────
fig, axes = plt.subplots(2, 2, figsize=(13, 9))
fig.suptitle("Page Fault Rate vs Frame Size — Synthetic Traces\n(Lower is Better)",
             fontsize=14, fontweight="bold")

for ax, trace in zip(axes.flat, SYNTH_TRACES):
    for algo in ALGOS:
        rates       = [get(trace, algo, f, "fault_rate") for f in FRAME_SIZES]
        frames_used = [f for f, r in zip(FRAME_SIZES, rates) if r is not None]
        rates_clean = [r for r in rates if r is not None]
        if not rates_clean:
            continue
        ax.plot(frames_used, rates_clean,
                label=algo, color=COLORS[algo],
                marker=MARKERS[algo], linewidth=2, markersize=7)

    ax.set_title(TRACE_LABELS[trace], fontweight="bold")
    ax.set_xlabel("Physical Frames")
    ax.set_ylabel("Page Fault Rate")
    ax.legend(fontsize=8)
    ax.grid(True, alpha=0.3)
    ax.yaxis.set_major_formatter(ticker.PercentFormatter(xmax=1))

plt.tight_layout()
plt.savefig(f"{PLOTS_DIR}/fault_rate_vs_frames.png", dpi=150, bbox_inches="tight")
plt.close()
print(f"Saved: {PLOTS_DIR}/fault_rate_vs_frames.png")


# ── Plot 2: Financial1 Real Trace — DEDICATED PLOT ───────────────────
fig, axes = plt.subplots(1, 2, figsize=(14, 5))
fig.suptitle("Financial1 Real Trace (5M Accesses — UMass Trace Repository)\n"
             "Random access pattern: TempoGraph cold-start disadvantage",
             fontsize=12, fontweight="bold")

# Left: without LFU (LFU is anomalously bad, distorts scale)
ax = axes[0]
for algo in ALGOS:
    if algo == "LFU":
        continue
    rates       = [get("Financial1", algo, f, "fault_rate") for f in FRAME_SIZES]
    frames_used = [f for f, r in zip(FRAME_SIZES, rates) if r is not None]
    rates_clean = [r for r in rates if r is not None]
    if not rates_clean:
        continue
    ax.plot(frames_used, rates_clean,
            label=algo, color=COLORS[algo],
            marker=MARKERS[algo], linewidth=2.5, markersize=8)

ax.set_title("LRU / ARC / SIEVE / TempoGraph\n(LFU excluded — anomalously high)",
             fontweight="bold")
ax.set_xlabel("Physical Frames")
ax.set_ylabel("Page Fault Rate")
ax.legend(fontsize=9)
ax.grid(True, alpha=0.3)
ax.yaxis.set_major_formatter(ticker.PercentFormatter(xmax=1))

# Annotate TempoGraph weakness
tg16 = get("Financial1", "TempoGraph", 16, "fault_rate")
if tg16 is not None:
    ax.annotate(
        "TempoGraph: high fault rate\nat small frames (cold start\n+ random-access pattern)",
        xy=(16, tg16), xytext=(40, tg16 - 0.05),
        arrowprops=dict(arrowstyle="->", color="purple", lw=1.5),
        fontsize=8, color="purple"
    )

# Right: all 5 algorithms
ax = axes[1]
for algo in ALGOS:
    rates       = [get("Financial1", algo, f, "fault_rate") for f in FRAME_SIZES]
    frames_used = [f for f, r in zip(FRAME_SIZES, rates) if r is not None]
    rates_clean = [r for r in rates if r is not None]
    if not rates_clean:
        continue
    ax.plot(frames_used, rates_clean,
            label=algo, color=COLORS[algo],
            marker=MARKERS[algo], linewidth=2, markersize=7)

ax.set_title("All 5 Algorithms (including LFU)", fontweight="bold")
ax.set_xlabel("Physical Frames")
ax.set_ylabel("Page Fault Rate")
ax.legend(fontsize=9)
ax.grid(True, alpha=0.3)
ax.yaxis.set_major_formatter(ticker.PercentFormatter(xmax=1))

plt.tight_layout()
plt.savefig(f"{PLOTS_DIR}/financial1_comparison.png", dpi=150, bbox_inches="tight")
plt.close()
print(f"Saved: {PLOTS_DIR}/financial1_comparison.png")


# ── Plot 3: Hit rate bar chart at frame=64 — ALL 5 traces ────────────
fig, ax = plt.subplots(figsize=(15, 6))
x         = np.arange(len(ALL_TRACES))
bar_width = 0.15
offsets   = [-2, -1, 0, 1, 2]

for i, algo in enumerate(ALGOS):
    hit_rates = [get(t, algo, 64, "hit_rate") or 0 for t in ALL_TRACES]
    ax.bar(x + offsets[i] * bar_width,
           hit_rates, bar_width,
           label=algo, color=COLORS[algo], edgecolor="white")

ax.set_title(
    "Cache Hit Rate at 64 Frames — All Traces (Higher is Better)\n"
    "★ = TempoGraph wins that trace  |  Financial1: classical algorithms win",
    fontsize=11, fontweight="bold"
)
ax.set_xticks(x)
ax.set_xticklabels([TRACE_LABELS[t] for t in ALL_TRACES], fontsize=10)
ax.set_ylabel("Hit Rate")
ax.yaxis.set_major_formatter(ticker.PercentFormatter(xmax=1))
ax.legend(fontsize=9)
ax.grid(True, axis="y", alpha=0.3)
ax.set_ylim(0, 1.05)

# Star annotations on TempoGraph bars where it wins
for i, trace in enumerate(ALL_TRACES):
    tg = get(trace, "TempoGraph", 64, "hit_rate")
    if tg is None:
        continue
    others = [get(trace, a, 64, "hit_rate") or 0 for a in ALGOS if a != "TempoGraph"]
    if all(tg >= o for o in others):
        ax.text(x[i] + offsets[3] * bar_width, tg + 0.012,
                "★", ha="center", fontsize=12, color="purple", fontweight="bold")

plt.tight_layout()
plt.savefig(f"{PLOTS_DIR}/hit_rate_bar.png", dpi=150, bbox_inches="tight")
plt.close()
print(f"Saved: {PLOTS_DIR}/hit_rate_bar.png")


# ── Plot 4: TempoGraph vs LRU improvement — ALL traces ───────────────
fig, ax = plt.subplots(figsize=(12, 5))

for trace in ALL_TRACES:
    improvements = []
    for f in FRAME_SIZES:
        lru_fr = get(trace, "LRU",        f, "fault_rate")
        tg_fr  = get(trace, "TempoGraph", f, "fault_rate")
        if lru_fr is not None and tg_fr is not None and lru_fr > 0:
            improvements.append((lru_fr - tg_fr) / lru_fr * 100)
        else:
            improvements.append(0)
    style = "--"  if trace == "Financial1" else "-"
    lw    = 1.5   if trace == "Financial1" else 2
    ax.plot(FRAME_SIZES, improvements,
            marker="o", linewidth=lw, markersize=7,
            linestyle=style, label=TRACE_LABELS[trace])

ax.axhline(0, color="black", linewidth=1.0, linestyle="--", alpha=0.6)
ax.set_title(
    "TempoGraph Fault Rate Reduction vs LRU (%)\n"
    "(Positive = TempoGraph Better  |  Financial1 dashed = TempoGraph loses vs LRU)",
    fontsize=12, fontweight="bold"
)
ax.set_xlabel("Physical Frames")
ax.set_ylabel("Fault Rate Reduction (%)")
ax.legend(fontsize=9)
ax.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig(f"{PLOTS_DIR}/tempograph_vs_lru.png", dpi=150, bbox_inches="tight")
plt.close()
print(f"Saved: {PLOTS_DIR}/tempograph_vs_lru.png")


# ── Plot 5: Window size sensitivity on scan trace ─────────────────────
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

    ax.set_title(
        "TempoGraph: Window Size W vs Fault Rate (Scan Trace)\n"
        "Sensitivity Analysis — W=10 is the default (dashed line)",
        fontsize=12, fontweight="bold"
    )
    ax.set_xlabel("Window Size W")
    ax.set_ylabel("Page Fault Rate")
    ax.yaxis.set_major_formatter(ticker.PercentFormatter(xmax=1))
    ax.legend(title="Frame Count", fontsize=8)
    ax.grid(True, alpha=0.3)
    ax.axvline(10, color="gray", linewidth=1.2, linestyle=":", alpha=0.8)
    ax.text(10.3, 0.98, "W=10\n(default)", fontsize=8, color="gray",
            va="top", transform=ax.get_xaxis_transform())

    plt.tight_layout()
    plt.savefig(f"{PLOTS_DIR}/sensitivity_window.png", dpi=150, bbox_inches="tight")
    plt.close()
    print(f"Saved: {PLOTS_DIR}/sensitivity_window.png")
except FileNotFoundError:
    print(f"INFO: {SENSITIVITY_FILE} not found — skipping sensitivity plot")


# ── Plot 6: Execution time comparison ────────────────────────────────
fig, axes = plt.subplots(1, 2, figsize=(14, 5))
fig.suptitle(
    "Simulation Execution Time (ms)\n"
    "TempoGraph overhead is simulation cost, not a fundamental limit of the algorithm",
    fontsize=11, fontweight="bold"
)

# Left: synthetic traces at 64 frames
ax = axes[0]
x_s = np.arange(len(SYNTH_TRACES))
for i, algo in enumerate(ALGOS):
    times = [get(t, algo, 64, "time_ms") or 0 for t in SYNTH_TRACES]
    ax.bar(x_s + (i - 2) * bar_width, times, bar_width,
           label=algo, color=COLORS[algo], edgecolor="white")
ax.set_title("Synthetic Traces at 64 Frames", fontweight="bold")
ax.set_xticks(x_s)
ax.set_xticklabels([TRACE_LABELS[t] for t in SYNTH_TRACES], fontsize=9)
ax.set_ylabel("Time (ms)")
ax.legend(fontsize=8)
ax.grid(True, axis="y", alpha=0.3)

# Right: Financial1 across frame sizes
ax = axes[1]
for algo in ALGOS:
    times       = [get("Financial1", algo, f, "time_ms") for f in FRAME_SIZES]
    frames_used = [f for f, t in zip(FRAME_SIZES, times) if t is not None]
    times_clean = [t for t in times if t is not None]
    if not times_clean:
        continue
    ax.plot(frames_used, times_clean,
            label=algo, color=COLORS[algo],
            marker=MARKERS[algo], linewidth=2, markersize=7)
ax.set_title("Financial1 Real Trace", fontweight="bold")
ax.set_xlabel("Physical Frames")
ax.set_ylabel("Time (ms)")
ax.legend(fontsize=8)
ax.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig(f"{PLOTS_DIR}/execution_time.png", dpi=150, bbox_inches="tight")
plt.close()
print(f"Saved: {PLOTS_DIR}/execution_time.png")


# ── Summary ───────────────────────────────────────────────────────────
print(f"\n{'='*60}")
print(f"All plots saved to: {PLOTS_DIR}/")
print(f"{'='*60}")
print("  1. fault_rate_vs_frames.png    — synthetic traces (2x2 grid)")
print("  2. financial1_comparison.png   — Financial1 real trace [NEW]")
print("  3. hit_rate_bar.png            — ALL 5 traces at 64 frames")
print("  4. tempograph_vs_lru.png       — improvement vs LRU (all traces)")
print("  5. sensitivity_window.png      — window size W sensitivity")
print("  6. execution_time.png          — timing overhead comparison")