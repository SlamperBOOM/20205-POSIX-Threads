#include <malloc.h>
#include <string.h>
#include "http_socket.h"

#define BUF_SIZE 4096

int parse_request(struct request* request) {
    return phr_parse_request(request->buf,
                             request->buf_len,
                             (const char**) &request->method,
                             &request->method_len,
                             (const char**) &request->path,
                             &request->path_len,
                             &request->minor_version,
                             request->headers,
                             &request->num_headers,
                             request->prev_buf_len);
}

int parse_response(struct response* response) {
    return phr_parse_response(response->buf,
                              response->buf_len,
                              &response->minor_version,
                              &response->status,
                              (const char**) &response->message,
                              &response->message_len,
                              response->headers,
                              &response->num_headers,
                              response->prev_buf_len);
}

int init_clients(struct client* clients, size_t size) {
    for (int i = 0; i < size; i++) {
        if (init_client(clients + i) != 0) {
            return -1;
        }
    }
    return 0;
}

int init_client(struct client* client) {
    setup_client(client);
    client->request.buf = (char*) malloc(client->request.buf_size * sizeof(char));
    if (client->request.buf == NULL) {
        return -1;
    }
    client->request.headers = (struct phr_header*) malloc(
            client->request.headers_max_size * sizeof(*client->request.headers));
    if (client->request.headers == NULL) {
        return -1;
    }
    return 0;
}

void setup_client(struct client* client) {
    client->fd = -1;
    client->cache_node = -1;
    client->processed = 0;
    client->request.buf_size = BUF_SIZE;
    client->request.headers_max_size = 100;
    client->request.buf_len = 0;
    client->request.prev_buf_len = 0;
    client->bytes_written = 0;
    client->response = NULL;
}

void init_servers(struct server* servers, size_t size) {
    for (int i = 0; i < size; i++) {
        setup_server(i, servers);
    }
}

void setup_server(size_t index, struct server* servers) {
    servers[index].fd = -1;
    servers[index].processed = 0;
    servers[index].bytes_written = 0;
    servers[index].request = NULL;
    servers[index].response = NULL;
}

void free_clients(struct client* clients, size_t clients_num) {
    for (int i = 0; i < clients_num; i++) {
        if (clients[i].request.buf != NULL) {
            free(clients[i].request.buf);
            free(clients[i].request.headers);
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
                return 2;
            }
            strncpy(*value, headers[i].value, *value_len);
            return 0;
        }
    }
    *value = NULL;
    return 1;
}
