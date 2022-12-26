#include "cache.h"
#include <stdio.h>

int InitCache(Cache* cache, int size, int bucket_capacity) {
    cache->size = size;
    cache->bucket_capacity = bucket_capacity;
    cache->buckets = (CacheNode *)malloc( size * sizeof(CacheNode));
    cache->mutexes = (pthread_mutex_t *)malloc( size * sizeof(pthread_mutex_t));
    cache->buckets_size = (int *) calloc(bucket_capacity, sizeof(int));
    if (cache->mutexes == NULL || cache->buckets == NULL || cache->buckets_size == NULL) {
        return 1;
    }
    for (int i = 0; i < size; ++i) {
        int err = pthread_mutex_init(&(cache->mutexes[i]), NULL);
        if (err != 0) {
            return err;
        }
    }
    for (int i = 0; i < size; ++i) {
        cache->buckets[i].next = NULL;
        cache->buckets[i].value = NULL;
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
    pthread_mutex_lock(&(cache->mutexes[hash]));
    if (cache->buckets_size[hash] >= cache->bucket_capacity) {
        pthread_mutex_unlock(&(cache->mutexes[hash]));
        return 1;
    }
    CacheNode* cursor = &(cache->buckets[hash]);
    while (cursor->next != NULL) {
        cursor = cursor->next;
    }
    CacheNode* inserted_node = malloc(sizeof(CacheNode));
    if (inserted_node == NULL) {
        pthread_mutex_unlock(&(cache->mutexes[hash]));
        return 1;
    }
    cache->buckets_size[hash]++;
    cursor->next = inserted_node;
    inserted_node->next = NULL;
    inserted_node->value = item;
    pthread_mutex_unlock(&(cache->mutexes[hash]));
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

CacheRow* GetCacheItem(Cache* cache, char* key, int size) {
    int hash = GetHash(key, size, cache->size);
    pthread_mutex_lock(&(cache->mutexes[hash]));

    CacheNode* cursor = cache->buckets[hash].next;
    while (cursor != NULL) {
        CacheItem* cursor_value = cursor->value;
        if (cursor_value == NULL) {
            cursor = cursor->next;
            continue;
        }
        if (StrEquals(cursor_value->key, cursor_value->key_size, key, size)) {
            cursor_value->last_visited = time(NULL);
            CacheRow* cache_row = malloc(sizeof(CacheRow));
            if (cache_row == NULL) {
                pthread_mutex_unlock(&(cache->mutexes[hash]));
                return NULL;
            }
            cache_row->content = malloc(cursor_value->content_size * sizeof(char));
            if (cache_row->content == NULL) {
                free(cache_row);
                pthread_mutex_unlock(&(cache->mutexes[hash]));
                return NULL;
            }
            cache_row->content_size = cursor_value->content_size;
            memmove(cache_row->content, cursor_value->content, cache_row->content_size);

            pthread_mutex_unlock(&(cache->mutexes[hash]));
            return cache_row;
        }
        cursor = cursor->next;
    }

    pthread_mutex_unlock(&(cache->mutexes[hash]));
    return NULL;
}

void DeleteCacheNode(CacheNode* cache_node) {
    if (cache_node == NULL) {
        return;
    }
    if (cache_node->value != NULL) {
        free(cache_node->value->key);
        free(cache_node->value->content);
    }
    free(cache_node->value);
    free(cache_node);
}

void LookupAndClean(Cache* cache, long timeout) {
    int cache_size = cache->size;
    for (int i = 0; i < cache_size; ++i) {
        pthread_mutex_lock(&(cache->mutexes[i]));
        CacheNode* prev = &(cache->buckets[i]);
        CacheNode* cursor = prev->next;
        while (cursor != NULL) {
            CacheItem* item = cursor->value;
            if (time(NULL) - item->last_visited > timeout) {
                prev->next = cursor->next;
                cache->buckets_size[i]--;
                DeleteCacheNode(cursor);
                cursor = prev;
            }
            prev = cursor;
            cursor = cursor->next;
        }
        pthread_mutex_unlock(&(cache->mutexes[i]));
    }
}

void DeleteCacheBucket(CacheNode* head) {
    CacheNode* cursor = head->next;
    while (cursor != NULL) {
        CacheNode* next = cursor->next;
        DeleteCacheNode(cursor);
        cursor = next;
    }
}

int DeleteCache(Cache* cache) {
    int cache_size = cache->size;
    for (int i = 0; i < cache_size; ++i) {
        pthread_mutex_lock(&(cache->mutexes[i]));
        DeleteCacheBucket(&(cache->buckets[i]));
        cache->buckets_size[i] = 0;
        pthread_mutex_unlock(&(cache->mutexes[i]));

        int err = pthread_mutex_destroy(&(cache->mutexes[i]));
        if (err != 0) {
            return 1;
        }
    }
    free(cache->mutexes);
    free(cache->buckets);
    free(cache->buckets_size);
    free(cache);
    return 0;
}