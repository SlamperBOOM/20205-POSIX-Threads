#include <malloc.h>
#include <string.h>
#include "http_socket.h"

#define BUF_SIZE 4096

void setup_client(int index, struct client* clients) {
    clients[index].fd = -1;
    clients[index].processed = 0;
    clients[index].request.buf_size = BUF_SIZE;
    clients[index].request.buf_len = 0;
    clients[index].request.prev_buf_len = 0;
    clients[index].bytes_written = 0;
    clients[index].response = NULL;
}

int setup_server(int index, struct server* servers) {
    servers[index].fd = -1;
    servers[index].processed = 0;
    servers[index].bytes_written = 0;
    servers[index].response = (struct response*) malloc(sizeof(struct response));
    if (servers[index].response == NULL) {
        perror("malloc failed");
        return 1;
    }
    servers[index].response->clients_num = 0;
    servers[index].response->buf_size = BUF_SIZE;
    servers[index].response->buf_len = 0;
    servers[index].response->content_length = -1;
    servers[index].response->not_content_length = -1;
    servers[index].response->buf = (char*) malloc(sizeof(char) * servers[index].response->buf_size);
    if (servers[index].response->buf == NULL) {
        perror("malloc failed");
        return 1;
    }
    servers[index].request = NULL;
    return 0;
}

void free_clients(struct client* clients, size_t servers_num) {
    for (int i = 0; i < servers_num; i++) {
        if (clients[i].request.buf != NULL) {
            free(clients[i].request.buf);
        }
    }
}


int add_fd_to_clients(int fd, size_t poll_index, struct client* clients, size_t clients_size) {
    for (int i = 0; i < clients_size; i++) {
        if (clients[i].fd == -1) {
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