#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "cache.h"
#include "picohttpparser-master/picohttpparser.h"
#include "task_queue.h"
#include "sync_pipe/sync_pipe.h"

#define ERROR_INVALID_ARGS 1
#define ERROR_PORT_CONVERSATION 2
#define ERROR_ALLOC 3
#define ERROR_SOCKET_INIT 4
#define ERROR_BIND 5
#define ERROR_LISTEN 6
#define ERROR_PIPE_OPEN 7
#define ERROR_SIG_HANDLER_INIT 8
#define ERROR_TASK_QUEUE_INIT 9
#define ERROR_INIT_SYNC_PIPE 10

#define TIMEOUT 1200
#define START_REQUEST_SIZE BUFSIZ
#define START_RESPONSE_SIZE BUFSIZ

int THREAD_POOL_SIZE = 5;

int REAL_THREAD_POOL_SIZE = 0;
task_queue_t *task_queue = NULL;
bool valid_task_queue = false;
int tasks_submitted = 0;
int tasks_completed = 0;
pthread_mutex_t tasks_counter_mutex;
bool valid_tasks_completed_mutex = false;
pthread_cond_t cond;
bool valid_cond;

pthread_t *tids = NULL;
bool *is_created_thread = NULL;
bool valid_threads_info = false;

bool is_stop = false;

void *threadFunc(void *arg) {
    while (!is_stop) {
        sync_pipe_wait();
        if (is_stop) {
            break;
        }
        task_t *task = popTask(task_queue);
        if (task != NULL) {
            task->func(task->arg);
            free(task);
            pthread_mutex_lock(&tasks_counter_mutex);
            tasks_completed += 1;
            if (tasks_completed == tasks_submitted) {
                pthread_cond_signal(&cond);
            }
            pthread_mutex_unlock(&tasks_counter_mutex);
        }
    }
    pthread_exit((void *)0);
}

size_t POLL_TABLE_SIZE = 32;
int poll_last_index = -1;
struct pollfd *poll_fds;
pthread_mutex_t poll_mutex;

int WRITE_STOP_FD = -1;
int READ_STOP_FD = -1;

void destroyPollFds() {
    pthread_mutex_lock(&poll_mutex);
    for (int i = 0; i < poll_last_index; i++) {
        if (poll_fds[i].fd > 0) {
            close(poll_fds[i].fd);
            poll_fds[i].fd = -1;
        }
    }
    free(poll_fds);
    poll_last_index = -1;
    pthread_mutex_unlock(&poll_mutex);
    pthread_mutex_destroy(&poll_mutex);
}

void removeFromPollFds(int fd) {
    int i;
    pthread_mutex_lock(&poll_mutex);
    for (i = 0; i < poll_last_index; i++) {
        pthread_mutex_unlock(&poll_mutex);

        pthread_mutex_lock(&poll_mutex);
        if (poll_fds[i].fd == fd) {
            close(poll_fds[i].fd);
            poll_fds[i].fd = -1;
            poll_fds[i].events = 0;
            poll_fds[i].revents = 0;
            break;
        }
        pthread_mutex_unlock(&poll_mutex);

        pthread_mutex_lock(&poll_mutex);
    }
    if (i == poll_last_index - 1) {
        poll_last_index -= 1;
    }
    for (i = (int)poll_last_index - 1; i > 0; i--) {
        pthread_mutex_unlock(&poll_mutex);

        pthread_mutex_lock(&poll_mutex);
        if (poll_fds[i].fd == -1) {
            poll_last_index -= 1;
        }
        else {
            break;
        }
    }
    pthread_mutex_unlock(&poll_mutex);
}

cache_list_t *cache_list = NULL;
bool valid_cache = false;
pthread_rwlockattr_t rw_lock_attr;
bool valid_rw_lock_attr = false;

void destroyRwLockAttr() {
    if (valid_rw_lock_attr) {
        pthread_rwlockattr_destroy(&rw_lock_attr);
        valid_rw_lock_attr = false;
    }
}

void destroyCacheList() {
    destroyList(cache_list);
    valid_cache = false;
}

typedef struct client {
    char *request;
    cache_t *cache_record;
    pthread_mutex_t client_mutex;
    size_t REQUEST_SIZE;
    size_t request_index;

    int write_response_index;

    int fd;
} client_t;

size_t CLIENTS_SIZE = 16;
client_t *clients;
bool valid_clients = false;
pthread_mutex_t clients_mutex;

void destroyClients() {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < CLIENTS_SIZE; i++) {
        if (clients[i].request != NULL) {
            free(clients[i].request);
            clients[i].request = NULL;
        }
        if (clients[i].fd != -1) {
            removeFromPollFds(clients[i].fd);
            clients[i].fd = -1;
        }
        clients[i].REQUEST_SIZE = 0;
        clients[i].request_index = 0;
        clients[i].cache_record = NULL;
        pthread_mutex_destroy(&clients[i].client_mutex);
        clients[i].write_response_index = -1;
    }
    free(clients);
    valid_clients = false;
    pthread_mutex_unlock(&clients_mutex);
    pthread_mutex_destroy(&clients_mutex);
}

typedef struct server {
    cache_t *cache_record;
    pthread_mutex_t server_mutex;
    int write_request_index;
    int fd;
} server_t;

size_t SERVERS_SIZE = 8;
server_t *servers;
bool valid_servers = false;
pthread_mutex_t servers_mutex;

void destroyServers() {
    pthread_mutex_lock(&servers_mutex);
    for (int i = 0; i < SERVERS_SIZE; i++) {
        if (servers[i].fd != -1) {
            removeFromPollFds(servers[i].fd);
            pthread_mutex_destroy(&servers[i].server_mutex);
            servers[i].cache_record = NULL;
            servers[i].fd = -1;
        }
    }
    free(servers);
    valid_servers = false;
    pthread_mutex_unlock(&servers_mutex);
    pthread_mutex_destroy(&servers_mutex);
}

void cleanUp() {
    fprintf(stderr, "\ncleaning up...\n");
    if (!is_stop) {
        is_stop = true;
        sync_pipe_notify(REAL_THREAD_POOL_SIZE);
        for (int i = 0; i < THREAD_POOL_SIZE; i++) {
            if (is_created_thread[i]) {
                pthread_join(tids[i], NULL);
            }
        }
    }
    if (READ_STOP_FD != -1) {
        close(READ_STOP_FD);
        READ_STOP_FD = -1;
    }
    if (WRITE_STOP_FD != -1) {
        close(WRITE_STOP_FD);
        WRITE_STOP_FD = -1;
    }
    if (valid_clients) {
        destroyClients();
    }
    if (valid_servers) {
        destroyServers();
    }
    if (poll_last_index >= 0) {
        destroyPollFds();
    }
    if (valid_cache) {
        destroyCacheList();
    }
    if (valid_rw_lock_attr) {
        destroyRwLockAttr();
    }
    if (valid_task_queue) {
        destroyTaskQueue(task_queue);
        valid_task_queue = false;
    }
    if (valid_threads_info) {
        free(is_created_thread);
        free(tids);
        valid_threads_info = false;
    }
    if (valid_tasks_completed_mutex) {
        pthread_mutex_destroy(&tasks_counter_mutex);
        valid_tasks_completed_mutex = false;
    }
    if (valid_cond) {
        pthread_cond_destroy(&cond);
        valid_cond = false;
    }
    sync_pipe_close();

}

void initRwLockAttr() {
    if (!valid_rw_lock_attr) {
        pthread_rwlockattr_init(&rw_lock_attr);
        pthread_rwlockattr_setkind_np(&rw_lock_attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
        valid_rw_lock_attr = true;
    }
}

void initEmptyServer(size_t i) {
    servers[i].fd = -1;
    servers[i].cache_record = NULL;
    pthread_mutex_init(&servers[i].server_mutex, NULL);
    servers[i].write_request_index = -1;
}

void initServers() {
    pthread_mutex_init(&servers_mutex, NULL);
    pthread_mutex_lock(&servers_mutex);
    servers = (server_t *) calloc(SERVERS_SIZE, sizeof(server_t));
    if (servers == NULL) {
        fprintf(stderr, "failed to alloc memory for servers\n");
        pthread_mutex_unlock(&servers_mutex);
        cleanUp();
        exit(ERROR_ALLOC);
    }
    for (size_t i = 0; i < SERVERS_SIZE; i++) {
        initEmptyServer(i);
    }
    pthread_mutex_unlock(&servers_mutex);
}

void reallocServers() {
    size_t prev_size = SERVERS_SIZE;
    SERVERS_SIZE *= 2;
    servers = realloc(servers, SERVERS_SIZE * sizeof(server_t));
    for (size_t i = prev_size; i < SERVERS_SIZE; i++) {
        initEmptyServer(i);
    }
}

int findFreeServer(int server_fd) {
    if (server_fd < 0) {
        return -1;
    }
    pthread_mutex_lock(&servers_mutex);
    for (int i = 0; i < SERVERS_SIZE; i++) {
        pthread_mutex_unlock(&servers_mutex);

        pthread_mutex_lock(&servers_mutex);
        if (servers[i].fd == -1) {
            servers[i].fd = server_fd;
            pthread_mutex_unlock(&servers_mutex);
            return i;
        }
        pthread_mutex_unlock(&servers_mutex);

        pthread_mutex_lock(&servers_mutex);
    }
    size_t prev_size = SERVERS_SIZE;
    reallocServers();
    servers[prev_size].fd = server_fd;
    pthread_mutex_unlock(&servers_mutex);
    return (int)prev_size;
}

int findServerByFd(int fd) {
    if (fd < 0) {
        return -1;
    }
    pthread_mutex_lock(&servers_mutex);
    for (int i = 0; i < SERVERS_SIZE; i++) {
        if (servers[i].fd == fd) {
            pthread_mutex_unlock(&servers_mutex);
            return i;
        }
        pthread_mutex_unlock(&servers_mutex);

        pthread_mutex_lock(&servers_mutex);
    }
    pthread_mutex_unlock(&servers_mutex);
    return -1;
}

void initEmptyClient(size_t i) {
    if (i >= CLIENTS_SIZE) {
        return;
    }
    clients[i].fd = -1;
    clients[i].request_index = 0;
    clients[i].request = NULL;
    clients[i].REQUEST_SIZE = 0;
    pthread_mutex_init(&clients[i].client_mutex, NULL);

    clients[i].cache_record = NULL;
    clients[i].write_response_index = -1;
}

void initClients() {
    pthread_mutex_init(&clients_mutex, NULL);
    pthread_mutex_lock(&clients_mutex);
    clients = (client_t *) calloc(CLIENTS_SIZE, sizeof(client_t));
    if (clients == NULL) {
        fprintf(stderr, "failed to alloc memory for clients\n");
        pthread_mutex_unlock(&clients_mutex);
        cleanUp();
        exit(ERROR_ALLOC);
    }
    for (size_t i = 0; i < CLIENTS_SIZE; i++) {
        initEmptyClient(i);
    }
    valid_clients = true;
    pthread_mutex_unlock(&clients_mutex);
}

void reallocClients() {
    size_t prev_size = CLIENTS_SIZE;
    CLIENTS_SIZE *= 2;
    clients = realloc(clients, CLIENTS_SIZE * sizeof(client_t));
    for (size_t i = prev_size; i < CLIENTS_SIZE; i++) {
        initEmptyClient(i);
    }
}

int findFreeClient(int client_fd) {
    if (client_fd < 0) {
        return -1;
    }
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < CLIENTS_SIZE; i++) {
        pthread_mutex_unlock(&clients_mutex);

        pthread_mutex_lock(&clients_mutex);
        if (clients[i].fd == -1) {
            clients[i].fd = client_fd;
            pthread_mutex_unlock(&clients_mutex);
            return i;
        }
        pthread_mutex_unlock(&clients_mutex);

        pthread_mutex_lock(&clients_mutex);
    }
    size_t prev_size = CLIENTS_SIZE;
    reallocClients();
    clients[prev_size].fd = client_fd;
    pthread_mutex_unlock(&clients_mutex);
    return (int)prev_size;
}

int findClientByFd(int fd) {
    if (fd < 0) {
        return -1;
    }
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < CLIENTS_SIZE; i++) {
        if (clients[i].fd == fd) {
            pthread_mutex_unlock(&clients_mutex);
            return i;
        }
        pthread_mutex_unlock(&clients_mutex);

        pthread_mutex_lock(&clients_mutex);
    }
    pthread_mutex_unlock(&clients_mutex);
    return -1;
}

void initEmptyCacheRecord(cache_t *record) {
    if (record == NULL) {
        return;
    }
    record->request = NULL;
    record->response = NULL;
    record->response_index = 0;
    record->RESPONSE_SIZE = 0;
    record->subscribers = NULL;
    record->full = false;
    record->url = NULL;
    record->URL_LEN = 0;
    record->SUBSCRIBERS_SIZE = 0;
    record->server_index = -1;
    record->private = true;
    pthread_rwlock_init(&record->rw_lock, &rw_lock_attr);
    record->valid_rw_lock = true;
    pthread_mutex_init(&record->subs_mutex, NULL);
    record->valid_subs_mutex = true;
    record->valid = false;
}

void initCacheList() {
    cache_list = initList();
    if (cache_list == NULL) {
        cleanUp();
        return;
    }
    valid_cache = true;
}

void initPollFds() {
    pthread_mutex_init(&poll_mutex, NULL);
    poll_fds = (struct pollfd *)calloc(POLL_TABLE_SIZE, sizeof(struct pollfd));
    if (poll_fds == NULL) {
        fprintf(stderr, "failed to alloc memory for poll_fds\n");
        cleanUp();
        exit(ERROR_ALLOC);
    }
    for (int i = 0; i < POLL_TABLE_SIZE; i++) {
        poll_fds[i].fd = -1;
    }
    poll_last_index = 0;
}

void reallocPollFds() {
    POLL_TABLE_SIZE *= 2;
    poll_fds = realloc(poll_fds, POLL_TABLE_SIZE * (sizeof(struct pollfd)));
    for (size_t i = poll_last_index; i < POLL_TABLE_SIZE; i++) {
        poll_fds[i].fd = -1;
    }
}

void addFdToPollFds(int fd, short events) {
    pthread_mutex_lock(&poll_mutex);
    for (int i = 0; i < poll_last_index; i++) {
        pthread_mutex_unlock(&poll_mutex);

        pthread_mutex_lock(&poll_mutex);
        if (poll_fds[i].fd == -1) {
            poll_fds[i].fd = fd;
            poll_fds[i].events = events;
            pthread_mutex_unlock(&poll_mutex);
            return;
        }
        pthread_mutex_unlock(&poll_mutex);

        pthread_mutex_lock(&poll_mutex);
    }
    if (poll_last_index >= POLL_TABLE_SIZE) {
        reallocPollFds();
    }
    poll_fds[poll_last_index].fd = fd;
    poll_fds[poll_last_index].events = events;
    poll_last_index += 1;
    pthread_mutex_unlock(&poll_mutex);
}

int connectToServerHost(char *hostname, int port) {
    if (hostname == NULL || port < 0) {
        return -1;
    }
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        return -1;
    }

    struct hostent *h = gethostbyname(hostname);
    if (h == NULL) {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr, h->h_addr, h->h_length);

    int connect_res = connect(server_sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
    if (connect_res != 0) {
        perror("connect");
        return -1;
    }
    return server_sock;
}

int initListener(int port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        cleanUp();
        exit(ERROR_SOCKET_INIT);
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    int bind_res = bind(listen_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
    if (bind_res != 0) {
        perror("bind");
        close(listen_fd);
        cleanUp();
        exit(ERROR_BIND);
    }

    int listen_res = listen(listen_fd, (int)CLIENTS_SIZE);
    if (listen_res == -1) {
        perror("listen");
        close(listen_fd);
        cleanUp();
        exit(ERROR_LISTEN);
    }
    return listen_fd;
}

void acceptNewClient(int listen_fd) {
    int new_client_fd = accept(listen_fd, NULL, NULL);
    if (new_client_fd == -1) {
        perror("new client accept");
        return;
    }
    int fcntl_res = fcntl(new_client_fd, F_SETFL, O_NONBLOCK);
    if (fcntl_res < 0) {
        perror("make new client nonblock");
        close(new_client_fd);
        return;
    }
    int index = findFreeClient(new_client_fd);
    addFdToPollFds(new_client_fd, POLLIN);
    fprintf(stderr, "new client %d accepted\n", index);
}

void changeEventForFd(int fd, short new_events) {
    pthread_mutex_lock(&poll_mutex);
    for (int i = 0; i < poll_last_index; i++) {
        pthread_mutex_unlock(&poll_mutex);

        pthread_mutex_lock(&poll_mutex);
        if (poll_fds[i].fd == fd) {
            poll_fds[i].events = new_events;
            pthread_mutex_unlock(&poll_mutex);
            return;
        }
        pthread_mutex_unlock(&poll_mutex);

        pthread_mutex_lock(&poll_mutex);
    }
    pthread_mutex_unlock(&poll_mutex);
}

void removeSubscriber(int client_num, cache_t *record) {
    if (record == NULL || client_num >= CLIENTS_SIZE || client_num < 0) {
        return;
    }
    //fprintf(stderr, "removing subscriber %d...\n", client_num);
    pthread_mutex_lock(&record->subs_mutex);
    for (int i = 0; i < record->SUBSCRIBERS_SIZE; i++) {
        if (record->subscribers[i] == client_num) {
            record->subscribers[i] = -1;
        }
    }
    pthread_mutex_unlock(&record->subs_mutex);
}

void disconnectClient(int client_num) {
    if (client_num < 0 || client_num >= CLIENTS_SIZE) {
        return;
    }
    pthread_mutex_lock(&clients_mutex);
    pthread_mutex_lock(&clients[client_num].client_mutex);
    fprintf(stderr, "disconnecting client %d...\n", client_num);
    if (clients[client_num].request != NULL) {
        //free(clients[client_num].request);
        //clients[client_num].request = NULL;
        memset(clients[client_num].request, 0, clients[client_num].request_index);
        clients[client_num].request_index = 0;
        //clients[client_num].REQUEST_SIZE = 0;
    }
    if (clients[client_num].cache_record != NULL) {
        removeSubscriber(client_num, clients[client_num].cache_record);
        clients[client_num].cache_record = NULL;
        clients[client_num].write_response_index = -1;
    }
    if (clients[client_num].fd != -1) {
        removeFromPollFds(clients[client_num].fd);
        clients[client_num].fd = -1;
    }
    pthread_mutex_unlock(&clients[client_num].client_mutex);
    pthread_mutex_unlock(&clients_mutex);
}

void addSubscriber(int client_num, cache_t *record) {
    if (record == NULL || client_num >= CLIENTS_SIZE || client_num < 0 ||
        !record->valid) {
        return;
    }
    pthread_mutex_lock(&record->subs_mutex);
    if (record->SUBSCRIBERS_SIZE == 0) {
        record->SUBSCRIBERS_SIZE = 4;
        record->subscribers = (int *) calloc(record->SUBSCRIBERS_SIZE, sizeof(int));
        if (record->subscribers == NULL) {
            pthread_mutex_unlock(&record->subs_mutex);
            disconnectClient(client_num);
            return;
        }
        for (int i = 0; i < record->SUBSCRIBERS_SIZE; i++) {
            record->subscribers[i] = -1;
        }
    }
    for (int i = 0; i < record->SUBSCRIBERS_SIZE; i++) {
        if (record->subscribers[i] == -1 || record->subscribers[i] == client_num) {
            record->subscribers[i] = client_num;
            pthread_mutex_unlock(&record->subs_mutex);
            return;
        }
    }
    size_t prev_index = record->SUBSCRIBERS_SIZE;
    record->SUBSCRIBERS_SIZE *= 2;
    record->subscribers = realloc(record->subscribers, record->SUBSCRIBERS_SIZE * sizeof(int));
    for (size_t i = prev_index; i < record->SUBSCRIBERS_SIZE; i++) {
        record->subscribers[i] = -1;
    }
    record->subscribers[prev_index] = client_num;
    pthread_mutex_unlock(&record->subs_mutex);
}

void notifySubscribers(cache_t *record, short new_events) {
    pthread_mutex_lock(&record->subs_mutex);
    for (int i = 0; i < record->SUBSCRIBERS_SIZE; i++) {
        if (record->subscribers[i] != -1) {
            changeEventForFd(clients[record->subscribers[i]].fd, new_events);
            if (clients[record->subscribers[i]].write_response_index < 0) {
                clients[record->subscribers[i]].write_response_index = 0;
            }
        }
    }
    pthread_mutex_unlock(&record->subs_mutex);
}

void disconnectServer(int server_num) {
    if (server_num < 0 || server_num > SERVERS_SIZE) {
        return;
    }
    pthread_mutex_lock(&servers_mutex);
    servers[server_num].write_request_index = -1;
    if (servers[server_num].cache_record != NULL) {
        servers[server_num].cache_record->server_index = -1;
        notifySubscribers(servers[server_num].cache_record, POLLIN | POLLOUT);
        servers[server_num].cache_record = NULL;
    }
    if (servers[server_num].fd != -1) {
        removeFromPollFds(servers[server_num].fd);
        servers[server_num].fd = -1;
    }
    pthread_mutex_unlock(&servers_mutex);
}

void freeCacheRecord(cache_t *record) {
    if (record == NULL) {
        return;
    }
    record->private = true;
    if (record->url != NULL) {
        free(record->url);
        record->url = NULL;
        record->URL_LEN = 0;
    }
    if (record->request != NULL) {
        free(record->request);
        record->request = NULL;
    }
    record->REQUEST_SIZE = 0;
    if (record->response != NULL) {
        free(record->response);
        record->response = NULL;
    }
    record->response_index = 0;
    record->RESPONSE_SIZE = 0;
    if (record->subscribers != NULL) {
        pthread_mutex_lock(&record->subs_mutex);
        for(int i = 0; i < record->SUBSCRIBERS_SIZE; i++) {
            if (record->subscribers[i] != -1) {
                disconnectClient(record->subscribers[i]);
            }
        }
        free(record->subscribers);
        record->subscribers = NULL;
        record->SUBSCRIBERS_SIZE = 0;
        pthread_mutex_unlock(&record->subs_mutex);
    }
    if (record->valid_subs_mutex) {
        pthread_mutex_destroy(&record->subs_mutex);
        record->valid_subs_mutex = false;
    }
    if (record->server_index != -1) {
        disconnectServer(record->server_index);
    }
    if (record->valid_rw_lock) {
        pthread_rwlock_destroy(&record->rw_lock);
        record->valid_rw_lock = false;
    }
    record->valid = false;
}

void printCacheRecord(cache_t *record) {
    if (record == NULL) {
        fprintf(stderr, "cache record is NULL\n");
        return;
    }
    fprintf(stderr, "cache record:\n");
    if (record->valid) {
        fprintf(stderr, "valid, ");
    }
    else {
        fprintf(stderr, "NOT valid, ");
    }
    if (record->private) {
        fprintf(stderr, "private, ");
    }
    else {
        fprintf(stderr, "public, ");
    }
    if (record->full) {
        fprintf(stderr, "full\n");
    }
    else {
        fprintf(stderr, "NOT full\n");
    }
    fprintf(stderr, "server_index = %d\n", record->server_index);
    fprintf(stderr, "REQ_SIZE = %lu\n", record->REQUEST_SIZE);
    if (record->valid_rw_lock) {
        fprintf(stderr, "rw_lock = valid, ");
    }
    else {
        fprintf(stderr, "rw_lock = NOT valid, ");
    }
    fprintf(stderr, "rsp_ind = %lu, RSP_SIZE = %lu\n", record->response_index, record->RESPONSE_SIZE);
    if (record->valid_subs_mutex) {
        fprintf(stderr, "subs_mutex = valid, ");
        fprintf(stderr, "SUBS_SIZE = %lu\n", record->SUBSCRIBERS_SIZE);
        pthread_mutex_lock(&record->subs_mutex);
        for (int i = 0; i < record->SUBSCRIBERS_SIZE; i++) {
            fprintf(stderr, "%d ", record->subscribers[i]);
        }
        pthread_mutex_unlock(&record->subs_mutex);
        fprintf(stderr, "\n\n");
    }
    else {
        fprintf(stderr, "subs_mutex = NOT valid, ");
        fprintf(stderr, "SUBS_SIZE = %lu\n\n", record->SUBSCRIBERS_SIZE);
    }
}

void printCacheList() {
    pthread_mutex_lock(&cache_list->mutex);
    cache_node_t *list_nodes = cache_list->head;
    fprintf(stderr, "printing cache...\n");
    while (list_nodes != NULL) {
        printCacheRecord(list_nodes->record);
        list_nodes = list_nodes->next;
    }
    pthread_mutex_unlock(&cache_list->mutex);
}

void findAndAddCacheRecord(char *url, size_t url_len, int client_num, char *host, int REQUEST_SIZE) {
    //fprintf(stderr, "adding client %d to cache record\nwith url: %s\n", client_num, url);
    pthread_mutex_lock(&cache_list->mutex);
    cache_node_t *list_nodes = cache_list->head;
    cache_node_t *prev_node = NULL;
    while (list_nodes != NULL) {
        if (!list_nodes->record->valid) {
            cache_node_t *next_node = list_nodes->next;
            freeCacheRecord(list_nodes->record);
            free(list_nodes->record);
            free(list_nodes);
            list_nodes = next_node;
            continue;
        }
        if (list_nodes->record->URL_LEN == url_len && strncmp(list_nodes->record->url, url, url_len) == 0) {
            pthread_mutex_lock(&clients[client_num].client_mutex);
            clients[client_num].cache_record = list_nodes->record;
            addSubscriber(client_num, list_nodes->record);
            changeEventForFd(clients[client_num].fd, POLLIN | POLLOUT);
            clients[client_num].write_response_index = 0;
            pthread_mutex_unlock(&clients[client_num].client_mutex);
            pthread_mutex_unlock(&cache_list->mutex);
            //fprintf(stderr, "client %d added to existing cache record\n", client_num);
            return;
        }
        pthread_mutex_unlock(&cache_list->mutex);

        pthread_mutex_lock(&cache_list->mutex);
        prev_node = list_nodes;
        list_nodes = list_nodes->next;
    }
    cache_node_t *new_node = (cache_node_t *) calloc(1, sizeof(cache_node_t));
    if (new_node == NULL) {
        pthread_mutex_unlock(&cache_list->mutex);
        fprintf(stderr, "failed to add client %d to cache (can't create new node)\n", client_num);
        disconnectClient(client_num);
        return;
    }
    new_node->record = (cache_t *)calloc(1, sizeof(cache_t));
    if (new_node->record == NULL) {
        pthread_mutex_unlock(&cache_list->mutex);
        fprintf(stderr, "failed to add client %d to cache (can't create new record)\n", client_num);
        free(new_node);
        disconnectClient(client_num);
        return;
    }
    new_node->next = NULL;
    initEmptyCacheRecord(new_node->record);
    new_node->record->url = url;
    new_node->record->URL_LEN = url_len;
    new_node->record->valid = true;
    if (prev_node == NULL) {
        cache_list->head = new_node;
    }
    else {
        prev_node->next = new_node;
    }
    pthread_mutex_unlock(&cache_list->mutex);
    //fprintf(stderr, "node added to list\n");

    new_node->record->request = (char *)calloc(REQUEST_SIZE, sizeof(char));
    if (new_node->record->request == NULL) {
        freeCacheRecord(new_node->record);
        disconnectClient(client_num);
        return;
    }
    pthread_mutex_lock(&clients[client_num].client_mutex);
    memcpy(new_node->record->request, clients[client_num].request, REQUEST_SIZE);
    pthread_mutex_unlock(&clients[client_num].client_mutex);
    new_node->record->REQUEST_SIZE = REQUEST_SIZE;
    int server_fd = connectToServerHost(host, 80);
    if (server_fd == -1) {
        fprintf(stderr, "failed to connect to remote host: %s\n", host);
        freeCacheRecord(new_node->record);
        disconnectClient(client_num);
        free(host);
        return;
    }
    int fcntl_res = fcntl(server_fd, F_SETFL, O_NONBLOCK);
    if (fcntl_res < 0) {
        perror("make new server fd nonblock");
        close(server_fd);
        freeCacheRecord(new_node->record);
        disconnectClient(client_num);
        free(host);
        return;
    }
    free(host);
    int server_num = findFreeServer(server_fd);
    servers[server_num].cache_record = new_node->record;
    servers[server_num].write_request_index = 0;
    addFdToPollFds(server_fd, POLLIN | POLLOUT);

    new_node->record->server_index = server_num;

    pthread_mutex_lock(&clients[client_num].client_mutex);
    clients[client_num].cache_record = new_node->record;
    addSubscriber(client_num, new_node->record);
    clients[client_num].write_response_index = 0;
    pthread_mutex_unlock(&clients[client_num].client_mutex);
}

void shiftRequest(int client_num, int pret) {
    if (client_num < 0 || client_num >= CLIENTS_SIZE || pret < 0 || clients[client_num].fd < 0 ||
        clients[client_num].request == NULL || clients[client_num].request_index == 0) {
        return;
    }
    for (int i = pret; i < clients[client_num].request_index; i++) {
        clients[client_num].request[i] = clients[client_num].request[i - pret];
    }
    memset(&clients[client_num].request[clients[client_num].request_index - pret], 0, pret);
    clients[client_num].request_index -= pret;
}

void readFromClient(int client_num) {
    //fprintf(stderr, "read from client %d\n", client_num);
    if (client_num < 0 || client_num > CLIENTS_SIZE) {
        return;
    }
    pthread_mutex_lock(&clients[client_num].client_mutex);
    if (clients[client_num].fd == -1) {
        pthread_mutex_unlock(&clients[client_num].client_mutex);
        return;
    }
    char buf[BUFSIZ];
    ssize_t was_read = read(clients[client_num].fd, buf, BUFSIZ);
    if (was_read < 0) {
        pthread_mutex_unlock(&clients[client_num].client_mutex);
        perror("read");
        return;
    }
    else if (was_read == 0) {
        fprintf(stderr, "client %d closed connection\n", client_num);
        pthread_mutex_unlock(&clients[client_num].client_mutex);
        disconnectClient(client_num);
        return;
    }
    if (clients[client_num].REQUEST_SIZE == 0) {
        clients[client_num].REQUEST_SIZE = START_REQUEST_SIZE;
        clients[client_num].request = (char *)calloc(clients[client_num].REQUEST_SIZE, sizeof(char));
        if (clients[client_num].request == NULL) {
            fprintf(stderr, "calloc returned NULL\n");
            pthread_mutex_unlock(&clients[client_num].client_mutex);
            disconnectClient(client_num);
        }
    }
    if (clients[client_num].request_index + was_read >= clients[client_num].REQUEST_SIZE) {
        clients[client_num].REQUEST_SIZE *= 2;
        clients[client_num].request = realloc(clients[client_num].request,
                                                    clients[client_num].REQUEST_SIZE * sizeof(char));
    }
    memcpy(&clients[client_num].request[clients[client_num].request_index], buf, was_read);
    clients[client_num].request_index += was_read;
    char *method;
    char *path;
    size_t method_len, path_len;
    int minor_version;
    size_t num_headers = 100;
    struct phr_header headers[num_headers];
    int pret = phr_parse_request(clients[client_num].request, clients[client_num].request_index,
                                 (const char **)&method, &method_len, (const char **)&path, &path_len,
                                 &minor_version, headers, &num_headers, 0);
    pthread_mutex_unlock(&clients[client_num].client_mutex);
    if (pret > 0) {
        if (strncmp(method, "GET", method_len) != 0) {
            disconnectClient(client_num);
            return;
        }
        size_t url_len = path_len;
        char *url = calloc(url_len, sizeof(char));
        if (url == NULL) {
            disconnectClient(client_num);
            return;
        }
        memcpy(url, path, path_len);


        char *host = NULL;
        for (size_t i = 0; i < num_headers; i++) {
            if (strncmp(headers[i].name, "Host", 4) == 0) {
                host = calloc(headers[i].value_len + 1, sizeof(char));
                if (host == NULL) {
                    free(url);
                    disconnectClient(client_num);
                    return;
                }
                memcpy(host, headers[i].value, headers[i].value_len);
                break;
            }
        }
        if (host == NULL) {
            free(url);
            disconnectClient(client_num);
            return;
        }
        findAndAddCacheRecord(url, url_len, client_num, host, pret);

        pthread_mutex_lock(&clients[client_num].client_mutex);
        shiftRequest(client_num, pret);
        pthread_mutex_unlock(&clients[client_num].client_mutex);
    }
    else if (pret == -1) {
        disconnectClient(client_num);
    }
}


void writeToServer(int server_num) {
    //fprintf(stderr, "write to server %d\n", server_num);
    if (server_num < 0 || server_num >= SERVERS_SIZE) {
        return;
    }
    ssize_t written = write(servers[server_num].fd,
                            &servers[server_num].cache_record->request[servers[server_num].write_request_index],
                            servers[server_num].cache_record->REQUEST_SIZE -
                                servers[server_num].write_request_index);
    if (written < 0) {
        perror("write");
        disconnectServer(server_num);
        return;
    }
    servers[server_num].write_request_index += (int)written;
    if (servers[server_num].write_request_index == servers[server_num].cache_record->REQUEST_SIZE) {
        changeEventForFd(servers[server_num].fd, POLLIN);
    }
}

void readFromServer(int server_num) {
    //fprintf(stderr, "read from server %d\n", server_num);
    if (server_num < 0 || server_num >= SERVERS_SIZE) {
        return;
    }
    char buf[BUFSIZ];
    ssize_t was_read = read(servers[server_num].fd, buf, BUFSIZ);
    //fprintf(stderr, "server %d was_read = %ld\n", server_num, was_read);
    if (was_read < 0) {
        perror("read");
        return;
    }
    else if (was_read == 0) {
        servers[server_num].cache_record->full = true;
        pthread_rwlock_wrlock(&servers[server_num].cache_record->rw_lock);

        servers[server_num].cache_record->response = realloc(
                servers[server_num].cache_record->response,
                servers[server_num].cache_record->response_index * sizeof(char));
        servers[server_num].cache_record->RESPONSE_SIZE = servers[server_num].cache_record->response_index;

        pthread_rwlock_unlock(&servers[server_num].cache_record->rw_lock);
        notifySubscribers(servers[server_num].cache_record, POLLIN | POLLOUT);
        disconnectServer(server_num);
        return;
    }
    pthread_rwlock_wrlock(&servers[server_num].cache_record->rw_lock);
    if (servers[server_num].cache_record->RESPONSE_SIZE == 0) {
        servers[server_num].cache_record->RESPONSE_SIZE = START_RESPONSE_SIZE;
        servers[server_num].cache_record->response = (char *)calloc(START_RESPONSE_SIZE, sizeof(char));
        if (servers[server_num].cache_record->response == NULL) {
            disconnectServer(server_num);
            pthread_rwlock_unlock(&servers[server_num].cache_record->rw_lock);
        }
    }
    if (was_read + servers[server_num].cache_record->response_index >=
        servers[server_num].cache_record->RESPONSE_SIZE) {
        servers[server_num].cache_record->RESPONSE_SIZE *= 2;
        servers[server_num].cache_record->response = realloc(
                servers[server_num].cache_record->response,
                servers[server_num].cache_record->RESPONSE_SIZE * sizeof(char));
    }
    memcpy(&servers[server_num].cache_record->response[servers[server_num].cache_record->response_index],
           buf, was_read);
    size_t prev_len = servers[server_num].cache_record->response_index;
    servers[server_num].cache_record->response_index += was_read;
    pthread_rwlock_unlock(&servers[server_num].cache_record->rw_lock);
    int minor_version, status;
    char *msg;
    size_t msg_len;
    size_t num_headers = 100;
    struct phr_header headers[num_headers];
    pthread_rwlock_rdlock(&servers[server_num].cache_record->rw_lock);
    int pret = phr_parse_response(servers[server_num].cache_record->response,
                                  servers[server_num].cache_record->response_index,
                                  &minor_version, &status, (const char **)&msg, &msg_len, headers,
                                  &num_headers, prev_len);
    pthread_rwlock_unlock(&servers[server_num].cache_record->rw_lock);
    notifySubscribers(servers[server_num].cache_record, POLLIN | POLLOUT);
    if (pret > 0) {
        if (status >= 200 && status < 300) {
            servers[server_num].cache_record->private = false;
        }
    }
}


void writeToClient(int client_num) {
    //fprintf(stderr, "write to client %d\n", client_num);
    if (client_num < 0 || client_num >= CLIENTS_SIZE) {
        fprintf(stderr, "invalid client_num %d\n", client_num);
        return;
    }
    pthread_mutex_lock(&clients[client_num].client_mutex);
    if (clients[client_num].cache_record == NULL) {
        fprintf(stderr, "client %d cache record is NULL\n", client_num);
        pthread_mutex_unlock(&clients[client_num].client_mutex);
        disconnectClient(client_num);
        return;
    }
    //printCacheRecord(clients[client_num].cache_record);
    if (clients[client_num].cache_record->server_index == -1 &&
        !clients[client_num].cache_record->full) {
        //fprintf(stderr, "invalid cache record detected by client %d\n", client_num);
        freeCacheRecord(clients[client_num].cache_record);
        pthread_mutex_unlock(&clients[client_num].client_mutex);
        disconnectClient(client_num);
        return;
    }
    //fprintf(stderr, "acquire rdlock by client %d...\n", client_num);
    pthread_rwlock_rdlock(&clients[client_num].cache_record->rw_lock);
    //fprintf(stderr, "acquire rdlock by client %d success\n", client_num);
    /*fprintf(stderr, "client %d write response index = %d, cache response index = %lu\n", client_num,
            clients[client_num].write_response_index, clients[client_num].cache_record->response_index);*/
    ssize_t written = write(clients[client_num].fd,
                            &clients[client_num].cache_record->response[clients[client_num].write_response_index],
                            clients[client_num].cache_record->response_index -
                                clients[client_num].write_response_index);
    pthread_rwlock_unlock(&clients[client_num].cache_record->rw_lock);
    //fprintf(stderr, "written %ld to client %d\n", written, client_num);
    if (written < 0) {
        perror("write");
        pthread_mutex_unlock(&clients[client_num].client_mutex);
        disconnectClient(client_num);
        return;
    }
    clients[client_num].write_response_index += (int)written;
    if (clients[client_num].write_response_index == clients[client_num].cache_record->response_index) {
        changeEventForFd(clients[client_num].fd, POLLIN);
    }
    pthread_mutex_unlock(&clients[client_num].client_mutex);

}

static void sigCatch(int sig) {
    if (sig == SIGINT) {
        if (WRITE_STOP_FD != -1) {
            char a = 'a';
            write(WRITE_STOP_FD, &a, 1);
            close(WRITE_STOP_FD);
            WRITE_STOP_FD = -1;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Error wrong amount of arguments\n");
        exit(ERROR_INVALID_ARGS);
    }
    char *invalid_sym;
    errno = 0;
    THREAD_POOL_SIZE = (int)strtol(argv[1], &invalid_sym, 10);
    if (errno != 0 || *invalid_sym != '\0') {
        fprintf(stderr, "Error wrong TREAD_POOL_SIZE\n");
        THREAD_POOL_SIZE = 5;
    }
    errno = 0;
    int port = (int)strtol(argv[2], &invalid_sym, 10);
    if (errno != 0 || *invalid_sym != '\0') {
        fprintf(stderr, "Error wrong port\n");
        exit(ERROR_PORT_CONVERSATION);
    }
    pthread_mutex_init(&tasks_counter_mutex, NULL);
    valid_tasks_completed_mutex = true;
    pthread_cond_init(&cond, NULL);
    valid_cond = true;

    tids = (pthread_t *)calloc(THREAD_POOL_SIZE, sizeof(pthread_t));
    if (tids == NULL) {
        fprintf(stderr, "failed to alloc memory for tids\n");
        exit(ERROR_ALLOC);
    }
    is_created_thread = (bool *) calloc(THREAD_POOL_SIZE, sizeof(bool));
    if (is_created_thread == NULL) {
        fprintf(stderr, "failed to alloc memory for thread creation results\n");
        free(tids);
        exit(ERROR_ALLOC);
    }
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        is_created_thread[i] = false;
    }
    valid_threads_info = true;

    task_queue = initTaskQueue();
    if (task_queue == NULL) {
        fprintf(stderr, "failed to init task queue\n");
        exit(ERROR_TASK_QUEUE_INIT);
    }
    valid_task_queue = true;

    int sync_pipe_res = sync_pipe_init();
    if (sync_pipe_res != 0) {
        fprintf(stderr, "failed to init sync pipe\n");
        cleanUp();
        exit(ERROR_INIT_SYNC_PIPE);
    }

    initPollFds();
    int pipe_fds[2];
    int pipe_res = pipe(pipe_fds);
    if (pipe_res != 0) {
        perror("pipe:");
        exit(ERROR_PIPE_OPEN);
    }
    READ_STOP_FD = pipe_fds[0];
    WRITE_STOP_FD = pipe_fds[1];
    addFdToPollFds(READ_STOP_FD, POLLIN);

    struct sigaction sig_act = { 0 };
    sig_act.sa_handler = sigCatch;
    sigemptyset(&sig_act.sa_mask);
    int sigact_res = sigaction(SIGINT, &sig_act, NULL);
    if (sigact_res != 0) {
        perror("sigaction");
        cleanUp();
        exit(ERROR_SIG_HANDLER_INIT);
    }

    sigset_t old_set;
    sigset_t thread_set;
    sigemptyset(&thread_set);
    sigaddset(&thread_set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &thread_set, &old_set);
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        int create_res = pthread_create(&tids[i], NULL, threadFunc, NULL);
        if (create_res != 0) {
            is_created_thread[i] = false;
        }
        else {
            REAL_THREAD_POOL_SIZE += 1;
            is_created_thread[i] = true;
        }
    }
    pthread_sigmask(SIG_SETMASK, &old_set, NULL);

    initRwLockAttr();
    initClients();
    initServers();
    initCacheList();

    int listen_fd = initListener(port);
    addFdToPollFds(listen_fd, POLLIN);

    while (1) {
        //fprintf(stderr, "poll()\n");
        int poll_res = poll(poll_fds, poll_last_index, TIMEOUT * 1000);
        if (poll_res < 0) {
            perror("poll");
            break;
        }
        else if (poll_res == 0) {
            fprintf(stdout, "proxy timeout\n");
            break;
        }
        int num_handled_fd = 0;
        size_t i = 0;
        size_t prev_last_index = poll_last_index;
        /*fprintf(stderr, "poll_res = %d\n", poll_res);
        for (int j = 0; j < prev_last_index; j++) {
            fprintf(stderr, "poll_fds[%d] = %d : ", j, poll_fds[j].fd);
            if (poll_fds[j].revents & POLLIN) {
                fprintf(stderr, "POLLIN ");
            }
            if (poll_fds[j].revents & POLLOUT) {
                fprintf(stderr, "POLLOUT ");
            }
            fprintf(stderr, "\n");
        }*/
        while (num_handled_fd < poll_res && i < prev_last_index) {
            if (poll_fds[i].fd == READ_STOP_FD && (poll_fds[i].revents & POLLIN)) {
                cleanUp();
                exit(0);
            }
            if (poll_fds[i].fd == listen_fd && (poll_fds[i].revents & POLLIN)) {
                acceptNewClient(listen_fd);
                num_handled_fd += 1;
                i += 1;
                continue;
            }

            int client_num = findClientByFd(poll_fds[i].fd);
            int server_num = -1;
            if (client_num == -1) {
                server_num = findServerByFd(poll_fds[i].fd);
            }
            //fprintf(stderr, "poll_fds[%lu].fd = %d, client %d, server %d\n", i, poll_fds[i].fd, client_num, server_num);
            bool handled = false;
            if (poll_fds[i].revents & POLLIN) {
                if (client_num != -1) {
                    task_t *task = (task_t *)calloc(1, sizeof(task_t));
                    if (task == NULL) {
                        i += 1;
                        continue;
                    }
                    task->func = readFromClient;
                    task->arg = client_num;
                    //readFromClient(client_num);
                    int submit_res = submitTask(task_queue, task);
                    if (submit_res == 0) {
                        sync_pipe_notify(1);
                        pthread_mutex_lock(&tasks_counter_mutex);
                        tasks_submitted += 1;
                        pthread_mutex_unlock(&tasks_counter_mutex);
                    }
                }
                else if (server_num != -1) {
                    task_t *task = (task_t *)calloc(1, sizeof(task_t));
                    if (task == NULL) {
                        i += 1;
                        continue;
                    }
                    task->func = readFromServer;
                    task->arg = server_num;
                    //readFromServer(server_num);
                    int submit_res = submitTask(task_queue, task);
                    if (submit_res == 0) {
                        sync_pipe_notify(1);
                        pthread_mutex_lock(&tasks_counter_mutex);
                        tasks_submitted += 1;
                        pthread_mutex_unlock(&tasks_counter_mutex);
                    }
                }
                handled = true;
            }
            if (poll_fds[i].revents & POLLOUT) {
                if (client_num != -1) {
                    task_t *task = (task_t *)calloc(1, sizeof(task_t));
                    if (task == NULL) {
                        i += 1;
                        continue;
                    }
                    task->func = writeToClient;
                    task->arg = client_num;
                    //writeToClient(client_num);
                    int submit_res = submitTask(task_queue, task);
                    if (submit_res == 0) {
                        sync_pipe_notify(1);
                        pthread_mutex_lock(&tasks_counter_mutex);
                        tasks_submitted += 1;
                        pthread_mutex_unlock(&tasks_counter_mutex);
                    }
                }
                else if (server_num != -1) {
                    task_t *task = (task_t *)calloc(1, sizeof(task_t));
                    if (task == NULL) {
                        i += 1;
                        continue;
                    }
                    task->func = writeToServer;
                    task->arg = server_num;
                    //writeToServer(server_num);
                    int submit_res = submitTask(task_queue, task);
                    if (submit_res == 0) {
                        sync_pipe_notify(1);
                        pthread_mutex_lock(&tasks_counter_mutex);
                        tasks_submitted += 1;
                        pthread_mutex_unlock(&tasks_counter_mutex);
                    }
                }
                handled = true;
            }
            if (client_num == -1 && server_num == -1 && poll_fds[i].fd != listen_fd && poll_fds[i].fd != READ_STOP_FD) {
                removeFromPollFds(poll_fds[i].fd);
            }
            if (handled) {
                num_handled_fd += 1;
            }
            i += 1;
        }

        pthread_mutex_lock(&tasks_counter_mutex);
        int err = 0;
        while (tasks_submitted != tasks_completed && err == 0) {
            err = pthread_cond_wait(&cond, &tasks_counter_mutex);
        }
        tasks_submitted = 0;
        tasks_completed = 0;
        pthread_mutex_unlock(&tasks_counter_mutex);

        //printCacheList();
    }
    cleanUp();
    return 0;
}

