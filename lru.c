/*
 * lru.c - Least Recently Used page replacement
 * CSD 204 - Operating Systems Project
 */

#include "lru.h"

/* ── Doubly-linked list node ──────────────────────────────────────── */
typedef struct Node {
    int          page;
    struct Node *prev;
    struct Node *next;
} Node;

/* ── Hash map: page -> Node* (chained) ───────────────────────────── */
#define HM_SIZE 8192
typedef struct HEntry { int key; Node *val; struct HEntry *next; } HEntry;
static HEntry *hmap[HM_SIZE];

static Node *hm_get(int key) {
    HEntry *e = hmap[(unsigned)key % HM_SIZE];
    while (e) { if (e->key == key) return e->val; e = e->next; }
    return NULL;
}
static void hm_put(int key, Node *val) {
    unsigned idx = (unsigned)key % HM_SIZE;
    HEntry *e = hmap[idx];
    while (e) { if (e->key == key) { e->val = val; return; } e = e->next; }
    e = malloc(sizeof(HEntry));
    e->key = key; e->val = val;
    e->next = hmap[idx]; hmap[idx] = e;
}
static void hm_del(int key) {
    unsigned idx = (unsigned)key % HM_SIZE;
    HEntry **pp = &hmap[idx];
    while (*pp) {
        if ((*pp)->key == key) {
            HEntry *tmp = *pp; *pp = tmp->next; free(tmp); return;
        }
        pp = &(*pp)->next;
    }
}

/* ── LRU state ────────────────────────────────────────────────────── */
static Node *head = NULL;   /* Most Recently Used end  */
static Node *tail = NULL;   /* Least Recently Used end */
static int   size = 0;
static int   capacity = 0;

/* ── Internal helpers ─────────────────────────────────────────────── */

/* Remove node from list (does not free it) */
static void list_remove(Node *n) {
    if (n->prev) n->prev->next = n->next; else head = n->next;
    if (n->next) n->next->prev = n->prev; else tail = n->prev;
    n->prev = n->next = NULL;
}

/* Insert node at head (MRU position) */
static void list_push_front(Node *n) {
    n->prev = NULL;
    n->next = head;
    if (head) head->prev = n;
    head = n;
    if (!tail) tail = n;
}

/* ── Public API ───────────────────────────────────────────────────── */

void lru_init(int n_frames) {
    lru_reset();
    capacity = n_frames;
}

int lru_access(int page) {
    Node *n = hm_get(page);

    if (n) {
        /* Cache HIT: move to MRU position */
        list_remove(n);
        list_push_front(n);
        return HIT;
    }

    /* Cache FAULT */
    if (size == capacity) {
        /* Evict LRU page (tail) */
        Node *victim = tail;
        hm_del(victim->page);
        list_remove(victim);
        free(victim);
        size--;
    }

    /* Load new page at MRU position */
    n = malloc(sizeof(Node));
    n->page = page;
    n->prev = n->next = NULL;
    list_push_front(n);
    hm_put(page, n);
    size++;
    return FAULT;
}

void lru_reset(void) {
    /* Free list */
    Node *n = head;
    while (n) { Node *tmp = n->next; free(n); n = tmp; }
    head = tail = NULL;
    size = capacity = 0;
    /* Free hash map */
    for (int i = 0; i < HM_SIZE; i++) {
        HEntry *e = hmap[i];
        while (e) { HEntry *tmp = e->next; free(e); e = tmp; }
        hmap[i] = NULL;
    }
}
