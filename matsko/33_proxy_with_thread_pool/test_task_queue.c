#include "task_queue.h"
#include <stdio.h>
#include <stdbool.h>
#include <malloc.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define ARRAY_SIZE 50
#define THREAD_POOL_SIZE 4

int start_fd;
int write_fd;

int sync_pipe_init() {
    int pipe_fds[2];
    int pipe_res = pipe(pipe_fds);
    if (pipe_res != 0) {
        perror("Error pipe():");
    }
    start_fd = pipe_fds[0];
    write_fd = pipe_fds[1];
    return pipe_res;
}

void sync_pipe_close() {
    close(start_fd);
    close(write_fd);
}

int sync_pipe_wait() {
    char buf;
    ssize_t was_read = read(start_fd, &buf, 1);
    if (was_read < 0) {
        perror("Error read for synchronization");
    }
    return (int)was_read;
}

void sync_pipe_notify(int num_really_created_threads) {
    char start_buf[BUFSIZ];
    ssize_t bytes_written = 0;
    while (bytes_written < num_really_created_threads) {
        ssize_t written = 0;
        if (num_really_created_threads - bytes_written <= BUFSIZ) {
            written = write(write_fd, start_buf, num_really_created_threads - bytes_written);
        }
        else {
            written = write(write_fd, start_buf, BUFSIZ);
        }
        if (written < 0) {
            perror("Error write");
            fprintf(stderr, "bytes_written: %ld / %d\n", bytes_written, num_really_created_threads);
        }
        else {
            bytes_written += written;
        }
    }
}

int array[ARRAY_SIZE];
pthread_t tids[THREAD_POOL_SIZE];
task_queue_t *queue;
bool is_stop = false;

void printFunc(int arg) {
    //usleep(500);
    fprintf(stderr, "%d\n", arg);
}

void *threadFunc(void *arg) {
    /*fprintf(stderr, "go to cycle\n");
    if (is_stop) {
        fprintf(stderr, "is_stop = true\n");
    }
    else {
        fprintf(stderr, "is_stop = false\n");
    }*/
    while (!is_stop) {
        //fprintf(stderr, "waiting on pipe...\n");
        sync_pipe_wait();
        task_t *task = popTask(queue);
        if (task != NULL) {
            task->func(task->arg);
            free(task);
        }
        else {
            fprintf(stderr, "task is NULL\n");
        }
    }
    pthread_exit((void *)0);
}

int main(int argc, char *argv[]) {
    fprintf(stderr, "starting...\n");
    for (int i = 0; i < ARRAY_SIZE; i++) {
        array[i] = i;
    }
    queue = initTaskQueue();
    if (queue != NULL) {
        fprintf(stderr, "init task queue success\n");
    }
    else {
        fprintf(stderr, " failed to init task queue\n");
    }
    sync_pipe_init();
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        int create_res = pthread_create(&tids[i], NULL, threadFunc, NULL);
        if (create_res != 0) {
            fprintf(stderr, "Error while creating thread %d: %s\n", i, strerror(create_res));
        }
    }
    sleep(1);
    int submitted = 0;
    for (int i = 0; i < ARRAY_SIZE; i++) {
        task_t *task = (task_t *)calloc(1, sizeof(task_t));
        if (task == NULL) {
            fprintf(stderr, "failed to create task for %d iteration\n", i);
            continue;
        }
        task->func = printFunc;
        task->arg = array[i];
        int submit_res = submitTask(queue, task);
        if (submit_res == 0) {
            submitted += 1;
        }
        else {
            fprintf(stderr, "failed to submit task\n");
        }
    }
    sync_pipe_notify(submitted);
    destroyTaskQueue(queue);
    //printTaskQueue(queue);
    sleep(10);

    //is_stop = true;
}
