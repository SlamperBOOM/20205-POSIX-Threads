#include <malloc.h>
#include <string.h>
#include "http_socket.h"

#define BUF_SIZE 4096


int init_clients(struct client* clients, size_t size) {
    for (int i = 0; i < size; i++) {
        setup_client(i, clients);
        if (init_client(&clients[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

int init_client(struct client* client) {
    client->request.buf = (char*) malloc(client->request.buf_size * sizeof(char));
    if (client->request.buf == NULL) {
        perror("malloc failed");
        return -1;
    }
    client->request.headers = (struct phr_header*) malloc(client->request.headers_max_size * sizeof(*client->request.headers));
    if (client->request.headers == NULL) {
        perror("malloc failed");
        return -1;
    }
    return 0;
}

void setup_client(size_t index, struct client* clients) {
    clients[index].fd = -1;
    clients[index].processed = 0;
    clients[index].request.buf_size = BUF_SIZE;
    clients[index].request.headers_max_size = 100;
    clients[index].request.buf_len = 0;
    clients[index].request.prev_buf_len = 0;
    clients[index].bytes_written = 0;
    clients[index].response = NULL;
}

int init_servers(struct server* servers, size_t size) {
    for (int i = 0; i < size; i++) {
        if (setup_server(i, servers) != 0) {
            return -1;
        }
    }
    return 0;
}

int setup_server(size_t index, struct server* servers) {
    servers[index].fd = -1;
    servers[index].processed = 0;
    servers[index].bytes_written = 0;
    servers[index].response = (struct response*) malloc(sizeof(struct response));
    if (servers[index].response == NULL) {
        perror("malloc failed");
        return -1;
    }
    servers[index].response->in_cache = 0;
    servers[index].response->clients_num = 0;
    servers[index].response->buf_size = BUF_SIZE;
    servers[index].response->buf_len = 0;
    servers[index].response->content_length = -1;
    servers[index].response->not_content_length = -1;
    servers[index].response->buf = (char*) malloc(sizeof(char) * servers[index].response->buf_size);
    if (servers[index].response->buf == NULL) {
        perror("malloc failed");
        return -1;
    }
    servers[index].request = NULL;
    servers[index].response->headers_max_size = 100;
    servers[index].response->headers = (struct phr_header*) malloc(servers[index].response->headers_max_size * sizeof(struct phr_header));
    if (servers[index].response->headers == NULL) {
        perror("malloc failed");
        return -1;
    }
    servers[index].response->clients_max_size = 100;
    servers[index].response->clients = (struct client**) malloc(sizeof(struct client*) * servers[index].response->clients_max_size);
    if (servers[index].response->clients == NULL) {
        perror("malloc failed");
        return -1;
    }
    return 0;
}

void free_clients(struct client* clients, size_t clients_num) {
    for (int i = 0; i < clients_num; i++) {
        if (clients[i].request.buf != NULL) {
            free(clients[i].request.buf);
            free(clients[i].request.headers);
        }
    }
}

void free_servers(struct server* servers, size_t servers_num) {
    for (int i = 0; i < servers_num; i++) {
        if (servers[i].response != NULL && servers[i].response->in_cache == 0) {
            free(servers[i].response->buf);
            free(servers[i].response->headers);
            free(servers[i].response->clients);
            free(servers[i].response);
        }
    }
}

int add_fd_to_clients(int fd, size_t poll_index, struct client* clients, size_t clients_size) {
    for (int i = 0; i < clients_size; i++) {
        if (clients[i].fd == -1) {
            printf("Add fd to clients %d\n", i);
            clients[i].fd = fd;
            clients[i].processed = 1;
            clients[i].poll_index = poll_index;
            return i;
        }
    }
    return -1;
}

int add_fd_to_servers(int fd, struct sockaddr_in serv_addr,
                      size_t poll_index, struct request* request,
                      struct server* servers, size_t servers_size) {
    for (int i = 0; i < servers_size; i++) {
        if (servers[i].fd == -1) {
            servers[i].fd = fd;
            servers[i].processed = 1;
            servers[i].serv_addr = serv_addr;
            servers[i].poll_index = poll_index;
            servers[i].request = request;
            return i;
        }
    }
    return -1;
}

ssize_t get_header_value(char** value, size_t* value_len, char* header_name,
                         struct phr_header* headers, size_t num_headers) {
    for (size_t i = 0; i < num_headers; i++) {
        if (strncmp(headers[i].name, header_name, headers[i].name_len) == 0) {
            *value_len = headers[i].value_len;
            *value = (char*) malloc(sizeof(char) * *value_len);
            if (*value == NULL) {
                perror("malloc");
                return 2;
            }
            strncpy(*value, headers[i].value, *value_len);
            return 0;
        }
    }
    *value = NULL;
    return 1;
}
