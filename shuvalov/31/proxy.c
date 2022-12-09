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
    struct pollfd* poll_fd;
    struct request request;
    int fd;
};

struct server {
    struct pollfd* poll_fd;
    struct request response;
    int fd;
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

int add_fd(int fd, short events, struct pollfd* poll_fds, int poll_fds_num) {
    for (int i = 0; i < poll_fds_num; i++) {
        if (poll_fds[i].fd == -1) {
            poll_fds[i].fd = fd;
            poll_fds[i].events = events;
            return i;
        }
    }
    return -1;
}

int add_fd_to_clients(int fd, struct client* clients, int clients_size) {
    for (int i = 0; i < clients_size; i++) {
        if (clients[i].fd == -1) {
            clients[i].fd = fd;
            return i;
        }
    }
    return -1;
}

int remove_fd(int fd, struct pollfd* poll_fds, int poll_fds_num) {
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
    clients[index].request.buf_size = BUF_SIZE;
    clients[index].request.buf_len = 0;
    clients[index].request.prev_buf_len = 0;
}

void free_clients(struct client* clients, int clients_num) {
    for (int i = 0; i < clients_num; i++) {
        free(clients->request.buf);
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

    int poll_fds_num = FD_SETSIZE;
    int clients_size = poll_fds_num;
    struct pollfd poll_fds[poll_fds_num];
    struct client clients[poll_fds_num];
    for (int i = 0; i < poll_fds_num; i++) {
        poll_fds[i].fd = -1;
        clients[i].fd = -1;
        clients[i].request.buf_size = BUF_SIZE;
        clients[i].request.buf_len = 0;
        clients[i].request.prev_buf_len = 0;
//        clients[i].request.headers = (struct phr_header*) malloc(100 * sizeof(struct phr_header));
        clients[i].request.buf = (char*) malloc(clients[i].request.buf_size * sizeof(char));
    }
    poll_fds[0].fd = proxy_fd;
    poll_fds[0].events = POLLIN;

    int iter = 0;
    while (iter++ < 10) {
        printf("%d.\n", iter);

        int poll_return = poll(poll_fds, poll_fds_num, -1);
        if (poll_return == -1) {
            perror("poll");
            close_all(poll_fds, poll_fds_num);
            return EXIT_FAILURE;
        }

        if (poll_fds[0].revents & POLLIN) {
            if (poll_fds[0].fd == proxy_fd) {
                int client_fd = accept(proxy_fd, NULL, NULL);
                if (client_fd < 0) {
                    perror("server accept failed");
                    close(proxy_fd);
                    return EXIT_FAILURE;
                } else {
                    printf("Server accepted the client...\n");
                }
                add_fd(client_fd, POLLIN, poll_fds, poll_fds_num);
                add_fd_to_clients(client_fd, clients, clients_size);
            }
        }

        for (int i = 0; i < clients_size; i++) {
            if (clients[i].fd != -1) {
                ssize_t rret;
                int pret;
                while ((rret = read(clients[i].fd, clients[i].request.buf + clients[i].request.buf_len,
                                    clients[i].request.buf_size - clients[i].request.buf_len)) == -1 &&
                       errno == EINTR);
                if (rret < 0) {
                    close_all(poll_fds, poll_fds_num);
                    perror("read");
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
                    fprintf(stderr, "phr_parse_request error\n");
                    return EXIT_FAILURE;
                }
                /* request is incomplete, continue the loop */
                if (pret == 2) {
                    printf("Read now: %zd\n", rret);
                    continue;
                }
                if (clients[i].request.buf_len == clients[i].request.buf_size) {
                    close_all(poll_fds, poll_fds_num);
                    fprintf(stderr, "request is too long\n");
                    return EXIT_FAILURE;
                }


                printf("Request length: %zu\n", clients[i].request.buf_len);
                printf("\nRequest:\n%.*s\n", (int) clients[i].request.buf_len, clients[i].request.buf);

                if (strncmp("GET", clients[i].request.method, clients[i].request.method_len) != 0) {
                    fprintf(stderr, "Method %.*s is not implemented\n",
                            (int) clients[i].request.method_len, clients[i].request.method);
                    char* client_response = "501 Not Implemented";
                    write_all(clients[i].fd, client_response, strlen(client_response));
                    remove_fd(clients[i].fd, poll_fds, poll_fds_num);
                    setup_client(i, clients);
                    close(clients[i].fd);
                    continue;
                }

                char* hostname;
                size_t hostname_len;
                get_hostname(&hostname, &hostname_len,
                             clients[i].request.headers, clients[i].request.num_headers);

                char ip[100];
                hostname_to_ip(hostname, ip);
                printf("IP: %s\n", ip);

                int server_fd;
                struct sockaddr_in servaddr;

                // socket create and verification
                server_fd = socket(AF_INET, SOCK_STREAM, 0);
                if (server_fd == -1) {
                    printf("socket creation failed...\n");
                    close_all(poll_fds, poll_fds_num);
                    return EXIT_FAILURE;
                } else {
                    printf("Socket successfully created..\n");
                }
                bzero(&servaddr, sizeof(servaddr));
                // assign IP, SERVER_PORT
                servaddr.sin_family = AF_INET;
                servaddr.sin_addr.s_addr = inet_addr(ip);
                servaddr.sin_port = htons(SERVER_PORT);

                //TODO add server_fd to poll_fds and to array of servers
                //  maybe make struct server or common struct for both server and client
                //  deal with poll_fd pointer in client


                // connect the client socket to server socket
                if (connect(server_fd, (SA*) &servaddr, sizeof(servaddr)) != 0) {
                    printf("connection with the server failed...\n");
                    close(clients[i].fd);
                    close(server_fd);
                    continue;
                } else {
                    printf("Connected to the server..\n");
                }

                ssize_t bytes_written = write_all(server_fd, clients[i].request.buf,
                                                  clients[i].request.buf_len);
                if (bytes_written == -1) {
                    perror("write_all");
                    close(clients[i].fd);
                    close(server_fd);
                    continue;
                }
                printf("Bytes written: %zd\n", bytes_written);
                size_t receive_buf_len = 1024;
                char* receive_buf = (char*) malloc(sizeof(char) * receive_buf_len);
                ssize_t bytes_read = read_all(server_fd, receive_buf, receive_buf_len);
                if (bytes_read == -1) {
                    perror("read_all");
                    close(server_fd);
                    close(clients[i].fd);
                    free(receive_buf);
                    continue;
                }
                printf("Bytes read: %zd\n\nResponse:\n%.*s\n\n", bytes_read, (int) bytes_read, receive_buf);
                write_all(clients[i].fd, receive_buf, bytes_read);
                close(server_fd);
                remove_fd(clients[i].fd, poll_fds, poll_fds_num);
                setup_client(i, clients);
                close(clients[i].fd);
                free(receive_buf);
            }
        }
    }

    free_clients(clients, clients_size);
    close(proxy_fd);
    return EXIT_SUCCESS;
}
