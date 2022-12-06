#ifndef CACHEPROXY_CACHE_H
#define CACHEPROXY_CACHE_H

#include <stdlib.h>
#include <pthread.h>
#include <string.h>

typedef struct {
    char* content;
    int content_size;
    char* key;
    int key_size;
    long last_visited;
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
    pthread_mutex_t* mutexes;
    int* buckets_size;
    int size;
    int bucket_capacity;
} Cache;

int InitCache(Cache* cache, int size, int bucket_capacity);

int InsertCacheItem(Cache* cache, CacheItem* item);

CacheRow* GetCacheItem(Cache* cache, char* key, int size);

void LookupAndClean(Cache* cache, long timeout);

int DeleteCache(Cache* cache);

#endif //CACHEPROXY_CACHE_H
