/*
 * tempograph.c - TempoGraph page replacement algorithm (Optimised v3)
 * CSD 204 - Operating Systems Project
 *
 * Optimisations over v1:
 *   1. O(N) incremental cached scoring instead of O(N^2) per fault
 *   2. LRU tiebreaking: equal-score pages evicted by recency
 *   3. Faster decay: skips zero-weight edges entirely
 *   4. Periodic full resync every RESYNC_INTERVAL faults to
 *      correct drift caused by graph updates between faults
 *
 * Why NOT bidirectional scoring:
 *   Forward-only scoring (w(p->q)) correctly identifies "tail" pages
 *   in sequential scan workloads. Bidirectional scoring destroys this
 *   signal, degrading scan performance to LRU levels (100% faults).
 */

#include "tempograph.h"
#include <limits.h>

/* ── Graph ──────────────────────────────────────────────────────── */
static uint16_t graph[MAX_UNIQUE][MAX_UNIQUE];

/* ── Frame pool ─────────────────────────────────────────────────── */
static int frames[MAX_FRAMES];
static int in_frames[MAX_UNIQUE];
static int frame_of[MAX_UNIQUE];
static int cap  = 0;
static int size = 0;

/* ── Cached scores ──────────────────────────────────────────────── */
/*
 * cached_score[f] = SUM of w(frames[f] -> frames[g]) for all g!=f
 *
 * Updated O(N) on each fault:
 *   Evict old_idx from victim:  cached_score[f] -= w(frames[f], old_idx)
 *   Load  new idx  at victim:   cached_score[victim] = SUM w(idx, frames[g])
 *                               cached_score[f]     += w(frames[f], idx)
 *
 * Scores drift between faults as graph grows from HITs.
 * Full O(N^2) resync every RESYNC_INTERVAL faults corrects this.
 */
static long cached_score[MAX_FRAMES];
static int  fault_count = 0;
#define RESYNC_INTERVAL 500

/* ── LRU tiebreaking ────────────────────────────────────────────── */
static int lru_time[MAX_FRAMES];
static int global_time = 0;

/* ── Sliding window ─────────────────────────────────────────────── */
#define MAX_WINDOW 64
static int window[MAX_WINDOW];
static int win_head = 0;
static int win_size = 0;
static int W = DEFAULT_WINDOW;

/* ── Decay ──────────────────────────────────────────────────────── */
static float alpha        = 1.0f;
static int   access_count = 0;
#define DECAY_INTERVAL 2000

/* ── Helpers ────────────────────────────────────────────────────── */
static inline int page_idx(int page) {
    return ((unsigned)page) % MAX_UNIQUE;
}

static void update_graph(int new_idx) {
    for (int i = 0; i < win_size; i++) {
        int pos = (win_head - 1 - i + W + W) % W;
        int u = window[pos];
        if (u != new_idx && graph[u][new_idx] < 65535)
            graph[u][new_idx]++;
    }
}

static void slide_window(int new_idx) {
    window[win_head] = new_idx;
    win_head = (win_head + 1) % W;
    if (win_size < W) win_size++;
}

static void apply_decay(void) {
    if (alpha >= 1.0f) return;
    for (int i = 0; i < MAX_UNIQUE; i++)
        for (int j = 0; j < MAX_UNIQUE; j++)
            if (graph[i][j] > 0)
                graph[i][j] = (uint16_t)(graph[i][j] * alpha);
}

static void resync_scores(void) {
    for (int f = 0; f < cap; f++) {
        if (frames[f] == -1) { cached_score[f] = 0; continue; }
        int src = page_idx(frames[f]);
        long s = 0;
        for (int g = 0; g < cap; g++)
            if (g != f && frames[g] != -1)
                s += graph[src][page_idx(frames[g])];
        cached_score[f] = s;
    }
}

static int choose_victim(void) {
    int  best_frame = -1;
    long best_score = LONG_MAX;
    int  best_time  = INT_MAX;

    for (int f = 0; f < cap; f++) {
        if (frames[f] == -1) return f;
        long s = cached_score[f];
        int  t = lru_time[f];
        if (s < best_score || (s == best_score && t < best_time)) {
            best_score = s;
            best_time  = t;
            best_frame = f;
        }
    }
    return best_frame;
}

/* ── Public API ─────────────────────────────────────────────────── */

void tg_init(int n_frames, int window_size, float decay) {
    tg_reset();
    cap   = n_frames;
    W     = (window_size > 0 && window_size <= MAX_WINDOW)
            ? window_size : DEFAULT_WINDOW;
    alpha = decay;
    for (int i = 0; i < cap; i++) frames[i] = -1;
}

int tg_access(int page) {
    int idx = page_idx(page);
    access_count++;
    global_time++;

    if (access_count % DECAY_INTERVAL == 0)
        apply_decay();

    /* ── HIT ─────────────────────────────────────────────────── */
    if (in_frames[idx]) {
        int f = frame_of[idx];
        lru_time[f] = global_time;
        update_graph(idx);
        slide_window(idx);
        return HIT;
    }

    /* ── FAULT ───────────────────────────────────────────────── */
    fault_count++;
    update_graph(idx);

    if (fault_count % RESYNC_INTERVAL == 0)
        resync_scores();

    int victim = choose_victim();

    /* Remove evicted page contribution */
    if (frames[victim] != -1) {
        int old_idx = page_idx(frames[victim]);
        for (int f = 0; f < cap; f++)
            if (f != victim && frames[f] != -1)
                cached_score[f] -= graph[page_idx(frames[f])][old_idx];
        in_frames[old_idx] = 0;
        frame_of[old_idx]  = -1;
    }

    /* Load new page */
    frames[victim]   = page;
    in_frames[idx]   = 1;
    frame_of[idx]    = victim;
    lru_time[victim] = global_time;
    if (size < cap) size++;

    /* Compute new slot's score */
    long new_score = 0;
    for (int g = 0; g < cap; g++)
        if (g != victim && frames[g] != -1)
            new_score += graph[idx][page_idx(frames[g])];
    cached_score[victim] = new_score;

    /* Add new page contribution to others */
    for (int f = 0; f < cap; f++)
        if (f != victim && frames[f] != -1)
            cached_score[f] += graph[page_idx(frames[f])][idx];

    slide_window(idx);
    return FAULT;
}

void tg_reset(void) {
    memset(graph,        0,  sizeof(graph));
    memset(frames,       -1, sizeof(frames));
    memset(in_frames,    0,  sizeof(in_frames));
    memset(frame_of,     -1, sizeof(frame_of));
    memset(cached_score, 0,  sizeof(cached_score));
    memset(lru_time,     0,  sizeof(lru_time));
    memset(window,       0,  sizeof(window));
    cap = size = win_head = win_size = 0;
    access_count = fault_count = global_time = 0;
    W     = DEFAULT_WINDOW;
    alpha = 1.0f;
}