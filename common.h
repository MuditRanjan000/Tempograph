/*
 * common.h - Shared definitions for TempoGraph simulation framework
 * CSD 204 - Operating Systems Project
 * Group 12: Mudit Ranjan, Mugdh Mittal, Aayush Trivedi
 */

#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>
#include <float.h>

/* ── Simulation limits ──────────────────────────────────────────────
 * MAX_FRAMES   - maximum number of physical frames supported.
 *                Increase if testing very large frame counts (>512).
 * MAX_UNIQUE   - maximum number of distinct page indices.
 *                Also sets the graph matrix dimension: MAX_UNIQUE^2 * 2 bytes.
 *                At 4096: graph = 4096*4096*2 = 32MB (lives in RAM/BSS).
 * MAX_TRACE    - maximum trace length (5M accesses). Increase for large traces.
 * DEFAULT_WINDOW - TempoGraph sliding window W. Tuned experimentally to W=10;
 *                  see sensitivity analysis in Section IV-B of the report.
 *                  Range tested: W=5,10,15,20,25. W=10 best overall.
 * DECAY_ALPHA  - TempoGraph edge decay factor applied every DECAY_INTERVAL
 *                accesses. 0.98 = 2% decay per interval. Controls how quickly
 *                old access patterns are forgotten. 1.0 = no decay.
 * ─────────────────────────────────────────────────────────────────── */
#define MAX_FRAMES      512
#define MAX_UNIQUE      4096
#define MAX_TRACE       5000000
#define DEFAULT_WINDOW  10
#define DECAY_ALPHA     0.98f

/* ── Access result codes ─────────────────────────────────────────── */
#define HIT   0
#define FAULT 1

/* ── Algorithm identifiers ───────────────────────────────────────────
 * Used as indices into ALGO_NAMES[] in main.c and as switch cases.
 * Add new algorithms at the end; update ALGO_COUNT accordingly.
 * ─────────────────────────────────────────────────────────────────── */
#define ALGO_LRU        0   /* Least Recently Used                     */
#define ALGO_LFU        1   /* Least Frequently Used                   */
#define ALGO_ARC        2   /* Adaptive Replacement Cache (FAST 2003)  */
#define ALGO_TEMPOGRAPH 3   /* TempoGraph — novel graph-based (2026)   */
#define ALGO_SIEVE      4   /* SIEVE — FIFO+visited bit (NSDI 2024)    */
#define ALGO_COUNT      5   /* Total number of algorithms              */

/* ── Simulation result ───────────────────────────────────────────── */
typedef struct {
    int    n_accesses;
    int    n_faults;
    float  fault_rate;
    float  hit_rate;
    double time_ms;
} Result;

#endif /* COMMON_H */