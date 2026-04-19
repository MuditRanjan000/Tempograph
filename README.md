# TempoGraph

TempoGraph is a novel cache replacement policy simulator based on directed weighted graphs. It predicts future accesses by tracking co-occurring pages in a sliding window, allowing it to excel in access patterns with strong sequential or correlated structures (like scans).

This project includes a fully functional cache simulator written in C, along with evaluation and plotting scripts in Python. It compares TempoGraph against standard replacement policies: **LRU**, **LFU**, **ARC**, and **SIEVE**.

## Features

- **TempoGraph Policy**: Uses an adjacency list to track page access relationships and predict what will be accessed next. O(1) eviction time by sampling from the LRU tail.
- **Classic Policies**: Implements LRU, LFU, ARC, and SIEVE for baseline comparisons.
- **Trace Generator**: Includes a Python script to generate synthetic traces (scan, correlated, mixed, uniform).
- **Visualization Suite**: Automatically parses experiment results and generates detailed performance plots.

## Prerequisites

- **GCC (MinGW for Windows)**: Required to compile the C simulator.
- **Python 3.x**: Required for the evaluation scripts.
- **Python Packages**: `matplotlib`, `numpy` (install via `pip install matplotlib numpy`).

## Compilation

To compile the C simulator, use the provided `Makefile` or run the following command directly:

```cmd
gcc -O2 -std=c99 -o tempograph.exe main.c lru.c lfu.c arc.c tempograph.c sieve.c -lm
```

This will produce `tempograph.exe` (or `tempograph` on Linux/macOS).

## Usage

You can run the simulator directly on any provided trace file.

**Basic Usage:**
```cmd
tempograph.exe -a <algorithm> -f <frames> <trace_file>
```

**Run all algorithms on a specific trace (e.g., scan trace at 64 frames):**
```cmd
tempograph.exe -a all -f 64 traces\scan.txt
```

**Run TempoGraph with a specific sliding window size (e.g., W=10):**
```cmd
tempograph.exe -a tempograph -f 64 -w 10 traces\scan.txt
```

## Running the Full Experiment Suite

To regenerate all results and plots across different traces, algorithms, and cache sizes:

1. **Run Experiments**: This will execute all simulations and create `results.csv`.
   ```cmd
   python fix_csv.py
   ```
2. **Generate Plots**: This reads the CSV and produces multiple PNG charts in the `results/plots/` folder.
   ```cmd
   python plot_results.py
   ```

## Trace Generation

If you need to generate new synthetic traces:

```cmd
python gen_trace.py 100000 255 --type scan --out traces\scan.txt
python gen_trace.py 100000 1000 --type correlated --out traces\correlated.txt
python gen_trace.py 100000 1000 --type mixed --out traces\mixed.txt
```

*(Note: Real-world traces like `Financial1.txt` are included directly in the traces directory).*

## Academic Context

This simulator was developed as part of CSD 204 (OS Project), Group 12. TempoGraph leverages graph-based working set estimation to identify cyclical patterns, significantly outperforming LRU/LFU on pure scan workloads.
