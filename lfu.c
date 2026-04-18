/*
 * lfu.c - Least Frequently Used page replacement
 * CSD 204 - Operating Systems Project
 *
 * Each frame slot stores a page, its frequency count, and the time
 * it was last accessed. Eviction selects the slot with the minimum
 * frequency; ties are broken by choosing the least recently used
 * among the tied slots.
 */

#include "lfu.h"
#include <limits.h>

/* ── Per-slot state ──────────────────────────────────────────────── */
typedef struct {
    int page;        /* Page currently in this frame (-1 = empty) */
    int freq;        /* Access frequency count                     */
    int last_time;   /* Last access timestamp (for tie-breaking)   */
} Slot;

static Slot  slots[MAX_FRAMES];
static int   cap  = 0;
static int   size = 0;
static int   lfu_clock = 0;  /* Logical clock */

/* ── Public API ──────────────────────────────────────────────────── */

void lfu_init(int n_frames) {
    lfu_reset();
    cap = n_frames;
    for (int i = 0; i < cap; i++) {
        slots[i].page = -1;
        slots[i].freq = 0;
        slots[i].last_time = 0;
    }
}

int lfu_access(int page) {
    lfu_clock++;

    /* Search for page in frames */
    for (int i = 0; i < cap; i++) {
        if (slots[i].page == page) {
            /* HIT: update frequency and recency */
            slots[i].freq++;
            slots[i].last_time = lfu_clock;
            return HIT;
        }
    }

    /* FAULT: find victim (min freq, tie-break by LRU) */
    int victim = -1;

    if (size < cap) {
        /* There is an empty slot - find it */
        for (int i = 0; i < cap; i++) {
            if (slots[i].page == -1) { victim = i; break; }
        }
        size++;
    } else {
        /* Evict: find slot with minimum frequency, LRU on tie */
        int min_freq = INT_MAX, min_time = INT_MAX;
        for (int i = 0; i < cap; i++) {
            if (slots[i].freq < min_freq ||
               (slots[i].freq == min_freq && slots[i].last_time < min_time)) {
                min_freq = slots[i].freq;
                min_time = slots[i].last_time;
                victim = i;
            }
        }
    }

    /* Load new page into victim slot */
    slots[victim].page      = page;
    slots[victim].freq      = 1;
    slots[victim].last_time = lfu_clock;
    return FAULT;
}

void lfu_reset(void) {
    cap = size = lfu_clock = 0;
    memset(slots, 0, sizeof(slots));
    for (int i = 0; i < MAX_FRAMES; i++) slots[i].page = -1;
}
