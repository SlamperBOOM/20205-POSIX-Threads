#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include "cache.h"

#define POLL_TIMEOUT (1000)
#define CLEANUP_TIMEOUT (10)
#define URL_BUFF_SIZE (1024)
#define GET_REQ_SIZE (8)
#define HTTP_DELIM "://"

// Errors
#define UNABLE_TO_FIND_HOST (-1)
#define UNABLE_TO_CREATE_SOCKET (-2)
#define UNABLE_TO_CONNECT (-3)


typedef struct {
    int idx;
    pthread_t pthread;
    int fd;
    char busy;
    char running;
    void* server;
} ClientItem;

typedef struct {
    Cache* cache;
    int max_clients;
    ClientItem* clients;
    int running;
} Server;

int running = 0;

void SigHandler(int id) {
    if (id == SIGINT) {
        running = 0;
    }
}

int ParseInt(char* str) {
    char * endpoint;
    int res = (int)strtol(str, &endpoint, 10);

    if (*endpoint != '\0') {
        fprintf(stderr, "Unable to parse: '%s' as int\n", str);
        exit(1);
    }
    return res;
}

void ParseFullURL(char* host, char* path, int* port, char* full_url) {
    char* url_without_http = strstr(full_url, HTTP_DELIM);
    if (url_without_http == NULL) {
        url_without_http = full_url;
    } else {
        url_without_http += strlen(HTTP_DELIM);
    }
    char* url_tmp = strpbrk(url_without_http, ":/");
    // default port
    *port = 80;
    if (url_tmp == NULL) {
        strcpy(path, "/");
        strcpy(host, url_without_http);
    } else {
        memcpy(host, url_without_http, url_tmp - url_without_http);
        host[url_tmp - url_without_http] = '\0';
        if (url_tmp[0] == '/') {
            strcpy(path, url_tmp);
        } else {
            sscanf(url_tmp, ":%99d/%99[^\n]", port, path);
        }
    }
}

int ReadFromHost(int host_socket, char** content) {
    *content = malloc(BUFSIZ * sizeof(char));
    int content_size = BUFSIZ;
    int read_count = 0;
    int read_iter = 1;
    while (read_iter > 0) {
        if (content_size - read_count < BUFSIZ) {
            *content = realloc(*content, read_count + BUFSIZ);
            content_size = read_count + BUFSIZ;
        }
        read_iter = (int)read(host_socket, *content + read_count, BUFSIZ);
        read_count += read_iter;
    }
    return read_count;
}

void CreateRequest(const char* path, char* request) {
    strcpy(request, "GET /");
    strcat(request, path);
    strcat(request, "\r\n");
}


void SendRequest(int socket, char* path) {
    if (path == NULL) {
        write(socket, "GET /\r\n", strlen("GET /\r\n"));
    } else {
        char request[strlen(path) + GET_REQ_SIZE];
        CreateRequest(path, request);
        write(socket, request, strlen(request));
    }
}

int ConnectToHost(char* host, int port) {
    struct hostent *hp = gethostbyname(host);
    if (hp == NULL) {
        return UNABLE_TO_FIND_HOST;
    }
    struct sockaddr_in addr;
    memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;

    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) {
        return UNABLE_TO_CREATE_SOCKET;
    }
    int opt_val = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&opt_val, sizeof(int));

    int connection = connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    if (connection == -1) {
        return UNABLE_TO_CONNECT;
    }
    return sock;
}

int WriteToClient(int client_fd, const char* buff, int size) {
    int write_size = 0;
    while (write_size != size) {
        int write_iter = (int)write(client_fd, buff + write_size, size);
        if (write_iter < 1) {
            return 1;
        }
        write_size += write_iter;
    }
    return 0;
}

void HandleClientDisconnect(ClientItem* client) {
    printf("Client %d disconnected\n", client->idx);
    client->running = 0;
    close(client->fd);
    pthread_exit(NULL);
}

void* ClientWorker(void* arg) {
    printf("Handle client\n");

    ClientItem* client_item = (ClientItem *)arg;
    Server* server = (Server *)client_item->server;
    int client_fd = client_item->fd;

    struct pollfd poll_fd;
    poll_fd.fd = client_fd;
    poll_fd.events = POLLIN;

    char input_buffer[URL_BUFF_SIZE];
    while (server->running) {
        poll_fd.revents = 0;
        int poll_res = poll(&poll_fd, 1, POLL_TIMEOUT);

        if (poll_res == 0) {
            continue;
        }

        if (poll_res == -1 && errno != EINTR) {
            fprintf(stderr, "Error while poll\n");
            break;
        }

        if (poll_fd.revents & POLLIN) {
            int read_bytes = (int) read(client_fd, input_buffer, URL_BUFF_SIZE);
            if (read_bytes < 1) {
                HandleClientDisconnect(client_item);
            }
            input_buffer[read_bytes] = '\0';
            if ((int) input_buffer[0] == 0) {
                HandleClientDisconnect(client_item);
            }
            printf("Get request: %s\n", input_buffer);
            CacheRow *cache_row = GetCacheItem(server->cache, input_buffer, read_bytes);
            if (cache_row == NULL) {
                printf("Unable to find %s in cache\n", input_buffer);
                char host[URL_BUFF_SIZE];
                char path[URL_BUFF_SIZE];
                int port;
                ParseFullURL(host, path, &port, input_buffer);
                printf("Parsed url: host=%s, path=%s, port=%d\n", host, path, port);
                int host_sock = ConnectToHost(host, port);
                printf("Connected to %s (port %d)\n", host, port);
                if (host_sock == UNABLE_TO_FIND_HOST) {
                    fprintf(stderr, "Unable to find host: %s\n", host);
                } else if (host_sock == UNABLE_TO_CONNECT) {
                    fprintf(stderr, "Unable to connect to host: %s\n", host);
                } else if (host_sock == UNABLE_TO_CREATE_SOCKET) {
                    fprintf(stderr, "Unable to create socket\n");
                } else {
                    SendRequest(host_sock, path);
                    printf("Send GET request to %s\n", host);

                    char *content;
                    int content_size = ReadFromHost(host_sock, &content);
                    close(host_sock);

                    // write to client
                    WriteToClient(client_fd, content, content_size);
                    printf("Send %s response to client\n", host);

                    // write to cache
                    CacheItem *cache_item = malloc(sizeof(CacheItem));
                    if (cache_item == NULL) {
                        HandleClientDisconnect(client_item);
                    }
                    cache_item->content = content;
                    cache_item->last_visited = time(NULL);
                    cache_item->content_size = content_size;
                    cache_item->key = malloc((read_bytes + 1) * sizeof(char));
                    if (cache_item->key == NULL) {
                        fprintf(stderr, "Unable to allocate memory for cache row\n");
                    }
                    strcpy(cache_item->key, input_buffer);
                    cache_item->key_size = read_bytes;
                    int err = InsertCacheItem(server->cache, cache_item);
                    if (err != 0) {
                        fprintf(stderr, "Unable to save to cache\n");
                    }
                    printf("Save %s response to cache\n", host);
                }
            } else {
                printf("Find %s in cache\n", input_buffer);

                // get data from cache
                int err = WriteToClient(client_fd, cache_row->content, cache_row->content_size);
                if (err != 0) {
                    HandleClientDisconnect(client_item);
                }
                // cache_row allocates in GetCacheItem
                // So client responsible for that memory
                free(cache_row->content);
                free(cache_row);
            }
        }
    }

    HandleClientDisconnect(client_item);
    pthread_exit(NULL);
}

void* CleanerWorker(void* arg) {
    Server* server = (Server*)arg;
    while (server->running) {
        printf("Do cache cleanup...\n");
        sleep(CLEANUP_TIMEOUT);
        LookupAndClean(server->cache, CLEANUP_TIMEOUT);
    }
    pthread_exit(NULL);
}

int GetNotBusyClient(ClientItem* clients, int max_clients) {
    for (int i = 0; i < max_clients; ++i) {
        if (!clients[i].busy) {
            return i;
        }
    }
    return -1;
}

void tryToJoinThreads(Server* server) {
    int max_clients = server->max_clients;
    ClientItem* clients = server->clients;
    for (int i = 0; i < max_clients; ++i) {
        if (clients[i].busy && !clients[i].running) {
            int err = pthread_join(clients[i].pthread, NULL);
            if (err != 0) {
                fprintf(stderr, "Error while joining client thread\n");
            }
            clients[i].busy = 0;
        }
    }
}

int InitServer(Server* server, int max_clients, int cache_size, int cache_bucket_capacity) {
    // init params
    server->max_clients = max_clients;
    server->running = 0;

    // init memory
    server->cache = (Cache *) malloc(sizeof(Cache));
    server->clients = (ClientItem *)malloc(server->max_clients * sizeof(ClientItem));
    if (server->cache == NULL || server->clients == NULL) {
        fprintf(stderr, "Error while allocate memory for clients and cache\n");
        return 1;
    }

    //init cache
    int err = InitCache(server->cache, cache_size, cache_bucket_capacity);
    if (err != 0) {
        fprintf(stderr, "Error while init cache\n");
        return 1;
    }

    for (int i = 0; i < server->max_clients; ++i) {
        server->clients[i].idx = i;
        server->clients[i].busy = 0;
        server->clients[i].running = 0;
        server->clients[i].server = server;
    }

    return 0;
}

int RunServer(Server* server, int port) {

    // init address struct
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    // init listen socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1){
        fprintf(stderr, "Error while create socket\n");
        return 1;
    }

    // bind socket to address
    int err = bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    if (err != 0) {
        fprintf(stderr, "Error while bind socket\n");
        return 1;
    }

    err = listen(listen_fd, server->max_clients);
    if (err != 0) {
        fprintf(stderr, "Error while listen socket\n");
        return 1;
    }

    struct pollfd poll_fd;
    poll_fd.fd = listen_fd;
    poll_fd.events = POLLIN;

    // start server
    running = 1;
    server->running = 1;

    // start cache cleaner
    pthread_t cleaner_thread;
    pthread_create(&cleaner_thread, NULL, CleanerWorker, (void *) (server));

    while (running) {
        poll_fd.revents = 0;
        int poll_res = poll(&poll_fd, 1, POLL_TIMEOUT);

        if (poll_res == 0) {
            continue;
        }

        if (poll_res == -1 && errno != EINTR) {
            fprintf(stderr, "Error while poll\n");
            break;
        }

        if (poll_fd.revents & POLLIN) {
            int new_client_fd = accept(listen_fd, NULL, NULL);
            if (new_client_fd == -1) {
                fprintf(stderr, "Error while accept client\n");
                continue;
            }

            tryToJoinThreads(server);
            int client_id = GetNotBusyClient(server->clients, server->max_clients);
            if (client_id == -1) {
                fprintf(stderr, "Unable to accept more clients\n");
                continue;
            }

            printf("Client %d connected\n", client_id);
            server->clients[client_id].busy = 1;
            server->clients[client_id].running = 1;
            server->clients[client_id].fd = new_client_fd;
            err = pthread_create(
                    &(server->clients[client_id].pthread),
                    NULL,
                    ClientWorker,
                    (void *)(&(server->clients[client_id]))
            );
            if (err != 0) {
                fprintf(stderr, "Error while create thread\n");
            }
        }
    }
    printf("\nGraceful shutdown:\n");

    // To stop all threads that is running til server running (every thread)
    server->running = 0;

    printf("Joining clients threads...\n");
    tryToJoinThreads(server);
    printf("Joining cleaner thread...\n");
    pthread_join(cleaner_thread, NULL);

    printf("Free memory...\n");
    DeleteCache(server->cache);
    free(server->clients);

    printf("Close socket...\n");
    close(listen_fd);

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr,
    "Specify exact 4 args: server port, max clients count, cache_capacity, cache bucket_capacity.\n");
        pthread_exit(NULL);
    }
    // read params
    int port = ParseInt(argv[1]);
    int max_clients = ParseInt(argv[2]);
    int cache_size = ParseInt(argv[3]);
    int cache_bucket_capacity = ParseInt(argv[4]);

    //set interrupt handler
    struct sigaction sa;
    sa.sa_handler = SigHandler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    // init server
    Server server;
    int err = InitServer(&server, max_clients, cache_size, cache_bucket_capacity);
    if (err != 0) {
        fprintf(stderr, "Unable to init server. Exiting\n");
        return 1;
    }

    // run server
    err = RunServer(&server, port);
    if (err != 0) {
        fprintf(stderr, "Error while run server. Free memory. Exiting\n");
        DeleteCache(server.cache);
        free(server.clients);
        return 1;
    }

    return 0;
}
