/*
 * tempograph.c - TempoGraph page replacement algorithm
 * CSD 204 - Operating Systems Project
 *
 * Key data structures:
 *   graph[i][j] - directed edge weight from page-index i to page-index j
 *   window[]    - circular buffer of last W page indices (the sliding window)
 *   frames[]    - pages currently in physical memory
 *   in_frames[] - boolean: is page-index i currently in a frame?
 *
 * Page IDs are mapped to indices 0..MAX_UNIQUE-1 using: idx = page % MAX_UNIQUE
 * This ensures graph stays within a fixed memory footprint.
 */

#include "tempograph.h"
#include <limits.h>

/* ── Graph (directed, weighted) ─────────────────────────────────── */
/*
 * graph[i][j] stores the co-access weight from page-index i to j.
 * Using uint16_t caps each weight at 65535 (sufficient for any trace).
 * Memory footprint: 4096 * 4096 * 2 = 32 MB.
 */
static uint16_t graph[MAX_UNIQUE][MAX_UNIQUE];

/* ── Frame pool ─────────────────────────────────────────────────── */
static int frames[MAX_FRAMES];  /* pages currently loaded            */
static int in_frames[MAX_UNIQUE];  /* in_frames[idx] = 1 if loaded   */
static int cap  = 0;            /* frame pool capacity               */
static int size = 0;            /* current number of loaded pages    */

/* ── Sliding window ─────────────────────────────────────────────── */
static int window[MAX_FRAMES + 16]; /* circular buffer of page indices */
static int win_head = 0;        /* next write position               */
static int win_size = 0;        /* current number of entries         */
static int W        = DEFAULT_WINDOW; /* window capacity             */

/* ── Decay ──────────────────────────────────────────────────────── */
static float alpha = 1.0f;      /* decay factor (1.0 = no decay)    */
static int   access_count = 0;  /* for periodic decay application   */
#define DECAY_INTERVAL 1000     /* apply decay every N accesses      */

/* ── Helper: map page_id to graph index ─────────────────────────── */
static inline int page_idx(int page) {
    return ((unsigned)page) % MAX_UNIQUE;
}

/* ── Update graph: record that 'new_idx' was accessed ───────────── */
/*
 * For every page u in the current window, increment w(u, new_idx).
 * This records that new_idx was accessed after u within window W.
 */
static void update_graph(int new_idx) {
    int count = win_size;
    for (int i = 0; i < count; i++) {
        int pos = (win_head - 1 - i + W + W) % W;
        int u = window[pos];
        if (u != new_idx) {
            /* Increment weight, cap at UINT16_MAX to prevent overflow */
            if (graph[u][new_idx] < 65535)
                graph[u][new_idx]++;
        }
    }
}

/* ── Slide window: add new_idx to the window ────────────────────── */
static void slide_window(int new_idx) {
    window[win_head] = new_idx;
    win_head = (win_head + 1) % W;
    if (win_size < W) win_size++;
}

/* ── Apply decay to all edge weights (periodic) ─────────────────── */
static void apply_decay(void) {
    if (alpha >= 1.0f) return;  /* No decay configured */
    for (int i = 0; i < MAX_UNIQUE; i++)
        for (int j = 0; j < MAX_UNIQUE; j++)
            graph[i][j] = (uint16_t)(graph[i][j] * alpha);
}

/* ── Choose victim: page with lowest score in frame pool ────────── */
/*
 * score(p) = SUM of w(p, q) for all q currently in frames
 * The page with the LOWEST score is evicted because it has the
 * weakest predicted connection to the current working set.
 */
static int choose_victim(void) {
    int   best_frame = 0;
    long  best_score = LONG_MAX;

    for (int f = 0; f < cap; f++) {
        if (frames[f] == -1) return f;  /* empty slot, use it */

        int src = page_idx(frames[f]);
        long score = 0;

        /* Sum outgoing weights to all currently loaded pages */
        for (int g = 0; g < cap; g++) {
            if (g != f && frames[g] != -1) {
                score += graph[src][page_idx(frames[g])];
            }
        }

        if (score < best_score) {
            best_score = score;
            best_frame = f;
        }
    }
    return best_frame;
}

/* ── Public API ─────────────────────────────────────────────────── */

void tg_init(int n_frames, int window_size, float decay) {
    tg_reset();
    cap   = n_frames;
    W     = (window_size > 0) ? window_size : DEFAULT_WINDOW;
    alpha = decay;

    /* Initialise all frames as empty */
    for (int i = 0; i < cap; i++) frames[i] = -1;
}

int tg_access(int page) {
    int idx = page_idx(page);
    access_count++;

    /* Periodically apply edge weight decay */
    if (access_count % DECAY_INTERVAL == 0)
        apply_decay();

    /* ── Hit: page is already in a frame ─────────────────────── */
    if (in_frames[idx]) {
        update_graph(idx);
        slide_window(idx);
        return HIT;
    }

    /* ── Fault: page not in memory ───────────────────────────── */
    int victim_frame = choose_victim();

    /* Evict current occupant of victim frame */
    if (frames[victim_frame] != -1) {
        in_frames[page_idx(frames[victim_frame])] = 0;
    }

    /* Load new page */
    frames[victim_frame] = page;
    in_frames[idx] = 1;
    if (size < cap) size++;

    /* Update graph and window after loading */
    update_graph(idx);
    slide_window(idx);

    return FAULT;
}

void tg_reset(void) {
    memset(graph,     0, sizeof(graph));
    memset(frames,    -1, sizeof(frames));
    memset(in_frames, 0, sizeof(in_frames));
    memset(window,    0, sizeof(window));
    cap = size = win_head = win_size = access_count = 0;
    W     = DEFAULT_WINDOW;
    alpha = 1.0f;
}
