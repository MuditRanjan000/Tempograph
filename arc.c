/*
 * arc.c - Adaptive Replacement Cache
 * CSD 204 - Operating Systems Project
 *
 * Reference: N. Megiddo and D. Modha, "ARC: A Self-Tuning, Low
 * Overhead Replacement Cache," USENIX FAST 2003.
 *
 * Each of the four lists (T1, T2, B1, B2) is implemented as a
 * doubly-linked list. A hash map provides O(1) membership lookup.
 */

#include "arc.h"

/* ── List node ──────────────────────────────────────────────────── */
typedef struct ANode {
    int           page;
    int           list;     /* Which list: 0=T1, 1=T2, 2=B1, 3=B2 */
    struct ANode *prev;
    struct ANode *next;
} ANode;

#define LIST_T1 0
#define LIST_T2 1
#define LIST_B1 2
#define LIST_B2 3

/* ── Doubly-linked list (head=MRU, tail=LRU) ────────────────────── */
typedef struct { ANode *head, *tail; int size; } DList;
static DList lists[4];

static void dl_remove(DList *l, ANode *n) {
    if (n->prev) n->prev->next = n->next; else l->head = n->next;
    if (n->next) n->next->prev = n->prev; else l->tail = n->prev;
    n->prev = n->next = NULL;
    l->size--;
}
static void dl_push_front(DList *l, ANode *n) {
    n->prev = NULL; n->next = l->head;
    if (l->head) l->head->prev = n;
    l->head = n;
    if (!l->tail) l->tail = n;
    l->size++;
}
static ANode *dl_pop_tail(DList *l) {
    if (!l->tail) return NULL;
    ANode *n = l->tail;
    dl_remove(l, n);
    return n;
}

/* ── Hash map: page -> ANode* ───────────────────────────────────── */
#define AHM_SIZE 8192
typedef struct AHE { int key; ANode *val; struct AHE *next; } AHE;
static AHE *ahmap[AHM_SIZE];

static ANode *ahm_get(int key) {
    AHE *e = ahmap[(unsigned)key % AHM_SIZE];
    while (e) { if (e->key == key) return e->val; e = e->next; }
    return NULL;
}
static void ahm_put(int key, ANode *val) {
    unsigned idx = (unsigned)key % AHM_SIZE;
    AHE *e = ahmap[idx];
    while (e) { if (e->key == key) { e->val = val; return; } e = e->next; }
    e = malloc(sizeof(AHE));
    e->key = key; e->val = val;
    e->next = ahmap[idx]; ahmap[idx] = e;
}
static void ahm_del(int key) {
    unsigned idx = (unsigned)key % AHM_SIZE;
    AHE **pp = &ahmap[idx];
    while (*pp) {
        if ((*pp)->key == key) {
            AHE *tmp = *pp; *pp = tmp->next; free(tmp); return;
        }
        pp = &(*pp)->next;
    }
}

/* ── ARC state ──────────────────────────────────────────────────── */
static int c = 0;  /* Cache capacity */
static int p = 0;  /* Target size for T1 */

/* ── ARC replace subroutine ─────────────────────────────────────── */
static void arc_replace(int in_b2) {
    int t1_size = lists[LIST_T1].size;
    /* Choose from T1 if T1 is over target, or if x is in B2 and T1==p */
    if (t1_size > 0 && (t1_size > p || (in_b2 && t1_size == p))) {
        /* Move LRU of T1 to MRU of B1 */
        ANode *victim = dl_pop_tail(&lists[LIST_T1]);
        victim->list = LIST_B1;
        dl_push_front(&lists[LIST_B1], victim);
        ahm_put(victim->page, victim);  /* keep in map for ghost tracking */
    } else {
        /* Move LRU of T2 to MRU of B2 */
        ANode *victim = dl_pop_tail(&lists[LIST_T2]);
        victim->list = LIST_B2;
        dl_push_front(&lists[LIST_B2], victim);
        ahm_put(victim->page, victim);
    }
}

/* ── Public API ─────────────────────────────────────────────────── */

void arc_init(int n_frames) {
    arc_reset();
    c = n_frames;
    p = 0;
}

int arc_access(int page) {
    ANode *n = ahm_get(page);

    /* ── Case 1: page in T1 or T2 (cache hit) ─────────────────── */
    if (n && (n->list == LIST_T1 || n->list == LIST_T2)) {
        dl_remove(&lists[n->list], n);
        n->list = LIST_T2;
        dl_push_front(&lists[LIST_T2], n);
        return HIT;
    }

    /* ── Case 2: page in B1 (ghost hit - was recently in T1) ───── */
    if (n && n->list == LIST_B1) {
        /* Increase p: B1 hit means T1 is too small */
        int delta = (lists[LIST_B1].size >= lists[LIST_B2].size) ? 1
                    : lists[LIST_B2].size / lists[LIST_B1].size;
        p = (p + delta < c) ? p + delta : c;
        /* Replace then promote to T2 */
        arc_replace(0);
        dl_remove(&lists[LIST_B1], n);
        n->list = LIST_T2;
        dl_push_front(&lists[LIST_T2], n);
        return FAULT;
    }

    /* ── Case 3: page in B2 (ghost hit - was recently in T2) ───── */
    if (n && n->list == LIST_B2) {
        /* Decrease p: B2 hit means T2 is too small */
        int delta = (lists[LIST_B2].size >= lists[LIST_B1].size) ? 1
                    : lists[LIST_B1].size / lists[LIST_B2].size;
        p = (p - delta > 0) ? p - delta : 0;
        /* Replace then promote to T2 */
        arc_replace(1);
        dl_remove(&lists[LIST_B2], n);
        n->list = LIST_T2;
        dl_push_front(&lists[LIST_T2], n);
        return FAULT;
    }

    /* ── Case 4: completely new page ───────────────────────────── */
    int t1_len = lists[LIST_T1].size + lists[LIST_B1].size;
    int total  = t1_len + lists[LIST_T2].size + lists[LIST_B2].size;

    if (t1_len == c) {
        /* B1 or T1 must give up a slot */
        if (lists[LIST_T1].size < c) {
            ANode *ghost = dl_pop_tail(&lists[LIST_B1]);
            ahm_del(ghost->page);
            free(ghost);
            arc_replace(0);
        } else {
            ANode *victim = dl_pop_tail(&lists[LIST_T1]);
            ahm_del(victim->page);
            free(victim);
        }
    } else if (t1_len < c && total >= c) {
        if (total >= 2 * c && lists[LIST_B2].size > 0) {
            ANode *ghost = dl_pop_tail(&lists[LIST_B2]);
            ahm_del(ghost->page);
            free(ghost);
        }
        arc_replace(0);
    }

    /* Add new page to MRU of T1 */
    ANode *nn = malloc(sizeof(ANode));
    nn->page = page; nn->list = LIST_T1;
    nn->prev = nn->next = NULL;
    dl_push_front(&lists[LIST_T1], nn);
    ahm_put(page, nn);
    return FAULT;
}

void arc_reset(void) {
    /* Free all nodes */
    for (int l = 0; l < 4; l++) {
        ANode *n = lists[l].head;
        while (n) { ANode *tmp = n->next; free(n); n = tmp; }
        lists[l].head = lists[l].tail = NULL;
        lists[l].size = 0;
    }
    /* Free hash map */
    for (int i = 0; i < AHM_SIZE; i++) {
        AHE *e = ahmap[i];
        while (e) { AHE *tmp = e->next; free(e); e = tmp; }
        ahmap[i] = NULL;
    }
    c = p = 0;
}
