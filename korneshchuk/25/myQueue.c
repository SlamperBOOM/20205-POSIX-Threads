#include <stdlib.h>
#include "thread_safe_queue.c"
#include <semaphore.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>

typedef struct MyQueue {
    Queue* queue;
    sem_t full_sem;
    sem_t empty_sem;
    pthread_mutex_t mutex;
    bool dropped;
} MyQueue;

void my_msg_init(MyQueue* myQueue) {
    myQueue->queue = create();
    myQueue->dropped = false;
    sem_init(&myQueue->full_sem, 0, 10);
    sem_init(&myQueue->empty_sem, 0, 0);
    pthread_mutex_init(&myQueue->mutex, NULL);
}

void my_msg_drop(MyQueue* myQueue) {
    pthread_mutex_lock(&myQueue->mutex);
    myQueue->dropped = true;
    sem_post(&myQueue->empty_sem);
    sem_post(&myQueue->full_sem);
    pthread_mutex_unlock(&myQueue->mutex);
}

void my_msg_destroy(MyQueue* myQueue) {
    destroy(myQueue->queue);
    sem_destroy(&myQueue->full_sem);
    sem_destroy(&myQueue->empty_sem);
    pthread_mutex_destroy(&myQueue->mutex);
    free(myQueue);
}

int my_msg_put(MyQueue* myQueue, char* msg) {
    sem_wait(&myQueue->full_sem);

    int status;
    if ((status = pthread_mutex_lock(&myQueue->mutex)) != 0) {
        printf("error in my_msg_get.pthread_mutex_lock. %d", status);
        sem_post(&myQueue->full_sem);
        return 0;
    }

    if (myQueue->dropped) {
        sem_post(&myQueue->full_sem);
        pthread_mutex_unlock(&myQueue->mutex);
        return 0;
    }

    int msg_len = (int)strlen(msg);
    int new_msg_len;
    if (msg_len > 80) {
        new_msg_len = 80;
    }
    else {
        new_msg_len = msg_len;
    }

    char* str = (char*)calloc(new_msg_len + 1, sizeof(char));
    strncpy(str, msg, new_msg_len);
    str[new_msg_len] = 0;

    push(myQueue->queue, str);

    sem_post(&myQueue->empty_sem);
    pthread_mutex_unlock(&myQueue->mutex);
    return msg_len;
}

int my_msg_get(MyQueue* myQueue, char *buf, size_t bufsize) {
    sem_wait(&myQueue->empty_sem);

    int status;
    if ((status = pthread_mutex_lock(&myQueue->mutex)) != 0) {
        printf("error in my_msg_get.pthread_mutex_lock. %d", status);
        sem_post(&myQueue->empty_sem);
        return 0;
    }

    if (!myQueue->dropped) {
        char* tmp = pop(myQueue->queue);
        if (tmp != NULL) {
            tmp[bufsize] = 0;
            strcpy(buf, tmp);

            sem_post(&myQueue->full_sem);
            pthread_mutex_unlock(&myQueue->mutex);
            return  (int)strlen(buf);
        }
    }

    sem_post(&myQueue->empty_sem);
    pthread_mutex_unlock(&myQueue->mutex);
    return 0;
}