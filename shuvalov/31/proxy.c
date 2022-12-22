#define _GNU_SOURCE

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
#include <sys/signalfd.h>
#include <signal.h>
#include <assert.h>
#include "socket_operations/socket_operations.h"
#include "http_socket/http_socket.h"
#include "http_socket/cache.h"
#include "log/src/log.h"

#define SA struct sockaddr
#define SERVER_PORT 80
#define PROXY_PORT 8080
#define PROXY_IP "0.0.0.0"
#define MAX_FDS 100
#define USAGE "USAGE:\tproxy [IP PORT]\nWHERE:\tIP - IP address of proxy\n\tPORT - port of proxy\n"
#define READ_BUFFER_SIZE (32 * 1024)
#define WRITE_BUFFER_SIZE (64 * 1024)
#define CACHE_SIZE 10


long proxy_port = PROXY_PORT;
char* proxy_ip = PROXY_IP;
struct cache cache;

extern int errno;

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

ssize_t get_substring(char* substring, char* string, size_t string_length) {
    char* p1, * p2, * p3;
    p1 = string;
    p2 = substring;
    int substring_beginning;
    for (int i = 0; i < string_length; i++) {
        if (*p1 == *p2) {
            p3 = p1;
            int j;
            for (j = 0; j < strlen(substring); j++) {
                if (*p3 == *p2) {
                    p3++;
                    p2++;
                } else {
                    break;
                }
            }
            p2 = substring;
            if (strlen(substring) == j) {
                substring_beginning = i;
                return substring_beginning;
            }
        }
        p1++;
    }
    return -1;
}

void close_client(struct pollfd* poll_fds, size_t poll_fds_num, struct client* client) {
    if (client->fd >= 0) {
        unsubscribe(client);
        close(client->fd);
        remove_from_poll(client->fd, poll_fds, poll_fds_num);
        setup_client(client);
    }
}

void close_server(int fd, struct pollfd* poll_fds, size_t poll_fds_num, struct server* servers, size_t server_index) {
    close(fd);
    remove_from_poll(fd, poll_fds, poll_fds_num);
    setup_server(server_index, servers);
}

int receive_from_client(struct client* client, size_t client_index, struct pollfd* poll_fds, size_t poll_fds_num,
                        struct client* clients,
                        struct server* servers, size_t servers_size) {
    ssize_t return_value;
    int parse_result;
    while ((return_value = read(client->fd,
                                client->request.buf + client->request.buf_len,
                                client->request.buf_size - client->request.buf_len)) == -1 &&
           errno == EINTR);
    log_debug("\tClient read");
    if (return_value < 0) {
        log_error("read from client: %s", strerror(errno));
        close_client(poll_fds, poll_fds_num, clients + client_index);
        return 1;
    } else {
        client->request.prev_buf_len = client->request.buf_len;
        client->request.buf_len += return_value;
        if (client->request.buf_len == 0) {
            close_client(poll_fds, poll_fds_num, clients + client_index);
            return 0;
        }
        log_debug("\tprev buf len: %zu, buf len: %zu", client->request.prev_buf_len, client->request.buf_len);
        client->request.num_headers = client->request.headers_max_size;
        parse_result = parse_request(&client->request);
        if (parse_result == -1) {
            log_error("request is too long");
            close_client(poll_fds, poll_fds_num, clients + client_index);
            return 1;
        }
        if (parse_result == 2) {
            log_debug("\tPart of request read");
            return 0;
        }
        if (client->request.buf_len == client->request.buf_size) {
            log_error("request is too long (%zu bytes)", client->request.buf_len);
            close_client(poll_fds, poll_fds_num, clients + client_index);
            return 1;
        }
        if (strncmp("GET", client->request.method, client->request.method_len) != 0) {
            log_info("Method %.*s is not implemented",
                     (int) client->request.method_len, client->request.method);
            close_client(poll_fds, poll_fds_num, clients + client_index);
            return 1;
        }
        char* url = NULL;
        return_value = buffer_to_string(clients->request.path, clients->request.path_len, &url);
        if (return_value != 0) {
            log_error("buffer_to_string failed");
            close_client(poll_fds, poll_fds_num, clients + client_index);
            return 1;
        }
        struct cache_node* cache_node = get(cache, url);
        if (cache_node != NULL && (cache_node->response->status == 200 || cache_node->response->status == -1)) {
            cache_node->response->subscribers[cache_node->response->subscribers_count++] = client;
            client->response = cache_node->response;
            if (cache_node->response->buf_len > 0) {
                poll_fds[client->poll_index].events = POLLOUT;
            } else {
                poll_fds[client->poll_index].events = 0;
            }
            log_debug("\tSubscribe to cache");
            free(url);
            return 0;
        }
        char* hostname;
        size_t hostname_len;
        return_value = get_header_value(&hostname, &hostname_len, "Host",
                                        client->request.headers, client->request.num_headers);
        if (return_value > 0) {
            log_error("get_header_value failed");
            free(url);
            return 1;
        }
        char* ip = (char*) malloc(sizeof(char) * 100);
        if (ip == NULL) {
            log_error("malloc failed: %s", strerror(errno));
            close_client(poll_fds, poll_fds_num, clients + client_index);
            free(url);
            return 1;
        }
        return_value = hostname_to_ip(hostname, hostname_len, ip);
        free(hostname);
        assert(ip != NULL);
        if (return_value != 0) {
            log_error("hostname_to_ip failed");
            close_client(poll_fds, poll_fds_num, clients + client_index);
            free(ip);
            free(url);
            return 1;
        }
        int server_fd;
        struct sockaddr_in serv_addr;
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) {
            log_error("socket: %s", strerror(errno));
            close_client(poll_fds, poll_fds_num, clients + client_index);
            free(ip);
            free(url);
            return 1;
        }
        bzero(&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(ip);
        free(ip);
        serv_addr.sin_port = htons(SERVER_PORT);
        int poll_index = add_fd_to_poll(server_fd, POLLOUT, poll_fds, poll_fds_num);
        int server_index;
        if (poll_index >= 0) {
            server_index = add_fd_to_servers(server_fd, serv_addr, poll_index,
                                             &client->request, servers, servers_size);
        }
        if (poll_index < 0 || server_index < 0) {
            log_error("too many connections");
            free(url);
            return 1;
        }
        log_debug("\tServer index %d", server_index);
        size_t cache_index = set(&cache, url);
        subscribe(client, &cache, cache_index);
        make_publisher(&(servers[server_index]), &cache, cache_index);
        poll_fds[client->poll_index].events = 0;
        log_debug("\tClient poll_index %zu", servers[server_index].response->subscribers[0]->poll_index);
    }
    return 0;
}

int send_to_client(struct client* client, size_t client_index, struct pollfd* poll_fds, size_t poll_fds_num,
                   struct client* clients) {
    ssize_t return_value;
    size_t to_write = client->response->buf_len - client->bytes_written;
    log_debug("\tbuf_len = %d, bytes_written = %d", client->response->buf_len, client->bytes_written);
    if (to_write > WRITE_BUFFER_SIZE) {
        to_write = WRITE_BUFFER_SIZE;
    }
    return_value = write(client->fd,
                         client->response->buf + client->bytes_written,
                         to_write);
    if (return_value < 0) {
        log_error("write to client: %s", strerror(errno));
        close(client->fd);
        remove_from_poll(client->fd, poll_fds, poll_fds_num);
        setup_client(clients + client_index);
        return 1;
    }
    log_debug("\tSend %zd bytes", return_value);
    client->bytes_written += return_value;
    if (client->response->buf_len == client->bytes_written) {
        poll_fds[client->poll_index].events = 0;
    }
    if ((client->response->content_length + client->response->not_content_length == client->bytes_written &&
         client->response->content_length != 1 && client->response->not_content_length != 1) ||
        return_value == 0) {
        unsubscribe(client);
        if (client->response->status != 200) {
            log_debug("\tcache node index: %d", client->cache_node);
            assert(client->cache_node >= 0 && client->cache_node < cache.size);
            clear_cache_node(&(cache.nodes[client->cache_node]));
        }
        close(client->fd);
        remove_from_poll(client->fd, poll_fds, poll_fds_num);
        setup_client(clients + client_index);
        return 0;
    }
    return 0;
}

int process_clients(struct pollfd* poll_fds, size_t poll_fds_num, struct client* clients, size_t clients_size,
                    struct server* servers, size_t servers_size) {
    for (size_t i = 0; i < clients_size; i++) {
        struct client* client = &clients[i];
        if (client->fd != -1 && !client->processed) {
            log_debug("Client %d", i);
            if ((poll_fds[client->poll_index].revents & POLLHUP) == POLLHUP) {
                log_debug("\tPOLLHUP");
                close_client(poll_fds, poll_fds_num, clients + i);
                continue;
            }
            if ((poll_fds[client->poll_index].revents & POLLERR) == POLLERR) {
                log_error("\tPOLLERR");
                close_client(poll_fds, poll_fds_num, clients + i);
                continue;
            }
            if ((poll_fds[client->poll_index].revents & POLLIN) == POLLIN) {
                receive_from_client(client, i, poll_fds, poll_fds_num, clients, servers, servers_size);
                continue;
            }
            if ((poll_fds[client->poll_index].revents & POLLOUT) == POLLOUT) {
                send_to_client(client, i, poll_fds, poll_fds_num, clients);
            }
        }
    }
    return 0;
}

void
close_server_and_subscribers(struct server* server, size_t server_index, struct pollfd* poll_fds, size_t poll_fds_num,
                             struct server* servers, struct client* clients) {
    for (int k = 0; k < server->response->subscribers_max_size; k++) {
        if (server->response->subscribers[k] != NULL) {
            close_client(poll_fds, poll_fds_num, server->response->subscribers[k]);
        }
    }
    close_server(server->fd, poll_fds, poll_fds_num, servers, server_index);
}

int connect_to_server(struct server* server, int server_index, struct pollfd* poll_fds, size_t poll_fds_num,
                      struct server* servers, struct client* clients) {
    if (connect(server->fd,
                (SA*) &server->serv_addr,
                sizeof(server->serv_addr)) != 0) {
        close_server_and_subscribers(server, server_index, poll_fds, poll_fds_num, servers, clients);
        return 1;
    } else {
        server->processed = 1;
        log_debug("\tConnect server fd %d", server->fd);
    }
    return 0;
}

int send_to_server(struct server* server, int server_index, struct pollfd* poll_fds, size_t poll_fds_num,
                   struct server* servers, struct client* clients) {
    ssize_t return_value;
    while ((return_value = write(server->fd, server->request->buf + server->bytes_written,
                                 server->request->buf_len - server->bytes_written)) == -1 &&
           errno == EINTR);
    if (return_value < 0) {
        log_error("write to server: %s", strerror(errno));
        close_server_and_subscribers(server, server_index, poll_fds, poll_fds_num, servers, clients);
        return 1;
    } else if (return_value == 0) {
        poll_fds[server->poll_index].events = POLLIN;
    }
    log_debug("\tSend %zd", return_value);
    server->bytes_written += return_value;
    return 0;
}

int receive_from_server(struct server* server, int server_index, struct pollfd* poll_fds, size_t poll_fds_num,
                        struct client* clients,
                        struct server* servers) {
    log_debug("\tfd %d", server->fd);
    ssize_t return_value;
    size_t to_read = server->response->buf_size - server->response->buf_len;
    if (to_read > READ_BUFFER_SIZE) {
        to_read = READ_BUFFER_SIZE;
    }
    while ((return_value = read(server->fd, server->response->buf + server->response->buf_len,
                                to_read)) == -1 &&
           errno == EINTR);
    log_debug("\tReceive %zd bytes", return_value);
    if (return_value < 0) {
        log_error("read from server: %s", strerror(errno));
        close_server_and_subscribers(server, server_index, poll_fds, poll_fds_num, servers, clients);
        return 1;
    }
    if (return_value == 0) {
        close_server_and_subscribers(server, server_index, poll_fds, poll_fds_num, servers, clients);
        return 0;
    }
    server->response->prev_buf_len = server->response->buf_len;
    server->response->buf_len += return_value;
    if (server->response->buf_len == server->response->buf_size) {
        server->response->buf_size *= 2;
        server->response->buf = (char*) realloc(server->response->buf, sizeof(char) * server->response->buf_size);
        if (server->response->buf == NULL) {
            log_error("realloc failed: %s", strerror(errno));
            close_server_and_subscribers(server, server_index, poll_fds, poll_fds_num, servers, clients);
            return 1;
        }
    }
    if (server->response->not_content_length == -1) {
        server->response->num_headers = server->response->headers_max_size;
        return_value = parse_response(server->response);
        log_debug("\tParse %zd", return_value);
        log_debug("\tResponse parsed");
        for (int i = 0; i < server->response->num_headers; i++) {
            log_debug("\t%.*s: %.*s", (int) server->response->headers[i].name_len, server->response->headers[i].name,
                      (int) server->response->headers[i].value_len, server->response->headers[i].value);
        }
        if (server->response->content_length == -1) {
            char* content_length;
            size_t content_length_len;
            return_value = get_header_value(&content_length, &content_length_len, "Content-Length",
                                            server->response->headers, server->response->num_headers);
            if (return_value == 2) {
                close_server_and_subscribers(server, server_index, poll_fds, poll_fds_num, servers, clients);
                return 1;
            }
            if (return_value == 0) {
                char* content_length_str = NULL;
                return_value = buffer_to_string(content_length, content_length_len, &content_length_str);
                free(content_length);
                if (return_value != 0) {
                    return -1;
                }
                server->response->content_length = (int) strtol(content_length_str, NULL, 0);
                free(content_length_str);
            }
        }
        log_debug("\tContent length parsed");
        if (server->response->not_content_length == -1) {
            return_value = get_substring("\r\n\r\n", server->response->buf, server->response->buf_len);
            if (return_value >= 0) {
                server->response->not_content_length = (int) return_value + 4;
                if (server->response->content_length == -1) {
                    server->response->content_length = 0;
                }
            }
        }
    }
    log_debug("\tClients num %d", server->response->subscribers_count);
    for (int k = 0; k < server->response->subscribers_max_size; k++) {
        if (server->response->subscribers[k] != NULL) {
            poll_fds[server->response->subscribers[k]->poll_index].events = POLLOUT;
        }
    }
    if (server->response->buf_len == server->response->content_length + server->response->not_content_length) {
        close_server(server->fd, poll_fds, poll_fds_num, servers, server_index);
        return 0;
    }
    return 0;
}

int process_servers(struct pollfd* poll_fds, size_t poll_fds_num, struct client* clients,
                    struct server* servers, size_t servers_size) {
    for (int i = 0; i < servers_size; i++) {
        struct server* server = &servers[i];
        if (server->fd != -1 && !server->processed) {
            log_debug("Server %d", i);
            if ((poll_fds[server->poll_index].revents & POLLERR) == POLLERR) {
                log_error("\tPOLLERR");
                close_server_and_subscribers(server, i, poll_fds, poll_fds_num, servers, clients);
                continue;
            }
            if ((poll_fds[server->poll_index].revents & (POLLOUT | POLLHUP)) == (POLLOUT | POLLHUP)) {
                connect_to_server(server, i, poll_fds, poll_fds_num, servers, clients);
                continue;
            }
            if ((poll_fds[server->poll_index].revents & POLLHUP) == POLLHUP) {
                log_debug("\tPOLLHUP");
                close_server_and_subscribers(server, i, poll_fds, poll_fds_num, servers, clients);
                continue;
            }
            if ((poll_fds[server->poll_index].revents & POLLOUT) == POLLOUT) {
                send_to_server(server, i, poll_fds, poll_fds_num, servers, clients);
                continue;
            }
            if ((poll_fds[server->poll_index].revents & POLLIN) == POLLIN) {
                receive_from_server(server, i, poll_fds, poll_fds_num, clients, servers);
            }
        }
    }
    return 0;
}

int accept_client(struct pollfd* poll_fds, size_t poll_fds_num,
                  struct client* clients, size_t clients_size) {
    if (poll_fds[0].revents & POLLIN) {
        int client_fd = accept(poll_fds[0].fd, NULL, NULL);
        if (client_fd < 0) {
            log_error("accept failed: %s", strerror(errno));
            return -1;
        } else {
        }
        log_debug("Accept client fd %d", client_fd);
        int poll_index = add_fd_to_poll(client_fd, POLLIN, poll_fds, poll_fds_num);
        int client_index;
        if (poll_index >= 0) {
            client_index = add_fd_to_clients(client_fd, poll_index, clients, clients_size);
        }
        if (poll_index < 0 || client_index < 0) {
            log_error("too many connections");
            return -1;
        }
    }
    return 0;
}

int process_signal(struct pollfd poll_socket_fd) {
    if ((poll_socket_fd.revents & POLLIN) == POLLIN) {
        struct signalfd_siginfo signal_fd_info;
        ssize_t return_value = read(poll_socket_fd.fd, &signal_fd_info, sizeof(signal_fd_info));
        if (return_value != sizeof(signal_fd_info)) {
            log_error("read signal: %s", strerror(errno));
            return 2;
        }
        if (signal_fd_info.ssi_signo == SIGINT) {
            return 1;
        } else {
            return 2;
        }
    }
    return 0;
}

int signal_fd_init(struct pollfd* poll_fds) {
    sigset_t mask;
    int signal_fd;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        log_error("sigprocmask: %s", strerror(errno));
    }
    signal_fd = signalfd(-1, &mask, 0);
    if (signal_fd == -1) {
        log_error("signalfd: %s", strerror(errno));
        return 1;
    }
    poll_fds[1].fd = signal_fd;
    poll_fds[1].events = POLLIN;
    return 0;
}

int proxy_fd_init(struct pollfd* poll_fds) {
    int proxy_fd;
    struct sockaddr_in proxyaddr;
    proxy_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_fd == -1) {
        log_error("socket creation failed: %s", strerror(errno));
        return -1;
    } else {
    }
    bzero(&proxyaddr, sizeof(proxyaddr));
    proxyaddr.sin_family = AF_INET;
    proxyaddr.sin_addr.s_addr = inet_addr(PROXY_IP);
    proxyaddr.sin_port = htons(PROXY_PORT);
    if ((bind(proxy_fd, (SA*) &proxyaddr, sizeof(proxyaddr))) != 0) {
        log_error("socket bind failed: %s", strerror(errno));
        close(proxy_fd);
        return -1;
    } else {
    }
    if ((listen(proxy_fd, SOMAXCONN)) != 0) {
        log_error("listen failed: %s", strerror(errno));
        close(proxy_fd);
        return -1;
    } else {
    }
    poll_fds[0].fd = proxy_fd;
    poll_fds[0].events = POLLIN;
    return 0;
}

int parse_args(int argc, char* argv[]) {
    if (argc == 3) {
        proxy_ip = argv[1];
        proxy_port = strtol(argv[2], NULL, 0);
        if (proxy_port == LONG_MIN || proxy_port == LONG_MAX) {
            log_error("strtol failed");
            return -1;
        }
        return 0;
    } else if (argc == 1) {
        return 0;
    }
    return -1;
}

void init_poll_fds(struct pollfd* poll_fds, size_t size) {
    for (int i = 0; i < size; i++) {
        poll_fds[i].fd = -1;
    }
}

void init_sig_mask(sigset_t* sig_mask) {
    sigemptyset(sig_mask);
    sigaddset(sig_mask, SIGINT);
}

int main(int argc, char* argv[]) {
    log_set_level(LOG_ERROR);
    int error = 0;
    if (parse_args(argc, argv) != 0) {
        printf(USAGE);
        return EXIT_FAILURE;
    }
    int poll_fds_num = MAX_FDS;
    int clients_size = poll_fds_num;
    int servers_size = poll_fds_num;
    struct pollfd poll_fds[poll_fds_num];
    struct client clients[clients_size];
    struct server servers[servers_size];
    if (init_clients(clients, clients_size) != 0) {
        log_error("init_clients: %s", strerror(errno));
        error = 1;
        goto CLEANUP;
    }
    init_servers(servers, servers_size);
    init_poll_fds(poll_fds, poll_fds_num);
    if (proxy_fd_init(poll_fds) != 0 || signal_fd_init(poll_fds) != 0) {
        error = 1;
        goto CLEANUP;
    }
    if (init_cache(&cache, CACHE_SIZE) != 0) {
        error = 1;
        goto CLEANUP;
    }
    sigset_t sig_mask;
    init_sig_mask(&sig_mask);
    int iteration = 0;
    while (1) {
        log_debug("%d.", iteration++);
        int poll_return = ppoll(poll_fds, poll_fds_num, NULL, &sig_mask);
        if (poll_return == -1) {
            if (errno != EINTR) {
                log_error("poll failed: %s", strerror(errno));
                error = 1;
                goto CLEANUP;
            }
        }
        log_debug("Return from poll");
        log_debug("====Process signal====");
        int return_value = process_signal(poll_fds[1]);
        if (return_value != 0) {
            if (return_value == 2) {
                error = 1;
            }
            goto CLEANUP;
        }
        log_debug("=====================\n");
        log_debug("====Accept client====");
        if (accept_client(poll_fds, poll_fds_num, clients, clients_size) != 0) {
            log_error("accept_client failed");
        }
        log_debug("====================\n");
        log_debug("====Process clients===");
        process_clients(poll_fds, poll_fds_num, clients, clients_size, servers, servers_size);
        log_debug("=====================\n");
        log_debug("===Process servers===");
        process_servers(poll_fds, poll_fds_num, clients, servers, servers_size);
        log_debug("=====================\n\n");
        for (int i = 0; i < poll_fds_num; i++) {
            servers[i].processed = 0;
            clients[i].processed = 0;
        }
    }
    CLEANUP:
    free_clients(clients, clients_size);
    close_all(poll_fds, poll_fds_num);
    free_cache(cache);
    if (error) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
