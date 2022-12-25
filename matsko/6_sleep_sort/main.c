#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ERROR_ALLOC 1
#define ERROR_READ 2
#define ERROR_INVALID_ARGUMENTS 3

#define BUF_SIZE BUFSIZ

int start_fd;
int coef = 30000;

typedef struct list_t {
    char *string;
    struct list_t *next;
    size_t s_len;
} list_t;

void *mythread(void *arg) {
    list_t *node = (list_t *)arg;
    char buf;
    ssize_t was_read = read(start_fd, &buf, 1);
    if (was_read < 0) {
        perror("Error read for synchronization");
    }
    usleep(node->s_len * coef);
    fputs(node->string, stdout);
    pthread_exit(NULL);
}

list_t *createNode(char *s, size_t s_len) {
    list_t *new_list = (list_t *)calloc(1, sizeof(list_t));
    if (new_list == NULL) {
        perror("Error: malloc returned NULL");
        exit(ERROR_ALLOC);
    }
    new_list->string = (char *)calloc(s_len, sizeof(char));
    if (new_list->string == NULL && s_len != 0) {
        perror("Error: malloc returned NULL");
        exit(ERROR_ALLOC);
    }
    memcpy(new_list->string, s, s_len);
    new_list->next = NULL;
    new_list->s_len = s_len;
    return new_list;
}

void add(list_t **list, char *s, size_t s_len) {
    if (*list == NULL) {
        *list = createNode(s, s_len);
        return;
    }
    list_t *tmp = *list;
    while ((*list)->next != NULL) {
        *list = (*list)->next;
    }
    (*list)->next = createNode(s, s_len);
    *list = tmp;
}

void destroyList(list_t *list) {
    while (list != NULL) {
        list_t *tmp = list->next;
        free(list->string);
        free(list);
        list = tmp;
    }
}

void printList(list_t *list) {
    while (list != NULL) {
        fputs(list->string, stdout);
        list = list->next;
    }
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
    list_t *list = NULL;
    size_t list_size = 0;

    while (1) {
        char *res = fgets(buf, BUF_SIZE, stdin);
        if (feof(stdin)) {
            break;
        }
        if (res == NULL && !feof(stdin)) {
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
            add(&list, string, strlen(string) + 1);
            index = 0;
            list_size += 1;
            memset(string, 0, max_buf_size * sizeof(char));
        }
    }
    //printf("\n==========LINES==========\n\n");
    pthread_t tids[list_size];
    bool was_created[list_size];
    list_t *tmp = list;

    int pipe_fds[2];
    int pipe_res = pipe(pipe_fds);
    if (pipe_res != 0) {
        perror("Error pipe():");
    }
    start_fd = pipe_fds[0];
    int write_fd = pipe_fds[1];

    size_t i = 0;
    int really_created = 0;
    while (tmp != NULL) {
        int create_res = pthread_create(&tids[i], NULL, mythread, tmp);
        if (create_res != 0) {
            fprintf(stderr, "Error while creating thread %zu: %s\n", i, strerror(create_res));
            was_created[i] = false;
        }
        else {
            really_created += 1;
            was_created[i] = true;
        }
        i += 1;
        tmp = tmp->next;
    }

    //fprintf(stderr, "THREADS CREATED\n");
    char start_buf[BUFSIZ];
    ssize_t bytes_written = 0;
    while (bytes_written < really_created) {
        ssize_t written = 0;
        if (really_created - bytes_written <= BUFSIZ) {
            written = write(write_fd, start_buf, really_created - bytes_written);
        }
        else {
            written = write(write_fd, start_buf, BUFSIZ);
        }
        if (written < 0) {
            perror("Error write");
            fprintf(stderr, "bytes_written: %ld / %d\n", bytes_written, really_created);
        }
        else {
            bytes_written += written;
        }
    }

    for (size_t j = 0; j < list_size; j++) {
        if (was_created[j]) {
            int join_res = pthread_join(tids[j], NULL);
            if (join_res != 0) {
                fprintf(stderr, "Error while joining thread %zu: %s\n", j, strerror(join_res));
            }
        }
    }

    close(start_fd);
    close(write_fd);
    destroyList(list);
    free(string);
    return 0;
}
