/*
 * sieve.h - SIEVE page replacement algorithm
 * CSD 204 - Operating Systems Project
 *
 * Based on: Y. Zhang, J. Yang, Y. Yue, Y. Vigfusson, K.V. Rashmi,
 *   "SIEVE is Simpler than LRU: an Efficient Turn-Key Eviction Algorithm
 *    for Web Caches," USENIX NSDI 2024, pp. 1229-1246.
 *
 * SIEVE uses a single FIFO queue and a "hand" pointer.
 * Each page has a visited bit. On HIT: set visited=1.
 * On FAULT: hand scans until it finds visited=0 -> evict that page.
 *   Pages with visited=1 get reset to 0 and skipped (given a second chance).
 *
 * Beats ARC on ~45% of real traces (1559 traces from Meta, Wikipedia, Twitter).
 * Simpler than LRU: fewer than 20 lines of core logic.
 */

#ifndef SIEVE_H
#define SIEVE_H

#include "common.h"

void sieve_init   (int n_frames);
int  sieve_access (int page);
void sieve_reset  (void);

#endif /* SIEVE_H */