#define _POSIX_C_SOURCE 200809L
/*
 * main.c - TempoGraph Simulation Framework
 * CSD 204 - Operating Systems Project
 * Group 12: Mudit Ranjan, Mugdh Mittal, Aayush Trivedi
 *
 * Usage:
 *   ./tempograph -a <algo> -f <frames> [-w <window>] [-d <decay>] <tracefile>
 *
 *   -a  Algorithm: lru | lfu | arc | tempograph | sieve | all
 *   -f  Number of physical frames (e.g. 64)
 *   -w  TempoGraph window size W (default: 10)
 *   -d  TempoGraph decay factor 0.0-1.0 (default: 1.0 = no decay)
 *   -v  Verbose: print per-access events
 *
 * Trace file format:
 *   One integer page number per line (lines starting with # are comments).
 *   Example:
 *     1042
 *     7831
 *     1042
 *     ...
 *
 * Output (CSV-friendly):
 *   algorithm,frames,accesses,faults,fault_rate,hit_rate,time_ms
 *
 * Algorithms:
 *   LRU        - Silberschatz et al., OS Concepts, 10th ed., Ch. 10
 *   LFU        - Silberschatz et al., OS Concepts, 10th ed., Ch. 10
 *   ARC        - Megiddo & Modha, USENIX FAST 2003
 *   TempoGraph - Novel graph-based algorithm, CSD204 Group 12, 2026
 *   SIEVE      - Zhang et al., USENIX NSDI 2024, pp. 1229-1246
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "lru.h"
#include "lfu.h"
#include "arc.h"
#include "tempograph.h"
#include "sieve.h"

/* Algorithm display names — order must match ALGO_* constants in common.h */
static const char *ALGO_NAMES[] = {"LRU", "LFU", "ARC", "TempoGraph", "SIEVE"};

/* ── Trace storage ──────────────────────────────────────────────── */
static int trace[MAX_TRACE];
static int trace_len = 0;

/* ── Load trace from file ───────────────────────────────────────── */
/*
 * load_trace - reads one integer page number per line.
 * Lines starting with '#' are treated as comments and skipped.
 * Based on: standard trace format used by UMass Trace Repository
 *   and libCacheSim (NSDI24-SIEVE benchmark suite).
 */
static int load_trace(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "Error: cannot open trace '%s'\n", path); return 0; }

    char line[256];
    trace_len = 0;
    while (fgets(line, sizeof(line), f) && trace_len < MAX_TRACE) {
        if (line[0] == '#' || line[0] == '\n') continue; /* skip comments */
        trace[trace_len++] = atoi(line);
    }
    fclose(f);
    fprintf(stderr, "Loaded %d accesses from %s\n", trace_len, path);
    return trace_len;
}

/* ── Run one simulation pass ────────────────────────────────────── */
/*
 * run_sim - replay the loaded trace with the given algorithm.
 * Returns a Result struct with fault/hit rates and timing.
 */
static Result run_sim(int algo, int n_frames, int window, float decay) {
    Result r = {0};
    r.n_accesses = trace_len;

    /* Initialise chosen algorithm */
    switch (algo) {
        case ALGO_LRU:        lru_init(n_frames);                   break;
        case ALGO_LFU:        lfu_init(n_frames);                   break;
        case ALGO_ARC:        arc_init(n_frames);                   break;
        case ALGO_TEMPOGRAPH: tg_init (n_frames, window, decay);    break;
        case ALGO_SIEVE:      sieve_init(n_frames);                 break;
    }

    /* Time the simulation */
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    /* Replay trace */
    for (int i = 0; i < trace_len; i++) {
        int result;
        switch (algo) {
            case ALGO_LRU:        result = lru_access(trace[i]);   break;
            case ALGO_LFU:        result = lfu_access(trace[i]);   break;
            case ALGO_ARC:        result = arc_access(trace[i]);   break;
            case ALGO_TEMPOGRAPH: result =  tg_access(trace[i]);   break;
            case ALGO_SIEVE:      result = sieve_access(trace[i]); break;
            default: result = FAULT;
        }
        if (result == FAULT) r.n_faults++;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    r.time_ms    = (t1.tv_sec - t0.tv_sec) * 1000.0
                 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
    r.fault_rate = (float)r.n_faults / r.n_accesses;
    r.hit_rate   = 1.0f - r.fault_rate;

    /* Clean up algorithm state */
    switch (algo) {
        case ALGO_LRU:        lru_reset();   break;
        case ALGO_LFU:        lfu_reset();   break;
        case ALGO_ARC:        arc_reset();   break;
        case ALGO_TEMPOGRAPH: tg_reset();    break;
        case ALGO_SIEVE:      sieve_reset(); break;
    }
    return r;
}

/* ── Print result row ───────────────────────────────────────────── */
static void print_result(const char *algo_name, int n_frames, Result *r) {
    printf("%s,%d,%d,%d,%.6f,%.6f,%.2f\n",
        algo_name, n_frames,
        r->n_accesses, r->n_faults,
        r->fault_rate, r->hit_rate,
        r->time_ms);
}

/* ── Main ───────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    /* ── Defaults ───────────────────────────────────────────────── */
    int   n_frames  = 64;
    int   window    = DEFAULT_WINDOW;
    float decay     = 1.0f;
    int   algo      = -1;           /* -1 means run ALL algorithms  */
    char *tracefile = NULL;

    /* ── Argument parsing ───────────────────────────────────────── */
    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "-f") && i+1 < argc) n_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-w") && i+1 < argc) window   = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-d") && i+1 < argc) decay    = atof(argv[++i]);
        else if (!strcmp(argv[i], "-a") && i+1 < argc) {
            i++;
            if      (!strcmp(argv[i], "lru"))        algo = ALGO_LRU;
            else if (!strcmp(argv[i], "lfu"))        algo = ALGO_LFU;
            else if (!strcmp(argv[i], "arc"))        algo = ALGO_ARC;
            else if (!strcmp(argv[i], "tempograph")) algo = ALGO_TEMPOGRAPH;
            else if (!strcmp(argv[i], "sieve"))      algo = ALGO_SIEVE;
            else if (!strcmp(argv[i], "all"))        algo = -1;
            else { fprintf(stderr, "Unknown algorithm: %s\n", argv[i]); return 1; }
        }
        else if (argv[i][0] != '-') tracefile = argv[i];
    }

    if (!tracefile) {
        fprintf(stderr,
            "Usage: %s -a <lru|lfu|arc|tempograph|sieve|all> -f <frames> "
            "[-w <window>] [-d <decay>] <tracefile>\n", argv[0]);
        return 1;
    }

    /* ── Load trace ─────────────────────────────────────────────── */
    if (!load_trace(tracefile)) return 1;

    /* ── Print CSV header ───────────────────────────────────────── */
    printf("algorithm,frames,accesses,faults,fault_rate,hit_rate,time_ms\n");

    /* ── Run simulation(s) ──────────────────────────────────────── */
    if (algo == -1) {
        /* Run all algorithms */
        for (int a = 0; a < ALGO_COUNT; a++) {
            Result r = run_sim(a, n_frames, window, decay);
            print_result(ALGO_NAMES[a], n_frames, &r);
        }
    } else {
        Result r = run_sim(algo, n_frames, window, decay);
        print_result(ALGO_NAMES[algo], n_frames, &r);
    }

    return 0;
}