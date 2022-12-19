#include <string.h>
#include <stdlib.h>
#include "hashmap/hashmap.h"
#include "cache.h"

int cached_response_compare(const void* a, const void* b, void* udata) {
    const struct cached_response* cra = a;
    const struct cached_response* crb = b;
    return strcmp(cra->url, crb->url);
}

uint64_t cached_response_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const struct cached_response* cached_response = item;
    return hashmap_sip(cached_response->url, strlen(cached_response->url), seed0, seed1);
}

void free_cache(struct cache cache) {
    if (cache.map != NULL) {
        size_t iter = 0;
        void* item;
        while (hashmap_iter(cache.map, &iter, &item)) {
            struct cached_response* cached_response = item;
            free(cached_response->url);
            free(cached_response->response->buf);
            free(cached_response->response->subscribers);
            free(cached_response->response->headers);
            free(cached_response->response);
        }
        hashmap_free(cache.map);
    }
}

void init_cache(struct cache* cache) {
    cache->map = hashmap_new(sizeof(struct cached_response), 0, 0, 0,
                        cached_response_hash, cached_response_compare, NULL, NULL);
}

struct cached_response* get_cached_response(struct cache cache, char* url) {
    return hashmap_get(cache.map,
                &(struct cached_response) {.url = url});
}

void add_response_to_cache(struct cache cache, char* url, struct response* response) {
    hashmap_set(cache.map,
                &(struct cached_response) {.url = url, .response = response});
    response->in_cache = 1;
}
