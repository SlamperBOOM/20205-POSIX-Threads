#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include "picohttpparser-master/picohttpparser.h"
#include "socket_operations/socket_operations.h"

#define SA struct sockaddr
#define SERVER_PORT 80
#define PROXY_PORT 8080
#define PROXY_IP "127.0.0.1"
#define BUF_SIZE 4096
#define CONNECTED 4
#define READ 0
#define WRITE 1
#define WAIT 3
#define MAX_FDS 10

extern int errno;

struct request {
    char* buf;
    char* method;
    char* path;
    struct phr_header headers[100];
    size_t buf_size, buf_len, prev_buf_len, method_len, path_len, num_headers;
    int minor_version;
};

struct response {
    char* buf;
    char* method;
    char* path;
    struct phr_header headers[100];
    size_t buf_size, buf_len, prev_buf_len, method_len, path_len, num_headers;
    int minor_version;
};

struct client {
    struct request request;
    size_t poll_index;
    int fd;
    int processed;
    int state;
};

struct server {
    struct request* request;
    struct response response;
    struct sockaddr_in serv_addr;
    size_t poll_index;
    int fd;
    int connected;
    int processed;
    int state;
};

void get_hostname(char** hostname, size_t* hostname_len, struct phr_header* headers, size_t num_headers) {
    for (size_t i = 0; i < num_headers; i++) {
        if (strncmp(headers[i].name, "Host", headers[i].name_len) == 0) {
            *hostname_len = headers[i].value_len;
            *hostname = (char*) malloc(sizeof(char) * *hostname_len);
            strncpy(*hostname, headers[i].value, *hostname_len);
            return;
        }
    }
    *hostname = NULL;
}

int add_fd_to_poll(int fd, short events, struct pollfd* poll_fds, int poll_fds_num) {
    for (int i = 0; i < poll_fds_num; i++) {
        if (poll_fds[i].fd == -1) {
            poll_fds[i].fd = fd;
            poll_fds[i].events = events;
            return i;
        }
    }
    return -1;
}

int add_fd_to_clients(int fd, size_t poll_index, struct client* clients, int clients_size) {
    for (int i = 0; i < clients_size; i++) {
        if (clients[i].fd == -1) {
            clients[i].fd = fd;
            clients[i].processed = 1;
            clients[i].state = READ;
            clients[i].poll_index = poll_index;
            return i;
        }
    }
    return -1;
}

int add_fd_to_servers(int fd, struct sockaddr_in serv_addr,
                      size_t poll_index, struct request* request,
                      struct server* servers, int servers_size) {
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

int remove_from_poll(int fd, struct pollfd* poll_fds, int poll_fds_num) {
    for (int i = 0; i < poll_fds_num; i++) {
        if (poll_fds[i].fd == fd) {
            poll_fds[i].fd = -1;
            return i;
        }
    }
    return -1;
}

void close_all(struct pollfd* poll_fds, int poll_fds_num) {
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
}

void setup_server(int index, struct server* servers) {
    servers[index].fd = -1;
    servers[index].connected = 0;
    servers[index].processed = 0;
    servers[index].response.buf_size = BUF_SIZE;
    servers[index].response.buf_len = 0;
    servers[index].response.prev_buf_len = 0;
}

void free_clients(struct client* clients, int servers_num) {
    for (int i = 0; i < servers_num; i++) {
        free(clients[i].request.buf);
    }
}

void free_servers(struct server* servers, int servers_num) {
    for (int i = 0; i < servers_num; i++) {
        free(servers[i].response.buf);
    }
}

int main(int argc, char* argv[]) {
    int proxy_fd;
    struct sockaddr_in proxyaddr;

    // socket create and verification
    proxy_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_fd == -1) {
        perror("socket creation failed");
        return EXIT_FAILURE;
    } else {
        printf("Socket successfully created..\n");
    }
    bzero(&proxyaddr, sizeof(proxyaddr));

    // assign IP, SERVER_PORT
    proxyaddr.sin_family = AF_INET;
    proxyaddr.sin_addr.s_addr = inet_addr(PROXY_IP);
    proxyaddr.sin_port = htons(PROXY_PORT);

    // Binding newly created socket to given IP and verification
    if ((bind(proxy_fd, (SA*) &proxyaddr, sizeof(proxyaddr))) != 0) {
        printf("socket bind failed...\n");
        close(proxy_fd);
        return EXIT_FAILURE;
    } else {
        printf("Socket successfully bound..\n");
    }
    printf("\n");

    // Now server is ready to listen and verification
    if ((listen(proxy_fd, SOMAXCONN)) != 0) {
        perror("listen failed");
        close(proxy_fd);
        return EXIT_FAILURE;
    } else {
        printf("Server listening..\n");
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
        setup_server(i, servers);
        servers[i].response.buf = (char*) malloc(servers[i].response.buf_size * sizeof(char));
    }
    poll_fds[0].fd = proxy_fd;
    poll_fds[0].events = POLLIN;

    int iter = 0;
    while (iter++ < 5) {
        printf("%d.\n", iter);

        int poll_return = poll(poll_fds, poll_fds_num, -1);
        if (poll_return == -1) {
            perror("poll");
            close_all(poll_fds, poll_fds_num);
            free_servers(servers, servers_size);
            free_clients(clients, clients_size);
            return EXIT_FAILURE;
        }
        printf("Ready descriptors: %d\n", poll_return);
        for (int i = 0; i < poll_fds_num; i++) {
            printf("%d ", poll_fds[i].revents);
        }

        if (poll_fds[0].revents & POLLIN) {
            if (poll_fds[0].fd == proxy_fd) {
                int client_fd = accept(proxy_fd, NULL, NULL);
                if (client_fd < 0) {
                    perror("server accept failed");
                    close_all(poll_fds, poll_fds_num);
                    free_servers(servers, servers_size);
                    free_clients(clients, clients_size);
                    return EXIT_FAILURE;
                } else {
                    printf("Server accepted the client...\n");
                }
                int poll_index = add_fd_to_poll(client_fd, POLLIN, poll_fds, poll_fds_num);
                if (poll_index >= 0) {
                    add_fd_to_clients(client_fd, poll_index, clients, clients_size);
                }
            }
        }

        for (int i = 0; i < clients_size; i++) {
            if (clients[i].fd != -1 && !clients[i].processed) {
                if (clients[i].state == READ && (poll_fds[clients[i].poll_index].revents & POLLIN) == POLLIN) {
                    ssize_t rret;
                    int pret;
                    while ((rret = read(clients[i].fd, clients[i].request.buf + clients[i].request.buf_len,
                                        clients[i].request.buf_size - clients[i].request.buf_len)) == -1 &&
                           errno == EINTR);
                    if (rret < 0) {
                        perror("read");
                        close_all(poll_fds, poll_fds_num);
                        free_servers(servers, servers_size);
                        free_clients(clients, clients_size);
                        return EXIT_FAILURE;
                    } else if (rret == 0) {
                        continue;
                    }

                    clients[i].request.prev_buf_len = clients[i].request.buf_len;
                    clients[i].request.buf_len += rret;
                    /* parse the request */
                    clients[i].request.num_headers = sizeof(clients[i].request.headers) /
                                                     sizeof(clients[i].request.headers[0]);
                    pret = phr_parse_request(clients[i].request.buf, clients[i].request.buf_len,
                                             (const char**) &clients[i].request.method,
                                             &clients[i].request.method_len,
                                             (const char**) &clients[i].request.path,
                                             &clients[i].request.path_len,
                                             &clients[i].request.minor_version,
                                             clients[i].request.headers,
                                             &clients[i].request.num_headers,
                                             clients[i].request.prev_buf_len);

                    if (pret == -1) {
                        close_all(poll_fds, poll_fds_num);

                        fprintf(stderr, "request is too long\n");
                        free_servers(servers, servers_size);
                        free_clients(clients, clients_size);
                        return EXIT_FAILURE;
                    }
                    /* request is incomplete, continue the loop */
                    if (pret == 2) {
                        printf("Read now: %zd\n", rret);
                        continue;
                    }
                    if (clients[i].request.buf_len == clients[i].request.buf_size) {
                        fprintf(stderr, "request is too long\n");
                        close_all(poll_fds, poll_fds_num);
                        free_servers(servers, servers_size);
                        free_clients(clients, clients_size);
                        return EXIT_FAILURE;
                    }

                    printf("Request length: %zu\n", clients[i].request.buf_len);
                    printf("\nRequest:\n%.*s\n", (int) clients[i].request.buf_len, clients[i].request.buf);

                    if (strncmp("GET", clients[i].request.method, clients[i].request.method_len) != 0) {
                        fprintf(stderr, "Method %.*s is not implemented\n",
                                (int) clients[i].request.method_len, clients[i].request.method);
                        char* client_response = "501 Not Implemented";
                        write_all(clients[i].fd, client_response, strlen(client_response));
                        remove_from_poll(clients[i].fd, poll_fds, poll_fds_num);
                        close(clients[i].fd);
                        setup_client(i, clients);
                        continue;
                    }

                    char* hostname;
                    size_t hostname_len;
                    get_hostname(&hostname, &hostname_len,
                                 clients[i].request.headers, clients[i].request.num_headers);

                    char ip[100];
                    if (hostname_to_ip(hostname, ip) == 1) {
                        remove_from_poll(clients[i].fd, poll_fds, poll_fds_num);
                        close(clients[i].fd);
                        setup_client(i, clients);
                        free(hostname);
                        continue;
                    }
                    free(hostname);
                    printf("IP: %s\n", ip);

                    int server_fd;
                    struct sockaddr_in serv_addr;

                    // socket create and verification
                    server_fd = socket(AF_INET, SOCK_STREAM, 0);
                    if (server_fd == -1) {
                        printf("socket creation failed...\n");
                        close_all(poll_fds, poll_fds_num);
                        free_servers(servers, servers_size);
                        free_clients(clients, clients_size);
                        return EXIT_FAILURE;
                    } else {
                        printf("Socket successfully created..\n");
                    }
                    bzero(&serv_addr, sizeof(serv_addr));
                    // assign IP, SERVER_PORT
                    serv_addr.sin_family = AF_INET;
                    serv_addr.sin_addr.s_addr = inet_addr(ip);
                    serv_addr.sin_port = htons(SERVER_PORT);

                    int poll_index = add_fd_to_poll(server_fd, POLLOUT, poll_fds, poll_fds_num);
                    if (poll_index >= 0) {
                        add_fd_to_servers(server_fd, serv_addr, poll_index, &clients[i].request, servers, servers_size);
                    } else {
                        fprintf(stderr, "too much connections\n");
                    }
                    remove_from_poll(clients[i].fd, poll_fds, poll_fds_num);
                    clients[i].state = WAIT;
                }
            }
        }


        for (int i = 0; i < servers_size; i++) {
            struct server server = servers[i];
            if (server.fd != -1 && !server.processed) {
                if ((poll_fds[server.poll_index].revents & (POLLOUT | POLLHUP)) == (POLLOUT | POLLHUP)) {
                    if (connect(server.fd,
                                (SA*) &server.serv_addr,
                                sizeof(server.serv_addr)) != 0) {
                        printf("connection with the server failed...\n");
                        remove_from_poll(server.fd, poll_fds, poll_fds_num);
                        close(server.fd);
                        setup_server(i, servers);
                    } else {
                        printf("Connected to the server..\n");
                        server.processed = 1;
                    }
                } else if ((poll_fds[server.poll_index].revents & POLLOUT) == POLLOUT) {
                    //TODO go to poll after single write call
                    ssize_t bytes_written = write_all(server.fd,
                                                      server.request->buf,
                                                      server.request->buf_len);

//                        ssize_t rret;
//                        int pret;
//                        while ((rret = write(server.fd, clients[i].request.buf + clients[i].request.buf_len,
//                                            clients[i].request.buf_size - clients[i].request.buf_len)) == -1 &&
//                               errno == EINTR);
//                        if (rret < 0) {
//                            perror("read");
//                            close_all(poll_fds, poll_fds_num);
//                            free_servers(servers, servers_size);
//                            free_clients(clients, clients_size);
//                            return EXIT_FAILURE;
//                        } else if (rret == 0) {
//                            continue;
//                        }
//                        clients[i].request.prev_buf_len = clients[i].request.buf_len;
//                        clients[i].request.buf_len += rret;

                    if (bytes_written == -1) {
                        perror("write_all");
                        goto CLEANUP;
                    }
                    printf("Bytes written: %zd\n", bytes_written);
                    size_t receive_buf_len = 1024;
                    char* receive_buf = (char*) malloc(sizeof(char) * receive_buf_len);
                    ssize_t bytes_read = read_all(server.fd, receive_buf, receive_buf_len);
                    if (bytes_read == -1) {
                        perror("read_all");
                        free(receive_buf);
                        goto CLEANUP;
                    }
                    printf("\nBytes read: %zd\nResponse:\n%.*s\n\n",
                           bytes_read, (int) bytes_read, receive_buf);
                    bytes_written = write_all(clients[i].fd, receive_buf, bytes_read);
                    if (bytes_written == -1) {
                        perror("write_all");
                    }
                    free(receive_buf);
                    CLEANUP:
                    remove_from_poll(server.fd, poll_fds, poll_fds_num);
                    close(server.fd);
                    setup_server(i, servers);
                }
            }
        }

        for (int i = 0; i < poll_fds_num; i++) {
            servers[i].processed = 0;
            clients[i].processed = 0;
        }
    }

    free_servers(servers, servers_size);
    free_clients(clients, clients_size);
    close_all(poll_fds, poll_fds_num);
    return EXIT_SUCCESS;
}
