#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../utilities/sync_pipe/sync_pipe.h" // my own library
/*
 * To use the library
 *
 * You need to run such command
 * $ export LD_LIBRARY_PATH="LD_LIBRARY_PATH:[path]"
 *
 * or add such line to /.bashrc
 * export LD_LIBRARY_PATH="[path]"
 *
 * where [path] is path to folder: utilities
 *
 */

#define ERROR_ALLOC 1
#define ERROR_READ 2
#define ERROR_INVALID_ARGUMENTS 3

#define BUF_SIZE BUFSIZ

/*
 *
 * how list_t looks
 *
 *  head                                         tail
 *   |                                            |
 *   V                                            V
 * +------+------+    +------+------+           +------+------+
 * | data | next | -> | data | next | -> ... -> | data | next | -> NULL
 * +------+------+    +------+------+           +------+------+
 *
 *
 */

typedef struct node_t {
    char *string;
    struct node_t *next;
    size_t s_len;
} node_t;

typedef struct list_t {
    struct node_t *head;
    struct node_t *tail;
    pthread_mutex_t tail_mutex;
    size_t size;
} list_t;

list_t *final_list;
int coef = 30000;

list_t *initList() {
    list_t *list = (list_t *) calloc(1, sizeof(list_t)); // Thread-safety
    if (list == NULL) {
        perror("Error: calloc returned NULL");
        pthread_exit((void *) ERROR_ALLOC);
    }
    list->size = 0;
    pthread_mutex_init(&list->tail_mutex, NULL);
    list->head = NULL;
    list->tail = NULL;
    return list;
}

node_t *createNode(char *s, size_t s_len, pthread_mutex_t *mutex) {
    node_t *new_node = (node_t *)calloc(1, sizeof(node_t));
    if (new_node == NULL) {
        perror("Error: calloc returned NULL");
        pthread_mutex_unlock(mutex);
        pthread_exit((void *) ERROR_ALLOC);
    }
    new_node->string = (char *)calloc(s_len, sizeof(char));
    if (new_node->string == NULL && s_len != 0) {
        perror("Error: calloc returned NULL");
        pthread_mutex_unlock(mutex);
        pthread_exit((void *)ERROR_ALLOC);
    }
    memcpy(new_node->string, s, s_len); // thread_safety
    new_node->next = NULL;
    new_node->s_len = s_len;
    return new_node;
}

void addNodeToList(list_t *list, node_t *node) {
    if (list == NULL || node == NULL) {
        return;
    }
    pthread_mutex_lock(&list->tail_mutex);
    if (list->tail == NULL) {
        list->head = node;
        list->head->next = NULL;
        list->tail = list->head;
    }
    else {
        list->tail->next = node;
        list->tail->next->next = NULL;
        list->tail = list->tail->next;
    }
    list->size += 1;
    pthread_mutex_unlock(&list->tail_mutex);
}

void addStringToList(list_t *list, char *s, size_t s_len) {
    if (list == NULL) {
        return;
    }
    pthread_mutex_lock(&list->tail_mutex);
    if (list->tail == NULL) {
        list->head = createNode(s, s_len, &list->tail_mutex);
        list->tail = list->head;
    }
    else {
        list->tail->next = createNode(s, s_len, &list->tail_mutex);
        list->tail = list->tail->next;
    }
    list->size += 1;
    pthread_mutex_unlock(&list->tail_mutex);
}

void destroyList(list_t *list) {
    if (list == NULL) {
        return;
    }
    pthread_mutex_destroy(&list->tail_mutex);
    while (list->head != NULL) {
        node_t *tmp = list->head->next;
        free(list->head->string);
        free(list->head);
        list->head = tmp;
    }
    free(list);
}

void safePrintList(list_t *list) {
    pthread_mutex_lock(&list->tail_mutex);
    node_t *list_nodes = list->head;
    while (list_nodes != NULL) {
        ssize_t was_printed = 0;
        while (was_printed < list_nodes->s_len) {
            ssize_t written = 0;
            if (list_nodes->s_len - was_printed < BUFSIZ) {
                written = write(1, &list_nodes->string[was_printed], list_nodes->s_len - was_printed);
            }
            else {
                written = write(1, &list_nodes->string[was_printed],BUFSIZ);
            }
            if (written < 0) {
                perror("Error write in safePrintList");
            }
            else {
                was_printed += written;
            }
        }
        pthread_mutex_unlock(&list->tail_mutex);

        pthread_mutex_lock(&list->tail_mutex);
        list_nodes = list_nodes->next;
    }
    pthread_mutex_unlock(&list->tail_mutex);
}

void unsafePrintList(list_t *list) {
    node_t *list_nodes = list->head;
    while (list_nodes != NULL) {
        ssize_t was_printed = 0;
        while (was_printed < list_nodes->s_len) {
            ssize_t written = 0;
            if (list_nodes->s_len - was_printed < BUFSIZ) {
                written = write(1, &list_nodes->string[was_printed], list_nodes->s_len - was_printed);
            }
            else {
                written = write(1, &list_nodes->string[was_printed],BUFSIZ);
            }
            if (written < 0) {
                perror("Error write in safePrintList");
            }
            else {
                was_printed += written;
            }
        }
        list_nodes = list_nodes->next;
    }
}

void *mythread(void *arg) {
    node_t *node = (node_t *)arg;

    sync_pipe_wait();

    usleep(node->s_len * coef);
    addNodeToList(final_list, node);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Error: wrong amount of arguments\n");
        exit(ERROR_INVALID_ARGUMENTS);
    }
    coef = atoi(argv[1]);
    size_t max_buf_size = BUF_SIZE;
    char buf[BUF_SIZE];
    char *string = (char *) calloc(max_buf_size, sizeof(char));
    size_t index = 0;
    list_t *list = initList();

    while (1) {
        char *res = fgets(buf, BUF_SIZE, stdin);
        if (feof(stdin)) {
            break;
        }
        if (res == NULL) {
            perror("Error while reading data");
            exit(ERROR_READ);
        }
        size_t s_len = strlen(buf);
        if (s_len + index >= max_buf_size - 1) {
            max_buf_size *= 2;
            string = realloc(string, max_buf_size * sizeof(char));
            if (string == NULL) {
                perror("Error: realloc returned NULL");
                exit(ERROR_ALLOC);
            }
        }
        memcpy(&string[index], buf, BUF_SIZE * sizeof(char));
        index += s_len;
        if (strchr(buf, '\n') != NULL) {
            addStringToList(list, string, strlen(string));
            index = 0;
            memset(string, 0, max_buf_size * sizeof(char));
        }
    }
    free(string);

    sync_pipe_init();
    size_t num_threads_to_create = list->size;
    pthread_t tids[num_threads_to_create];
    bool was_created[num_threads_to_create];

    final_list = initList();
    node_t *tmp_nodes = list->head; // tmp_nodes will be used by threads to create final_list
    list->head = NULL; // list will consist of the nodes prepared for threads which wouldn't be created
    list->tail = NULL;
    list->size = 0;

    size_t i = 0;
    int really_created = 0;
    while (tmp_nodes != NULL) {
        int create_res = pthread_create(&tids[i], NULL, mythread, tmp_nodes);
        if (create_res != 0) {
            fprintf(stderr, "Error while creating thread %zu: %s\n", i, strerror(create_res));
            was_created[i] = false;
            addNodeToList(list, tmp_nodes);
        }
        else {
            really_created += 1;
            was_created[i] = true;
        }
        i += 1;
        tmp_nodes = tmp_nodes->next;
    }
    //fprintf(stderr, "THREADS CREATED\n");
    //printf("\n==========LINES==========\n\n");

    sync_pipe_notify(really_created);
    destroyList(list);

    for (size_t j = 0; j < num_threads_to_create; j++) {
        if (was_created[j]) {
            int join_res = pthread_join(tids[j], NULL);
            if (join_res != 0) {
                fprintf(stderr, "Error while joining thread %zu: %s\n", j, strerror(join_res));
            }
        }
    }

    unsafePrintList(final_list);

    sync_pipe_close();
    destroyList(final_list);
    return 0;
}
