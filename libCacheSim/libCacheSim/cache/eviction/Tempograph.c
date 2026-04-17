#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/cache.h"
#include <limits.h>

#define MAX_UNIQUE_NODES 4096 // Bounded for memory safety
#define MAX_WINDOW 64
#define DEFAULT_WINDOW 10
#define RESYNC_INTERVAL 500
#define DECAY_INTERVAL 2000

typedef struct {
    uint16_t graph[MAX_UNIQUE_NODES][MAX_UNIQUE_NODES];
    long cached_score[MAX_UNIQUE_NODES];
    int window[MAX_WINDOW];
    int win_head;
    int win_size;
    int W;
    float alpha;
    int access_count;
    int fault_count;
    int global_time;
    
    // Maintain an MRU queue for tie-breaking
    cache_obj_t *q_head;
    cache_obj_t *q_tail;
} Tempograph_params_t;

// Hash full object IDs into the bounded graph space
static inline int page_idx(obj_id_t page) {
    return (int)(page % MAX_UNIQUE_NODES);
}

static void apply_decay(Tempograph_params_t *params) {
    if (params->alpha >= 1.0f) return;
    for (int i = 0; i < MAX_UNIQUE_NODES; i++) {
        for (int j = 0; j < MAX_UNIQUE_NODES; j++) {
            if (params->graph[i][j] > 0)
                params->graph[i][j] = (uint16_t)(params->graph[i][j] * params->alpha);
        }
    }
}

static void update_graph(Tempograph_params_t *params, int new_idx) {
    for (int i = 0; i < params->win_size; i++) {
        int pos = (params->win_head - 1 - i + params->W + params->W) % params->W;
        int u = params->window[pos];
        if (u != new_idx && params->graph[u][new_idx] < 65535)
            params->graph[u][new_idx]++;
    }
}

static void slide_window(Tempograph_params_t *params, int new_idx) {
    params->window[params->win_head] = new_idx;
    params->win_head = (params->win_head + 1) % params->W;
    if (params->win_size < params->W) params->win_size++;
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
        // Move to MRU position
        cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, req->obj_id);
        move_obj_to_head(&params->q_head, &params->q_tail, obj);
    } else {
        params->fault_count++;
    }

    update_graph(params, idx);
    slide_window(params, idx);
    
    return ck_hit;
}

static cache_obj_t *Tempograph_find(cache_t *cache, const request_t *req, const bool update_cache) {
    return cache_find_base(cache, req, update_cache);
}

static cache_obj_t *Tempograph_insert(cache_t *cache, const request_t *req) {
    Tempograph_params_t *params = (Tempograph_params_t *)cache->eviction_params;
    cache_obj_t *obj = cache_insert_base(cache, req);
    
    // Insert at MRU head
    prepend_obj_to_head(&params->q_head, &params->q_tail, obj);
    
    // Compute initial score
    int idx = page_idx(req->obj_id);
    long new_score = 0;
    cache_obj_t *curr = params->q_head;
    while(curr) {
        if (curr != obj) new_score += params->graph[idx][page_idx(curr->obj_id)];
        curr = curr->queue.next;
    }
    params->cached_score[idx] = new_score;
    
    return obj;
}

static cache_obj_t *Tempograph_to_evict(cache_t *cache, const request_t *req) {
    Tempograph_params_t *params = (Tempograph_params_t *)cache->eviction_params;
    cache_obj_t *best_frame = NULL;
    long best_score = LONG_MAX;
    
    cache_obj_t *curr = params->q_head;
    while (curr != NULL) {
        int idx = page_idx(curr->obj_id);
        long s = params->cached_score[idx];
        
        // Scan from MRU to LRU. First found with min score is kept (tie-breaking by MRU)
        if (s <= best_score) { 
            best_score = s;
            best_frame = curr;
        }
        curr = curr->queue.next;
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
    params->alpha = 1.0f; // Adjust decay via parsed arguments if needed
    
    return cache;
}