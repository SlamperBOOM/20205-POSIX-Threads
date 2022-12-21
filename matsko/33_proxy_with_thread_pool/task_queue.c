#include "task_queue.h"
#include <malloc.h>

task_queue_t *initTaskQueue() {
    task_queue_t *ret = (task_queue_t *) calloc(1, sizeof(task_queue_t));
    if (ret == NULL) {
        fprintf(stderr, "failed to alloc memory for task_queue\n");
        return NULL;
    }
    pthread_mutex_init(&ret->head_mutex, NULL);
    pthread_mutex_init(&ret->tail_mutex, NULL);
    ret->head = NULL;
    ret->tail = NULL;
    return ret;
}

void destroyTaskQueue(task_queue_t *queue) {
    //fprintf(stderr, "destroyTaskQueue\n");
    if (queue == NULL) {
        return;
    }
    pthread_mutex_lock(&queue->head_mutex);
    pthread_mutex_lock(&queue->tail_mutex);
    task_queue_node_t *nodes = queue->head;
    while (nodes != NULL) {
        task_queue_node_t *next_node = nodes->next;
        free(nodes->task);
        free(nodes);
        nodes = next_node;
    }
    queue->tail = NULL;
    queue->head = NULL;
    pthread_mutex_unlock(&queue->head_mutex);
    pthread_mutex_unlock(&queue->tail_mutex);
    pthread_mutex_destroy(&queue->head_mutex);
    pthread_mutex_destroy(&queue->tail_mutex);
    free(queue);
    //*queue = NULL;
}

int submitTask(task_queue_t *queue, task_t *task) {
    if (queue == NULL || task == NULL) {
        if (queue == NULL) {
            fprintf(stderr, "queue is NULL\n");
        }
        if (task == NULL) {
            fprintf(stderr, "task is NULL\n");
        }
        return -1;
    }
    task_queue_node_t *new_node = (task_queue_node_t *) calloc(1, sizeof(task_queue_node_t));
    if (new_node == NULL) {
        fprintf(stderr, "failed to alloc memory for node\n");
        return -1;
    }
    new_node->task = task;
    new_node->next = NULL;
    pthread_mutex_lock(&queue->head_mutex);
    pthread_mutex_lock(&queue->tail_mutex);
    if (queue->head == queue->tail) {
        if (queue->head == NULL) {
            queue->head = new_node;
            queue->tail = queue->head;
            pthread_mutex_unlock(&queue->head_mutex);
            pthread_mutex_unlock(&queue->tail_mutex);
            return 0;
        }
        else {
            queue->head->next = new_node;
            queue->tail = queue->head->next;
            pthread_mutex_unlock(&queue->head_mutex);
            pthread_mutex_unlock(&queue->tail_mutex);
            return 0;
        }
    }
    else {
        pthread_mutex_unlock(&queue->head_mutex);
        queue->tail->next = new_node;
        queue->tail = queue->tail->next;
        pthread_mutex_unlock(&queue->tail_mutex);
        return 0;
    }
}

task_t *popTask(task_queue_t *queue) {
    //fprintf(stderr, "popTask\n");
    if (queue == NULL) {
        return NULL;
    }
    pthread_mutex_lock(&queue->head_mutex);
    pthread_mutex_lock(&queue->tail_mutex);
    if (queue->head == queue->tail) {
        if (queue->head == NULL) {
            pthread_mutex_unlock(&queue->head_mutex);
            pthread_mutex_unlock(&queue->tail_mutex);
            return NULL;
        }
        else {
            task_queue_node_t *first_node = queue->head;
            queue->head = NULL;
            queue->tail = NULL;
            pthread_mutex_unlock(&queue->head_mutex);
            pthread_mutex_unlock(&queue->tail_mutex);
            task_t *ret = first_node->task;
            free(first_node);
            return ret;
        }
    }
    else {
        pthread_mutex_unlock(&queue->tail_mutex);
        task_queue_node_t *first_node = queue->head;
        queue->head = queue->head->next;
        pthread_mutex_unlock(&queue->head_mutex);
        task_t *ret = first_node->task;
        free(first_node);
        return ret;
    }
}

void printTaskQueue(task_queue_t *queue) {
    pthread_mutex_lock(&queue->head_mutex);
    pthread_mutex_lock(&queue->tail_mutex);

    task_queue_node_t *nodes = queue->head;
    while (nodes != NULL) {
        task_queue_node_t *next_node = nodes->next;
        fprintf(stderr, "in node: task->arg = %d\n", nodes->task->arg);
        nodes = next_node;
    }

    pthread_mutex_unlock(&queue->head_mutex);
    pthread_mutex_unlock(&queue->tail_mutex);
}
