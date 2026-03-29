/*
 * lru.h - Least Recently Used page replacement algorithm
 * CSD 204 - Operating Systems Project
 *
 * Uses a doubly-linked list (head=MRU, tail=LRU) + hash map for O(1)
 * access. On each hit, the page moves to the head (MRU). On a fault,
 * the tail (LRU) is evicted.
 */

#ifndef LRU_H
#define LRU_H

#include "common.h"

void lru_init   (int n_frames);
int  lru_access (int page);     /* Returns HIT or FAULT */
void lru_reset  (void);

#endif /* LRU_H */
