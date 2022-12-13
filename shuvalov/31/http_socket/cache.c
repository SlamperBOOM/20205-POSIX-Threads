#include <string.h>
#include <stdlib.h>
#include "cache.h"

struct hashmap* cache = NULL;

int cached_response_compare(const void* a, const void* b, void* udata) {
    const struct cached_response* cra = a;
    const struct cached_response* crb = b;
    return strcmp(cra->url, crb->url);
}

uint64_t cached_response_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const struct cached_response* cached_response = item;
    return hashmap_sip(cached_response->url, strlen(cached_response->url), seed0, seed1);
}

void free_cache() {
    if (cache != NULL) {
        size_t iter = 0;
        void* item;
        while (hashmap_iter(cache, &iter, &item)) {
            const struct cached_response* cached_response = item;
            free(cached_response->url);
            free(cached_response->response);
        }
        hashmap_free(cache);
    }
}
