#include <stdio.h>
#include "picohttpparser/picohttpparser.h"

#ifndef INC_31_CACHE_H
#define INC_31_CACHE_H


struct cached_response {
    struct response* response;
    char* url;
};

struct cache {
    struct hashmap* map;
};

int cached_response_compare(const void* a, const void* b, void* udata);

uint64_t cached_response_hash(const void* item, uint64_t seed0, uint64_t seed1);

void free_cache(struct cache cache);

void init_cache(struct cache* cache);

struct cached_response* get_cached_response(struct cache cache, char* url);

void add_response_to_cache(struct cache cache, char* url, struct response* response);

#endif //INC_31_CACHE_H
