#!/bin/bash
# run_experiments.sh - Run all experiments and collect results
# CSD 204 - Operating Systems Project
#
# Runs all 4 algorithms across 5 frame sizes on 4 trace types.
# Saves results to results/results.csv for plotting.
#
# Usage: bash run_experiments.sh

set -e

BINARY="./tempograph"
RESULTS_DIR="results"
RESULTS_FILE="$RESULTS_DIR/results.csv"
TRACES_DIR="traces"

# Frame sizes to test
FRAMES=(16 32 64 128 256)

# TempoGraph parameters
WINDOW=10
DECAY=0.98

# ── Check binary exists ──────────────────────────────────────────────
if [ ! -f "$BINARY" ]; then
    echo "Binary not found. Run 'make' first."
    exit 1
fi

# ── Create directories ───────────────────────────────────────────────
mkdir -p "$RESULTS_DIR" "$TRACES_DIR"

# ── Generate synthetic traces ────────────────────────────────────────
echo "Generating synthetic traces..."
python3 gen_trace.py 100000 1000 --type correlated > "$TRACES_DIR/correlated.txt"
python3 gen_trace.py 100000 1000 --type uniform    > "$TRACES_DIR/uniform.txt"
python3 gen_trace.py 100000 1000 --type scan       > "$TRACES_DIR/scan.txt"
python3 gen_trace.py 100000 1000 --type mixed      > "$TRACES_DIR/mixed.txt"
echo "Traces ready."

# ── CSV header ───────────────────────────────────────────────────────
echo "trace_type,algorithm,frames,accesses,faults,fault_rate,hit_rate,time_ms" \
     > "$RESULTS_FILE"

# ── Run all combinations ─────────────────────────────────────────────
for TRACE_TYPE in correlated uniform scan mixed; do
    TRACE_FILE="$TRACES_DIR/${TRACE_TYPE}.txt"
    echo "Running trace: $TRACE_TYPE"

    for F in "${FRAMES[@]}"; do
        echo "  Frames: $F"

        # Run each algorithm, skip CSV header line, prefix trace type
        for ALGO in lru lfu arc tempograph; do
            if [ "$ALGO" = "tempograph" ]; then
                RESULT=$("$BINARY" -a "$ALGO" -f "$F" -w "$WINDOW" -d "$DECAY" \
                         "$TRACE_FILE" 2>/dev/null | tail -1)
            else
                RESULT=$("$BINARY" -a "$ALGO" -f "$F" \
                         "$TRACE_FILE" 2>/dev/null | tail -1)
            fi
            echo "${TRACE_TYPE},${RESULT}" >> "$RESULTS_FILE"
        done
    done
done

echo ""
echo "Results saved to $RESULTS_FILE"
echo "Run 'python3 plot_results.py' to generate graphs."
