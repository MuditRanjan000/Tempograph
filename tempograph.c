/*
 * tempograph.c - TempoGraph page replacement algorithm (Optimised v5)
 * CSD 204 - Operating Systems Project
 *
 * Changes from v4:
 *
 * 1. Eliminated cached_score[] array and all O(N) score-maintenance.
 * v4 called update_scores_for_new_window_entry() on EVERY access
 * (hit or miss), looping over all cap frames — O(N) per access.
 * v5 computes scores on-demand only at eviction time.
 *
 * 2. On-demand inward scoring.
 * When a fault occurs, score_page(idx) scans the sliding window
 * (W entries, W=10 by default) and sums w(W_i → idx).  Cost:
 * O(W × E) where W ≤ 64 and E ≤ MAX_EDGES_PER_NODE — a small
 * constant independent of cache size N.
 *
 * 3. LRU-ordered doubly-linked list for O(1) tail sampling.
 * Frames are kept in a doubly-linked list ordered by recency
 * (head = MRU, tail = LRU).  On a hit the page moves to the
 * head in O(1).  At eviction time choose_victim() walks at most
 * SAMPLE_SIZE nodes from the tail, scores each one, and evicts
 * the lowest scorer.  Total eviction cost: O(S × W × E) where
 * S = SAMPLE_SIZE = 64 — O(1) w.r.t. N.
 *
 * 4. Asymptotic summary (N = cache size, W = window, E = edges/node,
 * S = sample size — W, E, S are all fixed compile-time constants):
 * Cache HIT:  O(W × E)  — update_graph only
 * Cache MISS: O(W × E)  — update_graph + score S candidates
 * (S × W × E folds into O(1) w.r.t N)
 *
 * 5. Adjacency list, integer-only decay, MRU tiebreaking all
 * carried forward from v4 unchanged.
 */

#include "tempograph.h"
#include <limits.h>

/* ── Compile-time constants ─────────────────────────────────────── */
#define MAX_EDGES_PER_NODE  64   /* max out-edges stored per graph node  */
#define SAMPLE_SIZE         64   /* LRU-tail candidates examined at evict */
#define MAX_WINDOW          64   /* hard cap on window size               */
#define DECAY_SHIFT          6   /* weight -= weight>>6  ≈ ×0.984        */
#define DECAY_INTERVAL    2000   /* accesses between decay sweeps         */

/* ── Adjacency list ─────────────────────────────────────────────── */
/*
 * edges[u][i] stores one directed edge from node u.
 * edge_cnt[u] is the number of live edges (0..MAX_EDGES_PER_NODE).
 * On overflow the slot with the lowest weight is replaced, preserving
 * the strongest (most predictive) historical signal.
 */
typedef struct { uint16_t dst; uint16_t w; } Edge;

static Edge    edges[MAX_UNIQUE][MAX_EDGES_PER_NODE];
static uint8_t edge_cnt[MAX_UNIQUE];

/* Return pointer to edge u→v, or NULL if absent. */
static Edge *adj_get(int u, int v) {
    int n = edge_cnt[u];
    for (int i = 0; i < n; i++)
        if (edges[u][i].dst == (uint16_t)v)
            return &edges[u][i];
    return NULL;
}

/* Increment w(u→v), inserting the edge if needed.
 * If node u is full, replace the lowest-weight edge. */
static void adj_inc(int u, int v) {
    int n = edge_cnt[u];
    for (int i = 0; i < n; i++) {
        if (edges[u][i].dst == (uint16_t)v) {
            if (edges[u][i].w < 65535) edges[u][i].w++;
            return;
        }
    }
    if (n < MAX_EDGES_PER_NODE) {
        edges[u][n].dst = (uint16_t)v;
        edges[u][n].w   = 1;
        edge_cnt[u]++;
        return;
    }
    /* Overflow: find and replace the weakest edge */
    int mi = 0;
    for (int i = 1; i < MAX_EDGES_PER_NODE; i++)
        if (edges[u][i].w < edges[u][mi].w) mi = i;
    edges[u][mi].dst = (uint16_t)v;
    edges[u][mi].w   = 1;
}

/* Return w(u→v), or 0 if absent. */
static inline uint32_t adj_weight(int u, int v) {
    Edge *e = adj_get(u, v);
    return e ? e->w : 0;
}

/* ── Integer decay ──────────────────────────────────────────────── */
/*
 * DECAY_SHIFT 6  →  weight × (1 − 1/64) ≈ weight × 0.984
 * No FPU instructions; safe for kernel context.
 * Dead edges (weight→0) are compacted out to keep lists short.
 */
static void apply_decay(void) {
#if DECAY_SHIFT > 0
    for (int u = 0; u < MAX_UNIQUE; u++) {
        int n = edge_cnt[u];
        for (int i = 0; i < n; ) {
            uint16_t w = edges[u][i].w;
            w = (uint16_t)(w - (w >> DECAY_SHIFT));
            if (w == 0) {
                edges[u][i] = edges[u][--n]; /* swap-remove */
            } else {
                edges[u][i].w = w;
                i++;
            }
        }
        edge_cnt[u] = (uint8_t)n;
    }
#endif
}

/* ── LRU doubly-linked list ─────────────────────────────────────── */
/*
 * All resident frames are linked MRU→LRU.
 * lru_head = most recently used frame index  (-1 if empty)
 * lru_tail = least recently used frame index (-1 if empty)
 * lru_prev[f], lru_next[f] — per-frame list links
 *
 * Operations:
 * lru_touch(f)   — move frame f to MRU head  O(1)
 * lru_insert(f)  — insert new frame at MRU head  O(1)
 * lru_remove(f)  — unlink frame f from any position  O(1)
 *
 * SAMPLE_SIZE tail nodes are inspected at eviction; all other frames
 * are never touched during eviction scoring.
 */
static int lru_prev[MAX_FRAMES];
static int lru_next[MAX_FRAMES];
static int lru_head = -1;
static int lru_tail = -1;

static void lru_unlink(int f) {
    int p = lru_prev[f], n = lru_next[f];
    if (p != -1) lru_next[p] = n; else lru_head = n;
    if (n != -1) lru_prev[n] = p; else lru_tail = p;
    lru_prev[f] = lru_next[f] = -1;
}

static void lru_push_head(int f) {
    lru_prev[f] = -1;
    lru_next[f] = lru_head;
    if (lru_head != -1) lru_prev[lru_head] = f;
    lru_head = f;
    if (lru_tail == -1) lru_tail = f;
}

static void lru_touch(int f) {
    if (lru_head == f) return;   /* already MRU */
    lru_unlink(f);
    lru_push_head(f);
}

/* ── Frame pool ─────────────────────────────────────────────────── */
static int frames[MAX_FRAMES];      /* frames[f] = page stored in slot f */
static int in_frames[MAX_UNIQUE];   /* 1 if page idx is resident         */
static int frame_of[MAX_UNIQUE];    /* which frame slot holds page idx   */
static int cap  = 0;
static int size = 0;

/* ── Sliding window ─────────────────────────────────────────────── */
static int win_buf[MAX_WINDOW];   /* circular buffer of page indices */
static int win_head = 0;
static int win_size = 0;
static int W = DEFAULT_WINDOW;

/* ── Recency timestamps for MRU tiebreaking ─────────────────────── */
/*
 * mru_time[f] records the logical access time at which frame f was
 * last touched (hit or loaded).  Used solely as a tiebreaker inside
 * choose_victim(): when two candidates have equal inward scores,
 * evict the MORE recently used one (MRU tiebreaking).
 *
 * Why MRU and not LRU?  In cyclic scan workloads the graph starts
 * cold and all scores are zero.  Evicting the MRU page keeps older
 * pages that cycle back sooner — the page just used won't be needed
 * again until the full scan repeats, so evicting it is safer.
 * Pure LRU tiebreaking produces 100% faults on scan.
 */
static int mru_time[MAX_FRAMES];
static int global_time = 0;

/* ── Misc ───────────────────────────────────────────────────────── */
static int access_count = 0;

/* ── Helpers ────────────────────────────────────────────────────── */
static inline int page_idx(int page) {
    return ((unsigned)page) % MAX_UNIQUE;
}

/*
 * update_graph(new_idx)
 * Build outgoing edges: increment w(u → new_idx) for every u
 * currently in the window.  Called BEFORE slide_window so new_idx
 * is not its own predecessor.  Cost: O(W × E).
 */
static void update_graph(int new_idx) {
    for (int i = 0; i < win_size; i++) {
        int pos = (win_head - 1 - i + W + W) % W;
        int u   = win_buf[pos];
        if (u != new_idx) adj_inc(u, new_idx);
    }
}

static void slide_window(int new_idx) {
    win_buf[win_head] = new_idx;
    win_head = (win_head + 1) % W;
    if (win_size < W) win_size++;
}

/*
 * score_page(idx)
 * On-demand window-inward score: Σ w(W_i → idx) for W_i in window.
 * Cost: O(W × E) — constant w.r.t. cache size N.
 *
 * Higher score = more strongly predicted by recent accesses = keep.
 * Lower score  = not predicted by recent window = evict.
 */
static long score_page(int idx) {
    long s = 0;
    for (int i = 0; i < win_size; i++) {
        int pos = (win_head - 1 - i + W + W) % W;
        s += adj_weight(win_buf[pos], idx);
    }
    return s;
}

/*
 * choose_victim()
 * Walk at most SAMPLE_SIZE nodes from the LRU tail.
 * Score each candidate on-demand.  Evict the lowest scorer;
 * break ties by keeping the one closest to the tail (oldest = more
 * likely to be part of an older, completed cycle).
 *
 * Total cost: O(S × W × E) — all compile-time constants, so O(1)
 * with respect to cache size N.
 */
static int choose_victim(void) {
    /* Empty slot fast-path */
    if (size < cap) {
        for (int f = 0; f < cap; f++)
            if (frames[f] == -1) return f;
    }

    int  best_frame = -1;
    long best_score = LONG_MAX;
    int  best_time  = -1;   /* MRU tiebreaker: track HIGHEST time seen */
    int  steps      = 0;
    int  f          = lru_tail;

    while (f != -1 && steps < SAMPLE_SIZE) {
        int  idx = page_idx(frames[f]);
        long s   = score_page(idx);
        int  t   = mru_time[f];

        /*
         * Evict: lowest inward score (least predicted by window).
         * Tie:   evict MOST recently used (highest mru_time).
         *
         * MRU tiebreaking on equal scores: the page just used won't
         * be needed again until the next cycle, so evicting it is
         * cheaper than evicting an older page that may return sooner.
         * This turns a cold-graph scan from 100% faults toward near-0.
         */
        if (s < best_score || (s == best_score && t > best_time)) {
            best_score = s;
            best_time  = t;
            best_frame = f;
        }
        f = lru_prev[f];   /* walk toward MRU */
        steps++;
    }

    return (best_frame != -1) ? best_frame : lru_tail;
}

/* ── Public API ─────────────────────────────────────────────────── */

/*
 * tg_init — initialise TempoGraph.
 * The `decay` float is accepted for API compatibility but unused
 * internally; integer bit-shift decay (DECAY_SHIFT) is used instead.
 */
void tg_init(int n_frames, int window_size, float decay) {
    (void)decay;
    tg_reset();
    cap = n_frames;
    W   = (window_size > 0 && window_size <= MAX_WINDOW)
          ? window_size : DEFAULT_WINDOW;
    for (int i = 0; i < cap; i++) {
        frames[i]   = -1;
        lru_prev[i] = lru_next[i] = -1;
    }
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
        mru_time[f] = global_time;  /* refresh recency stamp */
        lru_touch(f);               /* promote to MRU — O(1) */
        update_graph(idx);          /* O(W × E)               */
        slide_window(idx);
        return HIT;
    }

    /* ── FAULT ───────────────────────────────────────────────── */
    update_graph(idx);         /* O(W × E) — before slide so idx isn't own pred */

    int victim = choose_victim();   /* O(S × W × E) = O(1) w.r.t. N */

    /* Evict current occupant of victim slot */
    if (frames[victim] != -1) {
        int old_idx = page_idx(frames[victim]);
        in_frames[old_idx] = 0;
        frame_of[old_idx]  = -1;
        lru_unlink(victim);    /* remove from LRU list before re-inserting */
    }

    /* Load new page into victim slot */
    frames[victim]   = page;
    in_frames[idx]   = 1;
    frame_of[idx]    = victim;
    mru_time[victim] = global_time;
    if (size < cap) size++;

    lru_push_head(victim);     /* new page is MRU — O(1) */
    slide_window(idx);
    return FAULT;
}

void tg_reset(void) {
    memset(edges,     0,  sizeof(edges));
    memset(edge_cnt,  0,  sizeof(edge_cnt));
    memset(frames,   -1,  sizeof(frames));
    memset(in_frames, 0,  sizeof(in_frames));
    memset(frame_of, -1,  sizeof(frame_of));
    memset(lru_prev, -1,  sizeof(lru_prev));
    memset(lru_next, -1,  sizeof(lru_next));
    memset(win_buf,   0,  sizeof(win_buf));
    memset(mru_time,  0,  sizeof(mru_time));
    cap = size = win_head = win_size = 0;
    access_count = global_time = 0;
    lru_head = lru_tail = -1;
    W = DEFAULT_WINDOW;
}