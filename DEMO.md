# TempoGraph — Complete Demo Guide
## CSD 204, Group 12 | Windows + VSCode

---

## PRE-DEMO CHECKLIST (do this before the viva)

### Required tools
| Tool | Download | Verify |
|------|----------|--------|
| MinGW (GCC for Windows) | https://winlibs.com → download ZIP, extract to `C:\mingw64` | `gcc --version` in CMD |
| Python 3.x | https://python.org | `python --version` in CMD |
| matplotlib + numpy | `pip install matplotlib numpy` | `python -c "import matplotlib"` |

### Add GCC to PATH (if not already)
1. Open **Start → Edit the system environment variables**
2. Under **User variables**, click `Path` → **Edit** → **New**
3. Add: `C:\mingw64\bin`
4. Click OK, **restart CMD**

---

## STEP 1 — Open the Project

```
Open VSCode → File → Open Folder → select TempoGraph_Code folder
```

Open a **Command Prompt** (not PowerShell) inside VSCode:
- Terminal → New Terminal → click the dropdown arrow → **Command Prompt**

---

## STEP 2 — Compile the Code

**Paste this single line into Command Prompt:**

```cmd
gcc -O2 -std=c99 -o tempograph.exe main.c lru.c lfu.c arc.c tempograph.c sieve.c -lm
```

Expected output: **no errors**, and `tempograph.exe` appears in the folder.

If you get "gcc not recognized": check PATH step above.

---

## STEP 3 — Quick Sanity Test (show this first in the demo)

Run all algorithms on the **scan trace** at 64 frames:

```cmd
tempograph.exe -a all -f 64 traces\scan.txt
```

Expected output:
```
algorithm,frames,accesses,faults,fault_rate,hit_rate,time_ms
LRU,64,100000,100000,1.000000,0.000000,...
LFU,64,100000,100000,1.000000,0.000000,...
ARC,64,100000,100000,1.000000,0.000000,...
TempoGraph,64,100000,~77992,~0.779920,~0.220080,...
SIEVE,64,100000,100000,1.000000,0.000000,...
```

**Key talking point:** LRU/LFU/ARC/SIEVE all get 100% page faults on a cyclic scan.
TempoGraph gets only ~78% — it learns the graph structure and retains pages that
will be accessed again soon.

---

## STEP 4 — Run Other Traces Individually

### Mixed trace (TempoGraph wins):
```cmd
tempograph.exe -a all -f 64 traces\mixed.txt
```

### Financial1 real trace (TempoGraph loses):
```cmd
tempograph.exe -a all -f 64 traces\Financial1.txt
```
**Key talking point:** TempoGraph struggles on random real-world access patterns.
Cold start + no co-access structure means the graph edges never become useful.
ARC and LRU win here because the working set is small and fits in cache.

### Correlated trace:
```cmd
tempograph.exe -a all -f 128 traces\correlated.txt
```

---

## STEP 5 — Demonstrate Window Size Effect (Sensitivity Analysis)

Show that W=10 is the sweet spot:

```cmd
tempograph.exe -a tempograph -f 64 -w 5  traces\scan.txt
tempograph.exe -a tempograph -f 64 -w 10 traces\scan.txt
tempograph.exe -a tempograph -f 64 -w 20 traces\scan.txt
tempograph.exe -a tempograph -f 64 -w 25 traces\scan.txt
```

Fault rate should decrease from W=5 → W=10, then stabilize.

---

## STEP 6 — Run All Experiments (generates results.csv)

**This takes ~10–15 minutes due to Financial1 (5M accesses). Run it before your demo.**

```cmd
python fix_csv.py
```

Output: `results\results.csv` with 125 rows (5 algorithms × 5 frame sizes × 5 traces).

---

## STEP 7 — Generate All Plots

```cmd
python plot_results.py
```

This generates **6 plots** in `results\plots\`:

| File | What it shows |
|------|--------------|
| `fault_rate_vs_frames.png` | Synthetic traces 2×2 grid |
| `financial1_comparison.png` | Financial1 dedicated plot (**new**) |
| `hit_rate_bar.png` | All 5 traces at 64 frames, bar chart |
| `tempograph_vs_lru.png` | % improvement of TempoGraph over LRU |
| `sensitivity_window.png` | Window size W vs fault rate |
| `execution_time.png` | Timing overhead comparison |

Open them: **File Explorer → results → plots** → double-click any PNG.

---

## STEP 8 — Regenerate Traces (only if needed)

If trace files are missing or you want fresh ones:

```cmd
python gen_trace.py 100000 255  --type scan       --out traces\scan.txt
python gen_trace.py 100000 1000 --type correlated --out traces\correlated.txt
python gen_trace.py 100000 1000 --type mixed      --out traces\mixed.txt
python gen_trace.py 100000 1000 --type uniform    --out traces\uniform.txt
```

Note: `mixed` = 75% correlated + 25% uniform random (no scan component).
Financial1.txt is a real trace — do not regenerate, keep original.

---

## STEP 9 — Git Push (push both branches)

```cmd
git add .
git commit -m "Fix Financial1 plots, fix mixed trace gen, add execution_time plot"
git push origin main
git push origin libcachesim-benchmark
```

---

## VIVA QUICK-FIRE ANSWERS

**Q: What is TempoGraph in one sentence?**
A: A directed weighted graph tracks which pages co-occur in a sliding window;
   on eviction, we keep pages predicted by recent access patterns.

**Q: Why does it beat LRU on the scan trace?**
A: LRU evicts the oldest page — on a cyclic scan every page gets evicted before
   it loops back, giving 100% faults. TempoGraph learns that page 254→0 is a
   strong edge (scan always wraps around), so it retains page 0 and evicts 254.
   Result: ~78% fault rate at 64 frames vs 100% for LRU.

**Q: What is O(1) eviction?**
A: We sample SAMPLE_SIZE=64 candidates from the LRU tail and score each in
   O(W×E) = O(64×64) = O(4096) — a constant independent of cache size N.

**Q: Why MRU tiebreaking instead of LRU?**
A: During cold start the graph is empty and all scores are zero. LRU tiebreaking
   evicts the oldest page — but that's the page we want to keep (it cycles back
   sooner). MRU tiebreaking evicts the page just used (won't loop back yet).
   Changing this gives 100% fault rate on scan.

**Q: Why does TempoGraph lose on Financial1?**
A: Two reasons. (1) Cold start: the graph takes time to learn, and 5M accesses
   on a large working set means the cold period hurts. (2) Random access pattern:
   Financial1 has no sequential or correlated structure, so graph edges carry
   no useful predictive signal. ARC's adaptive policy handles random workloads better.

**Q: RAM vs Cache for the data structure?**
A: Adjacency list uses MAX_UNIQUE × MAX_EDGES × 4 bytes ≈ 4096 × 64 × 4 = 1MB.
   Fits in L2/L3 CPU cache. Old dense matrix design was 32MB (DRAM only).

**Q: Difference vs SIEVE?**
A: SIEVE uses one visited bit per page and a FIFO hand — simple and fast, but
   treats all cached pages equally. TempoGraph uses full directed graph relationships,
   letting it predict future accesses. SIEVE beats ARC on real random traces;
   TempoGraph beats SIEVE on correlated/scan/mixed.

**Q: Difference vs GRUMA [7]?**
A: GRUMA requires offline GRU training on historical traces. TempoGraph is fully
   online, deterministic, and requires no training data.

---

## DEMO SCRIPT (2-minute version for viva panel)

1. "Here is the compiled simulator. Let me run it on a scan trace."
   → `tempograph.exe -a all -f 64 traces\scan.txt`
   → Point out: "LRU, ARC, SIEVE all 100% faults. TempoGraph: 78%."

2. "The algorithm builds a directed graph. Page 254→0 gets a high weight because
   scan always wraps. So we evict 254, keep 0."

3. "On Financial1, a real random-access trace, TempoGraph loses."
   → Open `financial1_comparison.png`
   → "Cold start + no graph structure. ARC wins here."

4. "This is the sensitivity analysis — W=10 is the sweet spot."
   → Open `sensitivity_window.png`

5. "All results are reproducible. Run fix_csv.py and plot_results.py."
