#include "cache.h"
#include <stdio.h>

int InitCache(Cache* cache, int size, int bucket_capacity) {
    cache->size = size;
    cache->bucket_capacity = bucket_capacity;
    cache->buckets = (CacheNode *)malloc( size * sizeof(CacheNode));
    cache->buckets_size = (int *) calloc(bucket_capacity, sizeof(int));
    if (cache->buckets == NULL || cache->buckets_size == NULL) {
        return 1;
    }
    for (int i = 0; i < size; ++i) {
        cache->buckets[i].next = NULL;
        cache->buckets[i].value = malloc(sizeof(CacheItem));
        pthread_rwlock_init(&(cache->buckets[i].value->rwlock), NULL);
    }
    return 0;
}

int GetHash(const char* str, int size, int p) {
    int hash = 0;
    for (int i = 0; i < size; ++i) {
        hash = (hash + (int)(str[i])) % p;
    }
    return hash;
}

int InsertCacheItem(Cache* cache, CacheItem* item) {
    int hash = GetHash(item->key, item->key_size, cache->size);
    if (cache->buckets_size[hash] >= cache->bucket_capacity) {
        return 1;
    }
    CacheNode* cursor = &(cache->buckets[hash]);
    while (cursor->next != NULL) {
        cursor = cursor->next;
    }
    CacheNode* inserted_node = malloc(sizeof(CacheNode));
    if (inserted_node == NULL) {
        return 1;
    }
    cache->buckets_size[hash]++;
    cursor->next = inserted_node;
    inserted_node->next = NULL;
    inserted_node->value = item;
    return 0;
}

int StrEquals(const char* key_1, int key_1_size, const char* key_2, int key_2_size) {
    if (key_1_size != key_2_size) {
        return 0;
    }
    for (int i = 0; i < key_1_size; ++i) {
        if (key_1[i] != key_2[i]) return 0;
    }
    return 1;
}

CacheItem* GetOrCreateCacheItem(Cache* cache, char* key, int size) {
    int hash = GetHash(key, size, cache->size);

    // try to find
    pthread_rwlock_wrlock(&(cache->buckets[hash].value->rwlock));
    CacheNode* cursor = cache->buckets[hash].next;
    CacheItem* cache_item = NULL;
    while (cursor != NULL) {
        CacheItem* cursor_value = cursor->value;
        if (cursor_value == NULL) {
            cursor = cursor->next;
            continue;
        }
        if (StrEquals(cursor_value->key, cursor_value->key_size, key, size)) {
            cursor_value->last_visited = time(NULL);
            cache_item = cursor_value;
            break;
        }
        cursor = cursor->next;
    }

    if (cache_item != NULL) {
        pthread_rwlock_unlock(&(cache->buckets[hash].value->rwlock));
        return cache_item;
    }
    // try to create
    int err = 0;
    cache_item = malloc(sizeof(CacheItem));
    if (cache_item == NULL) {
        pthread_rwlock_unlock(&(cache->buckets[hash].value->rwlock));
        return NULL;
    }
    cache_item->key = malloc((size + 1) * sizeof(char));
    if (cache_item->key == NULL) {
        fprintf(stderr, "Unable to allocate memory for cache row\n");
        free(cache_item);
        pthread_rwlock_unlock(&(cache->buckets[hash].value->rwlock));
        return NULL;
    }
    strcpy(cache_item->key, key);
    cache_item->key_size = size;
    cache_item->last_visited = time(NULL);
    cache_item->content_size = 0;
    cache_item->status = STATUS_INITIAL;
    pthread_rwlock_init(&(cache_item->rwlock), NULL);
    err = InsertCacheItem(cache, cache_item);
    if (err != 0) {
        fprintf(stderr, "Unable to save to cache\n");
        pthread_rwlock_destroy(&(cache_item->rwlock));
        free(cache_item->key);
        free(cache_item->content);
        pthread_rwlock_unlock(&(cache->buckets[hash].value->rwlock));
        return NULL;
    }

    pthread_rwlock_unlock(&(cache->buckets[hash].value->rwlock));
    return cache_item;
}

void DeleteCacheNode(CacheNode* cache_node) {
    if (cache_node == NULL) {
        return;
    }
    if (cache_node->value != NULL) {
        CacheItem* value = cache_node->value;
        free(value->key);
        free(value->content);
        pthread_rwlock_destroy(&(value->rwlock));
    }
    free(cache_node->value);
    free(cache_node);
}

void LookupAndClean(Cache* cache, long timeout) {
    int deleted_nodes = 0;
    int cache_size = cache->size;
    for (int i = 0; i < cache_size; ++i) {
        pthread_rwlock_wrlock(&(cache->buckets[i].value->rwlock));
        CacheNode* prev = &(cache->buckets[i]);
        CacheNode* cursor = prev->next;
        while (cursor != NULL) {
            CacheItem* item = cursor->value;
            if (time(NULL) - item->last_visited > timeout) {
                pthread_rwlock_wrlock(&(item->rwlock));
                prev->next = cursor->next;
                cache->buckets_size[i]--;
                DeleteCacheNode(cursor);
                cursor = prev;
                pthread_rwlock_unlock(&(item->rwlock));
            }
            prev = cursor;
            cursor = cursor->next;
            deleted_nodes++;
        }
        pthread_rwlock_unlock(&(cache->buckets[i].value->rwlock));
    }

    printf("%d nodes deleted\n", deleted_nodes);
}

void DeleteCacheBucket(CacheNode* head) {
    CacheNode* cursor = head->next;
    pthread_rwlock_destroy(&(head->value->rwlock));
    while (cursor != NULL) {
        CacheNode* next = cursor->next;
        DeleteCacheNode(cursor);
        cursor = next;
    }
}

int DeleteCache(Cache* cache) {
    int cache_size = cache->size;
    for (int i = 0; i < cache_size; ++i) {
        DeleteCacheBucket(&(cache->buckets[i]));
        cache->buckets_size[i] = 0;
    }
    free(cache->buckets);
    free(cache->buckets_size);
    free(cache);
    return 0;
}