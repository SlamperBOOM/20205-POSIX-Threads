#include "picohttpparser/picohttpparser.h"
#include "hashmap/hashmap.h"
#include <stdio.h>

#ifndef INC_31_CACHE_H
#define INC_31_CACHE_H


struct cached_response {
    struct response* response;
    char* url;
};

struct hashmap* cache;

int cached_response_compare(const void* a, const void* b, void* udata);

uint64_t cached_response_hash(const void* item, uint64_t seed0, uint64_t seed1);

void free_cache();

#endif //INC_31_CACHE_H
