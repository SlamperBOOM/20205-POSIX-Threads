#ifndef INC_33_PROXY_WITH_THREAD_POOL_TASK_QUEUE_H
#define INC_33_PROXY_WITH_THREAD_POOL_TASK_QUEUE_H

#include <pthread.h>

typedef struct task {
    void (*func)(int arg);
    int arg;
} task_t;

typedef struct task_queue_node {
    task_t *task;
    struct task_queue_node *next;
} task_queue_node_t;

typedef struct task_queue {
    task_queue_node_t *head;
    task_queue_node_t *tail;
    pthread_mutex_t head_mutex;
    pthread_mutex_t tail_mutex;
} task_queue_t;

task_queue_t *initTaskQueue();
void destroyTaskQueue(task_queue_t *queue);
int submitTask(task_queue_t *queue, task_t *task);
task_t *popTask(task_queue_t *queue);
void printTaskQueue(task_queue_t *queue);

#endif //INC_33_PROXY_WITH_THREAD_POOL_TASK_QUEUE_H
