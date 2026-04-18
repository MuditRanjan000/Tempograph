/*
 * lfu.h - Least Frequently Used page replacement algorithm
 * CSD 204 - Operating Systems Project
 *
 * Tracks access frequency per page. On a fault, evicts the page with
 * the lowest frequency. Ties broken by LRU (recency).
 */

#ifndef LFU_H
#define LFU_H

#include "common.h"

void lfu_init   (int n_frames);
int  lfu_access (int page);
void lfu_reset  (void);

#endif /* LFU_H */
