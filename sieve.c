/*
 * sieve.c - SIEVE page replacement algorithm
 * CSD 204 - Operating Systems Project
 *
 * Based on: Y. Zhang, J. Yang, Y. Yue, Y. Vigfusson, K.V. Rashmi,
 *   "SIEVE is Simpler than LRU: an Efficient Turn-Key Eviction Algorithm
 *    for Web Caches," USENIX NSDI 2024, pp. 1229-1246.
 *
 * Data structures:
 *   pages[N]    - page stored in each slot (-1 = empty)
 *   visited[N]  - visited bit per slot (0 or 1)
 *   hand        - integer pointer into pages[], advances on eviction
 *   n           - capacity (number of physical frames)
 *
 * Complexity:
 *   HIT:   O(N) scan (linear search for page in frames)
 *   FAULT: O(N) worst-case hand scan
 *   Space: O(N)
 */

#include "sieve.h"
#include <string.h>

/* ── State ────────────────────────────────────────────────────────── */
static int pages  [MAX_FRAMES];   /* page stored in each slot          */
static int visited[MAX_FRAMES];   /* visited bit: 1 = recently hit     */
static int hand;                  /* eviction hand position            */
static int n;                     /* frame capacity                    */

/* ── Public API ───────────────────────────────────────────────────── */

/*
 * sieve_init - initialise SIEVE with n_frames physical frames.
 * Based on: Zhang et al., USENIX NSDI 2024, Algorithm 1.
 */
void sieve_init(int n_frames) {
    n    = n_frames;
    hand = 0;
    memset(pages,   -1, sizeof(int) * n);
    memset(visited,  0, sizeof(int) * n);
}

/*
 * sieve_access - process one page access.
 * Returns HIT if page already in frames, FAULT otherwise.
 * Based on: Zhang et al., USENIX NSDI 2024, Algorithm 1.
 *
 * HIT path:  find page -> set visited=1 -> return HIT
 * FAULT path: advance hand past visited=1 pages (resetting them to 0)
 *             until visited=0 slot found -> evict it -> insert new page
 */
int sieve_access(int page) {
    /* ── HIT: linear scan for existing page ─────────────────────── */
    for (int i = 0; i < n; i++) {
        if (pages[i] == page) {
            visited[i] = 1;     /* give it a second chance */
            return HIT;
        }
    }

    /* ── FAULT: advance hand to find eviction victim ─────────────── */
    while (visited[hand]) {
        visited[hand] = 0;              /* clear second-chance bit   */
        hand = (hand + 1) % n;          /* advance hand (circular)   */
    }

    /* Evict pages[hand], insert new page */
    pages  [hand] = page;
    visited[hand] = 0;
    hand = (hand + 1) % n;

    return FAULT;
}

/*
 * sieve_reset - free all state (re-initialise to empty).
 * Based on: Zhang et al., USENIX NSDI 2024.
 */
void sieve_reset(void) {
    sieve_init(n);
}