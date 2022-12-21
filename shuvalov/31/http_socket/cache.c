#include <string.h>
#include <stdlib.h>
#include "cache.h"

#define BUF_SIZE 4096


void subscribe(struct client* client, struct cache* cache, size_t cache_index) {
    struct response* response = cache->nodes[cache_index].response;
    for (int i = 0; i < response->subscribers_max_size; i++) {
        if (response->subscribers[i] == NULL) {
            response->subscribers[i] = client;
        }
    }
    client->cache_node = (int) cache_index;
    response->subscribers_count++;
    client->response = response;
}

void unsubscribe(struct client* client) {
    struct response* response = client->response;
    for (int i = 0; i < response->subscribers_max_size; i++) {
        if (response->subscribers[i] != NULL && response->subscribers[i]->fd == client->fd) {
            response->subscribers[i] = NULL;
        }
    }
    client->response->subscribers_count--;
}

struct client* get_subscriber(struct server server, size_t index) {
    return server.response->subscribers[index];
}

size_t get_subscribers_count(struct server server) {
    return server.response->subscribers_count;
}

void make_publisher(struct server* server, struct cache* cache, size_t cache_index) {
    struct response* response = cache->nodes[cache_index].response;
    server->response = response;
}

void free_cache(struct cache cache) {
    if (cache.nodes != NULL) {
        for (int i = 0; i < cache.size; i++) {
            free_cache_node(cache.nodes[i]);
        }
    }
}

void free_cache_node(struct cache_node cache_node) {
    if (cache_node.url != NULL) {
        free(cache_node.url);
    }
    if (cache_node.response != NULL) {
        free(cache_node.response->buf);
        free(cache_node.response->headers);
        free(cache_node.response->subscribers);
        free(cache_node.response);
    }
}

int init_cache(struct cache* cache, size_t size) {
    cache->last_node_set = -1;
    cache->nodes = (struct cache_node*) malloc(sizeof(cache->nodes[0]) * size);
    if (cache->nodes == NULL) {
        return -1;
    }
    cache->size = size;
    for (size_t i = 0; i < size; i++) {
        if (init_cache_node((cache->nodes + i)) != 0) {
            return -1;
        }
    }
    return 0;
}

int init_cache_node(struct cache_node* cache_node) {
    cache_node->empty = 1;
    cache_node->url = NULL;
    cache_node->response = (struct response*) malloc(sizeof(struct response));
    if (cache_node->response == NULL) {
        return -1;
    }
    cache_node->response->status = -1;
    cache_node->response->subscribers_count = 0;
    cache_node->response->buf_size = BUF_SIZE;
    cache_node->response->buf_len = 0;
    cache_node->response->content_length = -1;
    cache_node->response->not_content_length = -1;
    cache_node->response->buf = (char*) malloc(sizeof(char) * cache_node->response->buf_size);
    if (cache_node->response->buf == NULL) {
        return -1;
    }
    cache_node->response->headers_max_size = 100;
    cache_node->response->headers = (struct phr_header*) malloc(
            cache_node->response->headers_max_size * sizeof(struct phr_header));
    if (cache_node->response->headers == NULL) {
        return -1;
    }
    cache_node->response->subscribers_max_size = 100;
    cache_node->response->subscribers = (struct client**) malloc(
            sizeof(struct client*) * cache_node->response->subscribers_max_size);
    if (cache_node->response->subscribers == NULL) {
        return -1;
    }
    for (int i = 0; i < cache_node->response->subscribers_max_size; i++) {
        cache_node->response->subscribers[i] = NULL;
    }
    return 0;
}

void clear_cache_node(struct cache_node* cache_node) {
    cache_node->empty = 1;
    if (cache_node->url != NULL) {
        free(cache_node->url);
    }
    cache_node->url = NULL;
    cache_node->response->subscribers_count = 0;
    cache_node->response->buf_size = BUF_SIZE;
    cache_node->response->buf_len = 0;
    cache_node->response->content_length = -1;
    cache_node->response->not_content_length = -1;
    cache_node->response->headers_max_size = 100;
    cache_node->response->subscribers_max_size = 100;
}

struct cache_node* get(struct cache cache, char* url) {
    for (int i = 0; i < cache.size; i++) {
        if (cache.nodes->empty == 0) {
            if (strcmp(cache.nodes->url, url) == 0) {
                return cache.nodes;
            }
        }
    }
    return NULL;
}

void set_cache_node(struct cache_node* cache_node, char* url) {
    clear_cache_node(cache_node);
    cache_node->url = url;
    cache_node->empty = 0;
}

size_t set(struct cache* cache, char* url) {
    for (size_t i = 0; i < cache->size; i++) {
        if (cache->nodes[i].empty == 1) {
            set_cache_node(&(cache->nodes[i]), url);
            return i;
        }
    }
    size_t index = (++(cache->last_node_set)) % cache->size;
    set_cache_node(&(cache->nodes[index]), url);
    return index;
}
