/*
 * arc.h - Adaptive Replacement Cache (ARC) page replacement
 * CSD 204 - Operating Systems Project
 *
 * ARC (Megiddo & Modha, USENIX FAST 2003) maintains four lists:
 *   T1 - recently accessed pages (seen exactly once)
 *   T2 - frequently accessed pages (seen two or more times)
 *   B1 - ghost list: recently evicted from T1
 *   B2 - ghost list: recently evicted from T2
 * An adaptive parameter p balances T1 vs T2 based on workload.
 */

#ifndef ARC_H
#define ARC_H

#include "common.h"

void arc_init   (int n_frames);
int  arc_access (int page);
void arc_reset  (void);

#endif /* ARC_H */
