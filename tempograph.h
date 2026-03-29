/*
 * tempograph.h - TempoGraph page replacement algorithm
 * CSD 204 - Operating Systems Project
 *
 * TempoGraph models page access history as a directed weighted graph
 * G = (V, E). Edge weight w(u,v) counts how often page v was accessed
 * within a sliding window of W accesses after page u.
 *
 * On eviction, each resident page p is scored:
 *   score(p) = SUM of w(p,q) for all q currently in frames
 * The page with the lowest score is evicted.
 */

#ifndef TEMPOGRAPH_H
#define TEMPOGRAPH_H

#include "common.h"

void tg_init   (int n_frames, int window_size, float decay);
int  tg_access (int page);
void tg_reset  (void);

#endif /* TEMPOGRAPH_H */
