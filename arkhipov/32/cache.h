#ifndef CACHEPROXY_CACHE_H
#define CACHEPROXY_CACHE_H

#define STATUS_INITIAL (0)
#define STATUS_IN_PROCESS (1)
#define STATUS_COMPLETED (2)

#include <stdlib.h>
#include <pthread.h>
#include <string.h>

typedef struct {
    char* content;
    int content_size;
    char* key;
    int key_size;
    long last_visited;
    char status;
    pthread_rwlock_t rwlock;
} CacheItem;

typedef struct {
    char* content;
    int content_size;
} CacheRow;

typedef struct CacheNode {
    struct CacheNode* next;
    CacheItem* value;
} CacheNode;

typedef struct {
    CacheNode* buckets;
    int* buckets_size;
    int size;
    int bucket_capacity;
} Cache;

int InitCache(Cache* cache, int size, int bucket_capacity);

CacheItem* GetOrCreateCacheItem(Cache* cache, char* key, int size);

void LookupAndClean(Cache* cache, long timeout);

int DeleteCache(Cache* cache);

#endif //CACHEPROXY_CACHE_H
