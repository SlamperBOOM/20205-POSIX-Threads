#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <limits.h>
#include "picohttpparser/picohttpparser.h"
#include "socket_operations/socket_operations.h"
#include "hashmap/hashmap.h"

#define SA struct sockaddr
#define SERVER_PORT 80
#define PROXY_PORT 8080
#define PROXY_IP "0.0.0.0"
#define BUF_SIZE 4096
#define MAX_FDS 10
#define USAGE "proxy IP PORT"

long proxy_port;
char* proxy_ip;
struct hashmap* cache;

extern int errno;

struct request {
    char* buf, * method, * path;
    struct phr_header headers[100];
    size_t buf_size, buf_len, prev_buf_len, method_len, path_len, num_headers;
    int minor_version;
};

struct response {
    struct client* clients[10];
    char* buf, * message;
    struct phr_header headers[100];
    size_t buf_size, buf_len, prev_buf_len, message_len, num_headers;
    int minor_version, status, clients_num;
};

struct cached_response {
    struct response* response;
    char* url;
};

//struct cache {
//    struct hashmap* map;
//};

struct client {
    struct response* response;
    struct request request;
    size_t poll_index;
    ssize_t bytes_written;
    int fd, processed;
};

struct server {
    struct client* clients[10];
    struct request* request;
    struct response* response;
    struct sockaddr_in serv_addr;
    size_t poll_index;
    ssize_t bytes_written;
    int clients_num, fd, processed;
};

ssize_t get_hostname(char** hostname, size_t* hostname_len, struct phr_header* headers, size_t num_headers) {
    for (size_t i = 0; i < num_headers; i++) {
        if (strncmp(headers[i].name, "Host", headers[i].name_len) == 0) {
            *hostname_len = headers[i].value_len;
            *hostname = (char*) malloc(sizeof(char) * *hostname_len);
            if (*hostname == NULL) {
                perror("malloc");
                return 1;
            }
            strncpy(*hostname, headers[i].value, *hostname_len);
            return 0;
        }
    }
    free(*hostname);
    *hostname = NULL;
    return 1;
}

int add_fd_to_poll(int fd, short events, struct pollfd* poll_fds, size_t poll_fds_num) {
    for (int i = 0; i < poll_fds_num; i++) {
        if (poll_fds[i].fd == -1) {
            poll_fds[i].fd = fd;
            poll_fds[i].events = events;
            return i;
        }
    }
    return -1;
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
            servers[i].processed = 0;
            servers[i].serv_addr = serv_addr;
            servers[i].poll_index = poll_index;
            servers[i].request = request;
            return i;
        }
    }
    return -1;
}

int remove_from_poll(int fd, struct pollfd* poll_fds, size_t poll_fds_num) {
    for (int i = 0; i < poll_fds_num; i++) {
        if (poll_fds[i].fd == fd) {
            poll_fds[i].fd = -1;
            return i;
        }
    }
    return -1;
}

void close_all(struct pollfd* poll_fds, size_t poll_fds_num) {
    for (int i = 0; i < poll_fds_num; i++) {
        if (poll_fds[i].fd != -1) {
            close(poll_fds[i].fd);
        }
    }
}

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
    servers[index].clients_num = 0;
    servers[index].bytes_written = 0;
    servers[index].response = (struct response*) malloc(sizeof(struct response));
    if (servers[index].response == NULL) {
        perror("malloc failed");
        return 1;
    }
    servers[index].response->clients_num = 0;
    servers[index].response->buf_size = BUF_SIZE;
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

void free_servers(struct server* servers, size_t servers_num) {
//    for (int i = 0; i < servers_num; i++) {
//        if (servers[i].response->buf != NULL) {
//            free(servers[i].response->buf);
//        }
//    }
}

void close_client(int fd, struct pollfd* poll_fds, size_t poll_fds_num, struct client* clients, int client_index) {
    if (fd >= 0) {
        close(fd);
        remove_from_poll(fd, poll_fds, poll_fds_num);
        if (client_index < 0) {
            for (int i = 0; i < poll_fds_num; i++) {
                if (clients[i].fd == fd) {
                    setup_client(i, clients);
                    break;
                }
            }
        } else {
            setup_client(client_index, clients);
        }
    }
}

void close_server(int fd, struct pollfd* poll_fds, size_t poll_fds_num, struct server* servers, int server_index) {
    close(fd);
    remove_from_poll(fd, poll_fds, poll_fds_num);
    setup_server(server_index, servers);
}

int receive_from_client(struct client* client, int client_index, struct pollfd* poll_fds, size_t poll_fds_num,
                        struct client* clients,
                        struct server* servers, size_t servers_size) {
    ssize_t return_value;
    int parse_result;
    while ((return_value = read(client->fd,
                                client->request.buf + client->request.buf_len,
                                client->request.buf_size - client->request.buf_len)) == -1 &&
           errno == EINTR);
    if (return_value < 0) {
        perror("read");
        close_client(client->fd, poll_fds, poll_fds_num, clients, client_index);
        return 1;
    } else if (return_value > 0) { //TODO handle 0 correctly
        client->request.prev_buf_len = client->request.buf_len;
        client->request.buf_len += return_value;
        client->request.num_headers = sizeof(client->request.headers) / sizeof(client->request.headers[0]);
        parse_result = phr_parse_request(client->request.buf,
                                         client->request.buf_len,
                                         (const char**) &client->request.method,
                                         &client->request.method_len,
                                         (const char**) &client->request.path,
                                         &client->request.path_len,
                                         &client->request.minor_version,
                                         client->request.headers,
                                         &client->request.num_headers,
                                         client->request.prev_buf_len);
        if (parse_result == -1) {
            fprintf(stderr, "request is too long\n");
            close_client(client->fd, poll_fds, poll_fds_num, clients, client_index);
            return 1;
        }
        /* request is incomplete, continue the loop */
        if (parse_result == 2) {
            printf("Have read now: %zd\n", return_value);
            return 0;
        }
        if (client->request.buf_len == client->request.buf_size) {
            fprintf(stderr, "request is too long\n");
            close_client(client->fd, poll_fds, poll_fds_num, clients, client_index);
            return 1;
        }
        printf("Request length: %zu\n", client->request.buf_len);
        printf("\nRequest:\n%.*s\n", (int) client->request.buf_len, client->request.buf);
        if (strncmp("GET", client->request.method, client->request.method_len) != 0) {
            fprintf(stderr, "Method %.*s is not implemented\n",
                    (int) client->request.method_len, client->request.method);
//                        char* client_response = "501 Not Implemented";
//                        write_all(client->fd, client_response, strlen(client_response));
            close_client(client->fd, poll_fds, poll_fds_num, clients, client_index);
            return 1;
        }

        struct cached_response* cached_response = hashmap_get(cache, &(struct cached_response){ .url = client->request.path});
        if (cached_response != NULL) {
            cached_response->response->clients[cached_response->response->clients_num++] = client;
            client->response = cached_response->response;
            if (cached_response->response->buf_len > 0) {
                poll_fds[client->poll_index].events = POLLOUT;
            } else {
                poll_fds[client->poll_index].events = 0;
            }
            printf("Client events: %d\n", poll_fds[client->poll_index].events);
            return 0;
        }
        char* hostname;
        size_t hostname_len;
        return_value = get_hostname(&hostname, &hostname_len,
                                    client->request.headers, client->request.num_headers);
        if (return_value == 1) {
            fprintf(stderr, "get_hostname failed\n");
            return 1;
        }
        char ip[100];
        return_value = hostname_to_ip(hostname, ip);
        free(hostname);
        if (return_value == 1) {
            close_client(client->fd, poll_fds, poll_fds_num, clients, client_index);
            return 1;
        }
        printf("IP: %s\n", ip);
        int server_fd;
        struct sockaddr_in serv_addr;
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) {
            printf("socket creation failed...\n");
            close_client(client->fd, poll_fds, poll_fds_num, clients, client_index);
            return 1;
        }
        printf("Socket successfully created..\n");
        bzero(&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(ip);
        serv_addr.sin_port = htons(SERVER_PORT);
        int poll_index = add_fd_to_poll(server_fd, POLLOUT, poll_fds, poll_fds_num);
        int server_index;
        if (poll_index >= 0) {
            server_index = add_fd_to_servers(server_fd, serv_addr, poll_index,
                                             &client->request, servers, servers_size);
        }
        if (poll_index < 0 || server_index < 0) {
            fprintf(stderr, "too many connections\n");
            return 1;
        }
//        servers[server_index].clients[servers[server_index].clients_num++] = client;
        servers[server_index].response->clients[servers[server_index].response->clients_num++] = client;
        hashmap_set(cache,
                    &(struct cached_response) {.url = client->request.path, .response = servers[server_index].response});
        client->response = servers[server_index].response;
        poll_fds[client->poll_index].events = 0;
    }
    return 0;
}

int send_to_client(struct client* client, int client_index, struct pollfd* poll_fds, size_t poll_fds_num,
                   struct client* clients) {
    ssize_t return_value;
    while ((return_value = write(client->fd, client->response->buf + client->bytes_written,
                                 client->response->buf_len - client->bytes_written)) == -1 &&
           errno == EINTR);
    if (return_value < 0) {
        perror("read");
        close(client->fd);
        remove_from_poll(client->fd, poll_fds, poll_fds_num);
        setup_client(client_index, clients);
        return 1;
    }
    client->bytes_written += return_value;
    poll_fds[client->poll_index].events = 0;
    if (return_value == 0 || client->response->buf_len == client->bytes_written) {
        printf("Send request to the client\n");
        printf("Bytes written: %zd\n", client->bytes_written);
        close(client->fd);
        remove_from_poll(client->fd, poll_fds, poll_fds_num);
        setup_client(client_index, clients);
    }
    return 0;
}

int process_clients(struct pollfd* poll_fds, size_t poll_fds_num, struct client* clients, size_t clients_size,
                    struct server* servers, size_t servers_size) {
    for (int i = 0; i < clients_size; i++) {
        struct client* client = &clients[i];
        if (client->fd != -1 && !client->processed) {
            if ((poll_fds[client->poll_index].revents & POLLIN) == POLLIN) {
                receive_from_client(client, i, poll_fds, poll_fds_num, clients, servers, servers_size);
            } else if ((poll_fds[client->poll_index].revents & POLLOUT) == POLLOUT) {
                send_to_client(client, i, poll_fds, poll_fds_num, clients);
            }
        }
    }
    return 0;
}

int connect_to_server(struct server* server, int server_index, struct pollfd* poll_fds, size_t poll_fds_num,
                      struct server* servers) {
    if (connect(server->fd,
                (SA*) &server->serv_addr,
                sizeof(server->serv_addr)) != 0) {
        printf("connection with the server failed...\n");
        remove_from_poll(server->fd, poll_fds, poll_fds_num);
        close(server->fd);
        setup_server(server_index, servers);
    } else {
        printf("Connected to the server\n");
        server->processed = 1;
    }
    return 0;
}

int send_to_server(struct server* server, int server_index, struct pollfd* poll_fds, size_t poll_fds_num,
                   struct server* servers) {
    printf("buf_len: %zd\n", server->request->buf_len);
    ssize_t return_value;
    while ((return_value = write(server->fd, server->request->buf + server->bytes_written,
                                 server->request->buf_len - server->bytes_written)) == -1 &&
           errno == EINTR);
    if (return_value < 0) {
        perror("read");
        close_server(server->fd, poll_fds, poll_fds_num, servers, server_index);
        return 1;
    } else if (return_value == 0) {
        printf("Send request to the server\n");
        printf("Bytes written: %zd\n", server->bytes_written);
        poll_fds[server->poll_index].events = POLLIN;
    }
    server->bytes_written += return_value;
    return 0;
}

int receive_from_server(struct server* server, int server_index, struct pollfd* poll_fds, size_t poll_fds_num,
                        struct client* clients,
                        struct server* servers) {
    //TODO just in the moment when proxy read servers response
    // we have to write it to the clients

    ssize_t return_value;
    while ((return_value = read(server->fd, server->response->buf + server->response->buf_len,
                                server->response->buf_size - server->response->buf_len)) == -1 &&
           errno == EINTR);
    if (return_value < 0) {
        perror("read");
        for (int k = 0; k < server->response->clients_num; k++) {
            close_client(server->response->clients[k]->fd, poll_fds, poll_fds_num, clients, -1);
        }
        close_server(server->fd, poll_fds, poll_fds_num, servers, server_index);
        return 1;
    } else if (return_value == 0) { //TODO handle correctly
        for (int k = 0; k < server->response->clients_num; k++) {
            printf("Client POLLOUT\n");
            poll_fds[server->response->clients[k]->poll_index].events = POLLOUT;
            close_server(server->fd, poll_fds, poll_fds_num, servers, server_index);
        }
    } else {
        server->response->prev_buf_len = server->response->buf_len;
        server->response->buf_len += return_value;
        server->response->num_headers = sizeof(server->response->headers) /
                                       sizeof(server->response->headers[0]);
        int parse_result = phr_parse_response(server->response->buf,
                                              server->response->buf_len,
                                              &server->response->minor_version,
                                              &server->response->status,
                                              (const char**) &server->response->message,
                                              &server->response->message_len,
                                              server->response->headers,
                                              &server->response->num_headers,
                                              server->response->prev_buf_len
        );

        if (parse_result == -1) {
            fprintf(stderr, "response is too long\n");
            for (int k = 0; k < server->response->clients_num; k++) {
                close_client(server->response->clients[k]->fd, poll_fds, poll_fds_num, clients, -1);
            }
            close_server(server->fd, poll_fds, poll_fds_num, servers, server_index);
            return 1;
        }
        if (parse_result == 2) {
            printf("Read now: %zd\n", return_value);
            for (int k = 0; k < server->response->clients_num; k++) {
                printf("Client POLLOUT\n");
                poll_fds[server->response->clients[k]->poll_index].events = POLLOUT;
            }
        } else {
            if (server->response->buf_len == server->response->buf_size) {
                fprintf(stderr, "response is too long\n");
                for (int k = 0; k < server->response->clients_num; k++) {
                    close_client(server->response->clients[k]->fd, poll_fds, poll_fds_num, clients, -1);
                }
                close_server(server->fd, poll_fds, poll_fds_num, servers, server_index);
                return 1;
            }
            for (int k = 0; k < server->response->clients_num; k++) {
                printf("Client POLLOUT\n");
                poll_fds[server->response->clients[k]->poll_index].events = POLLOUT;
            }
            printf("Response length: %zu\n", server->response->buf_len);
            printf("\nResponse:\n%.*s\n", (int) server->response->buf_len, server->response->buf);
            close_server(server->fd, poll_fds, poll_fds_num, servers, server_index);
        }
    }
    return 0;
}

int process_servers(struct pollfd* poll_fds, size_t poll_fds_num, struct client* clients,
                    struct server* servers, size_t servers_size) {
    for (int i = 0; i < servers_size; i++) {
        struct server* server = &servers[i];
        if (server->fd != -1 && !server->processed) {
            if ((poll_fds[server->poll_index].revents & (POLLOUT | POLLHUP)) == (POLLOUT | POLLHUP)) {
                printf("connect_to_server\n");
                connect_to_server(server, i, poll_fds, poll_fds_num, servers);
            } else if ((poll_fds[server->poll_index].revents & POLLOUT) == POLLOUT) {
                printf("send_to_server\n");
                send_to_server(server, i, poll_fds, poll_fds_num, servers);
            } else if ((poll_fds[server->poll_index].revents & POLLIN) == POLLIN) {
                printf("receive_from_server\n");
                receive_from_server(server, i, poll_fds, poll_fds_num, clients, servers);
            }
        }
    }
    return 0;
}

int
accept_client(struct pollfd* poll_fds, size_t poll_fds_num, struct client* clients, size_t clients_size, int proxy_fd) {
    if (poll_fds[0].revents & POLLIN) {
        if (poll_fds[0].fd == proxy_fd) {
            int client_fd = accept(proxy_fd, NULL, NULL);
            if (client_fd < 0) {
                perror("accept failed");
                return 1;
            } else {
                printf("Proxy accepted the client...\n");
            }
            int poll_index = add_fd_to_poll(client_fd, POLLIN, poll_fds, poll_fds_num);
            int client_index;
            if (poll_index >= 0) {
                client_index = add_fd_to_clients(client_fd, poll_index, clients, clients_size);
            }
            if (poll_index < 0 || client_index < 0) {
                fprintf(stderr, "too many connections\n");
                return 1;
            }
        }
    }
    return 0;
}

int cached_response_compare(const void* a, const void* b, void* udata) {
    const struct cached_response* cra = a;
    const struct cached_response* crb = b;
    return strcmp(cra->url, crb->url);
}

uint64_t cached_response_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const struct cached_response* cached_response = item;
    return hashmap_sip(cached_response->url, strlen(cached_response->url), seed0, seed1);
}

//TODO handle errors in process_servers
// add cache

int main(int argc, char* argv[]) {
    int error = 0;
    if (argc >= 3) {
        proxy_ip = argv[1];
        proxy_port = strtol(argv[2], NULL, 0);
        if (proxy_port == LONG_MIN || proxy_port == LONG_MAX) {
            perror("strtol failed");
            printf(USAGE);
            return EXIT_FAILURE;
        }
    } else {
        proxy_port = PROXY_PORT;
        proxy_ip = PROXY_IP;
    }
    printf("IP address: %s\n", proxy_ip);
    printf("Port: %ld\n\n", proxy_port);
    int proxy_fd;
    struct sockaddr_in proxyaddr;
    proxy_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_fd == -1) {
        perror("socket creation failed");
        return EXIT_FAILURE;
    } else {
        printf("Socket successfully created..\n");
    }
    bzero(&proxyaddr, sizeof(proxyaddr));
    proxyaddr.sin_family = AF_INET;
    proxyaddr.sin_addr.s_addr = inet_addr(PROXY_IP);
    proxyaddr.sin_port = htons(PROXY_PORT);
    if ((bind(proxy_fd, (SA*) &proxyaddr, sizeof(proxyaddr))) != 0) {
        perror("socket bind failed");
        close(proxy_fd);
        return EXIT_FAILURE;
    } else {
        printf("Socket successfully bound..\n");
    }
    if ((listen(proxy_fd, SOMAXCONN)) != 0) {
        perror("listen failed");
        close(proxy_fd);
        return EXIT_FAILURE;
    } else {
        printf("Proxy listening..\n");
    }
    int poll_fds_num = MAX_FDS;
    int clients_size = poll_fds_num;
    int servers_size = poll_fds_num;
    struct pollfd poll_fds[poll_fds_num];
    struct client clients[poll_fds_num];
    struct server servers[poll_fds_num];
    for (int i = 0; i < poll_fds_num; i++) {
        poll_fds[i].fd = -1;
        setup_client(i, clients);
        clients[i].request.buf = (char*) malloc(clients[i].request.buf_size * sizeof(char));
        if (clients[i].request.buf == NULL) {
            perror("malloc failed");
            error = 1;
            goto CLEANUP;
        }

//        servers[i].response->buf = (char*) malloc(servers[i].response->buf_size * sizeof(char));
        if (setup_server(i, servers) == 1) {
            error = 1;
            goto CLEANUP;
        }
    }
    cache = hashmap_new(sizeof(struct cached_response), 0, 0, 0,
                        cached_response_hash, cached_response_compare, NULL, NULL);

    poll_fds[0].fd = proxy_fd;
    poll_fds[0].events = POLLIN;
    int iter = 0;
    while (iter++ < 1000) {
//    while (1) {
        printf("%d.\n", iter++);
        int poll_return = poll(poll_fds, poll_fds_num, -1);
        if (poll_return == -1) {
            perror("poll failed");
            error = 1;
            goto FREE_CACHE;
        }
        printf("Ready descriptors number: %d\nReady descriptors revents: ", poll_return);
        for (int i = 0; i < poll_fds_num; i++) {
            printf("%d ", poll_fds[i].revents);
        }
        printf("\n");
        if (accept_client(poll_fds, poll_fds_num, clients, clients_size, proxy_fd) == 1) {
            fprintf(stderr, "accept_client failed\n");
        }
        printf("Client accepted\n");
        process_clients(poll_fds, poll_fds_num, clients, clients_size, servers, servers_size);
        printf("Clients processed\n");
        process_servers(poll_fds, poll_fds_num, clients, servers, servers_size);
        printf("Servers processed\n");

        for (int i = 0; i < poll_fds_num; i++) {
            servers[i].processed = 0;
            clients[i].processed = 0;
        }
    }

    FREE_CACHE:
    hashmap_free(cache);
    CLEANUP:
    free_servers(servers, servers_size);
    free_clients(clients, clients_size);
    close_all(poll_fds, poll_fds_num);
    if (error) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
