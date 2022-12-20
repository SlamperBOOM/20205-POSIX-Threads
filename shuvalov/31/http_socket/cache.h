#include <stdio.h>
#include "http_socket.h"

#ifndef INC_31_CACHE_H
#define INC_31_CACHE_H


struct cache_node {
    struct response* response;
    char* url;
    int empty;
};

struct cache {
    struct cache_node* nodes;
    int last_node_set;
    size_t size;
};

void subscribe(struct client* client, struct cache* cache, size_t cache_index);

void make_publisher(struct server* server, struct cache* cache, size_t cache_index);

void unsubscribe(struct client* client);

struct client* get_subscriber(struct server server, size_t index);

size_t get_subscribers_count(struct server server);

void free_cache(struct cache cache);

void free_cache_node(struct cache_node cache_node);

int init_cache(struct cache* cache, size_t size);

int init_cache_node(struct cache_node* cache_node);

struct cache_node* get(struct cache cache, char* url);

size_t set(struct cache* cache, char* url);

#endif //INC_31_CACHE_H
