#include <limits.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/cache.h"

#define MAX_UNIQUE_NODES 1048576  // Bounded hashing space
#define MAX_EDGES_PER_NODE 64     // Sparse array edge limit
#define SAMPLE_SIZE 64            // LRU tail sample size
#define MAX_WINDOW 64             // Max sliding window size
#define DEFAULT_WINDOW 10
#define DECAY_SHIFT 6  // weight -= weight>>6  (~0.984)
#define DECAY_INTERVAL 2000

typedef struct {
  uint16_t dst;
  uint16_t w;
} Edge;

typedef struct {
  Edge edges[MAX_UNIQUE_NODES][MAX_EDGES_PER_NODE];
  uint8_t edge_cnt[MAX_UNIQUE_NODES];

  int win_buf[MAX_WINDOW];
  int win_head;
  int win_size;
  int W;

  int mru_time[MAX_UNIQUE_NODES];  // Tracks recency for tiebreaking
  int access_count;
  int global_time;

  // libCacheSim native LRU doubly-linked list pointers
  cache_obj_t *q_head;
  cache_obj_t *q_tail;
} Tempograph_params_t;

static inline int page_idx(obj_id_t page) { return (int)((uint64_t)page % MAX_UNIQUE_NODES); }

static void adj_inc(Tempograph_params_t *params, int u, int v) {
  int n = params->edge_cnt[u];
  for (int i = 0; i < n; i++) {
    if (params->edges[u][i].dst == (uint16_t)v) {
      if (params->edges[u][i].w < 65535) params->edges[u][i].w++;
      return;
    }
  }
  if (n < MAX_EDGES_PER_NODE) {
    params->edges[u][n].dst = (uint16_t)v;
    params->edges[u][n].w = 1;
    params->edge_cnt[u]++;
    return;
  }
  // Overflow: replace weakest edge
  int mi = 0;
  for (int i = 1; i < MAX_EDGES_PER_NODE; i++) {
    if (params->edges[u][i].w < params->edges[u][mi].w) mi = i;
  }
  params->edges[u][mi].dst = (uint16_t)v;
  params->edges[u][mi].w = 1;
}

static inline uint32_t adj_weight(Tempograph_params_t *params, int u, int v) {
  int n = params->edge_cnt[u];
  for (int i = 0; i < n; i++) {
    if (params->edges[u][i].dst == (uint16_t)v) return params->edges[u][i].w;
  }
  return 0;
}

static void apply_decay(Tempograph_params_t *params) {
#if DECAY_SHIFT > 0
  for (int u = 0; u < MAX_UNIQUE_NODES; u++) {
    int n = params->edge_cnt[u];
    for (int i = 0; i < n;) {
      uint16_t w = params->edges[u][i].w;
      w = (uint16_t)(w - (w >> DECAY_SHIFT));
      if (w == 0) {
        params->edges[u][i] = params->edges[u][--n];
      } else {
        params->edges[u][i].w = w;
        i++;
      }
    }
    params->edge_cnt[u] = (uint8_t)n;
  }
#endif
}

static void update_graph(Tempograph_params_t *params, int new_idx) {
  for (int i = 0; i < params->win_size; i++) {
    int pos = (params->win_head - 1 - i + params->W + params->W) % params->W;
    int u = params->win_buf[pos];
    if (u != new_idx) adj_inc(params, u, new_idx);
  }
}

static void slide_window(Tempograph_params_t *params, int new_idx) {
  params->win_buf[params->win_head] = new_idx;
  params->win_head = (params->win_head + 1) % params->W;
  if (params->win_size < params->W) params->win_size++;
}

static long score_page(Tempograph_params_t *params, int idx) {
  long s = 0;
  for (int i = 0; i < params->win_size; i++) {
    int pos = (params->win_head - 1 - i + params->W + params->W) % params->W;
    s += adj_weight(params, params->win_buf[pos], idx);
  }
  return s;
}

static void Tempograph_free(cache_t *cache) {
  free(cache->eviction_params);
  cache_struct_free(cache);
}

static bool Tempograph_get(cache_t *cache, const request_t *req) {
  Tempograph_params_t *params = (Tempograph_params_t *)cache->eviction_params;
  params->access_count++;
  params->global_time++;

  if (params->access_count % DECAY_INTERVAL == 0) apply_decay(params);

  bool ck_hit = cache_get_base(cache, req);
  int idx = page_idx(req->obj_id);

  if (ck_hit) {
    cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, req->obj_id);
    params->mru_time[idx] = params->global_time;  // O(1) timestamp update

    move_obj_to_head(&params->q_head, &params->q_tail, obj);
    update_graph(params, idx);
    slide_window(params, idx);
  }

  return ck_hit;
}

static cache_obj_t *Tempograph_find(cache_t *cache, const request_t *req, const bool update_cache) {
  return cache_find_base(cache, req, update_cache);
}

static cache_obj_t *Tempograph_insert(cache_t *cache, const request_t *req) {
  Tempograph_params_t *params = (Tempograph_params_t *)cache->eviction_params;
  cache_obj_t *obj = cache_insert_base(cache, req);

  int idx = page_idx(req->obj_id);

  update_graph(params, idx);  // Update graph *before* sliding window

  params->mru_time[idx] = params->global_time;
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);

  slide_window(params, idx);

  return obj;
}

static cache_obj_t *Tempograph_to_evict(cache_t *cache, const request_t *req) {
  Tempograph_params_t *params = (Tempograph_params_t *)cache->eviction_params;

  cache_obj_t *best_frame = NULL;
  long best_score = LONG_MAX;
  int best_time = -1;
  int steps = 0;

  cache_obj_t *curr = params->q_tail;

  // O(1) Tail Sampling
  while (curr != NULL && steps < SAMPLE_SIZE) {
    int idx = page_idx(curr->obj_id);
    long s = score_page(params, idx);
    int t = params->mru_time[idx];

    if (s < best_score || (s == best_score && t > best_time)) {
      best_score = s;
      best_time = t;
      best_frame = curr;
    }

    curr = curr->queue.prev;
    steps++;
  }

  return best_frame;
}

static void Tempograph_evict(cache_t *cache, const request_t *req) {
  Tempograph_params_t *params = (Tempograph_params_t *)cache->eviction_params;
  cache_obj_t *obj = Tempograph_to_evict(cache, req);

  if (obj != NULL) {
    remove_obj_from_list(&params->q_head, &params->q_tail, obj);
    cache_evict_base(cache, obj, true);
  }
}

static bool Tempograph_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) return false;

  Tempograph_params_t *params = (Tempograph_params_t *)cache->eviction_params;
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj, true);
  return true;
}

cache_t *Tempograph_init(const common_cache_params_t ccache_params, const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("Tempograph", ccache_params, cache_specific_params);
  cache->cache_init = Tempograph_init;
  cache->cache_free = Tempograph_free;
  cache->get = Tempograph_get;
  cache->find = Tempograph_find;
  cache->insert = Tempograph_insert;
  cache->evict = Tempograph_evict;
  cache->remove = Tempograph_remove;
  cache->to_evict = Tempograph_to_evict;

  cache->eviction_params = malloc(sizeof(Tempograph_params_t));
  memset(cache->eviction_params, 0, sizeof(Tempograph_params_t));

  Tempograph_params_t *params = (Tempograph_params_t *)cache->eviction_params;
  params->W = DEFAULT_WINDOW;

  return cache;
}