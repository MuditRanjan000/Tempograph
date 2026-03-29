/*
 * common.h - Shared definitions for TempoGraph simulation framework
 * CSD 204 - Operating Systems Project
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

/* ── Simulation limits ─────────────────────────────────────────── */
#define MAX_FRAMES      512
#define MAX_UNIQUE      4096
#define MAX_TRACE       5000000
#define DEFAULT_WINDOW  10
#define DECAY_ALPHA     0.98f

/* ── Access result codes ───────────────────────────────────────── */
#define HIT   0
#define FAULT 1

/* ── Algorithm identifiers ─────────────────────────────────────── */
#define ALGO_LRU        0
#define ALGO_LFU        1
#define ALGO_ARC        2
#define ALGO_TEMPOGRAPH 3
#define ALGO_COUNT      4

/* ── Simulation result ─────────────────────────────────────────── */
typedef struct {
    int    n_accesses;
    int    n_faults;
    float  fault_rate;
    float  hit_rate;
    double time_ms;
} Result;

#endif /* COMMON_H */
