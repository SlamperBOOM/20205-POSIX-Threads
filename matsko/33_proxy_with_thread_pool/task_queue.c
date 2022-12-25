#include "task_queue.h"
#include <malloc.h>

task_queue_t *initTaskQueue() {
    task_queue_t *ret = (task_queue_t *) calloc(1, sizeof(task_queue_t));
    if (ret == NULL) {
        fprintf(stderr, "failed to alloc memory for task_queue\n");
        return NULL;
    }
<<<<<<< Updated upstream
    pthread_mutex_init(&ret->head_mutex, NULL);
    pthread_mutex_init(&ret->tail_mutex, NULL);
=======
    pthread_mutex_init(&ret->queue_mutex, NULL);
>>>>>>> Stashed changes
    ret->head = NULL;
    ret->tail = NULL;
    return ret;
}

void destroyTaskQueue(task_queue_t *queue) {
    //fprintf(stderr, "destroyTaskQueue\n");
    if (queue == NULL) {
        return;
    }
<<<<<<< Updated upstream
    pthread_mutex_lock(&queue->head_mutex);
    pthread_mutex_lock(&queue->tail_mutex);
    task_queue_node_t *nodes = queue->head;
    while (nodes != NULL) {
        task_queue_node_t *next_node = nodes->next;
        free(nodes->task);
=======
    pthread_mutex_lock(&queue->queue_mutex);
    task_queue_node_t *nodes = queue->head;
    while (nodes != NULL) {
        task_queue_node_t *next_node = nodes->next;
>>>>>>> Stashed changes
        free(nodes);
        nodes = next_node;
    }
    queue->tail = NULL;
    queue->head = NULL;
<<<<<<< Updated upstream
    pthread_mutex_unlock(&queue->head_mutex);
    pthread_mutex_unlock(&queue->tail_mutex);
    pthread_mutex_destroy(&queue->head_mutex);
    pthread_mutex_destroy(&queue->tail_mutex);
=======
    pthread_mutex_unlock(&queue->queue_mutex);
    pthread_mutex_destroy(&queue->queue_mutex);
>>>>>>> Stashed changes
    free(queue);
    //*queue = NULL;
}

<<<<<<< Updated upstream
int submitTask(task_queue_t *queue, task_t *task) {
    if (queue == NULL || task == NULL) {
        if (queue == NULL) {
            fprintf(stderr, "queue is NULL\n");
        }
        if (task == NULL) {
            fprintf(stderr, "task is NULL\n");
        }
=======
int submitTask(task_queue_t *queue, int arg) {
    if (queue == NULL) {
        fprintf(stderr, "queue is NULL\n");
>>>>>>> Stashed changes
        return -1;
    }
    task_queue_node_t *new_node = (task_queue_node_t *) calloc(1, sizeof(task_queue_node_t));
    if (new_node == NULL) {
        fprintf(stderr, "failed to alloc memory for node\n");
        return -1;
    }
<<<<<<< Updated upstream
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
=======
    new_node->arg = arg;
    new_node->next = NULL;
    pthread_mutex_lock(&queue->queue_mutex);
    if (queue->head == NULL) {
        queue->head = new_node;
        queue->tail = queue->head;
        pthread_mutex_unlock(&queue->queue_mutex);
        return 0;
    }
    queue->tail->next = new_node;
    queue->tail = queue->tail->next;
    pthread_mutex_unlock(&queue->queue_mutex);
    return 0;
}

int popTask(task_queue_t *queue) {
    //fprintf(stderr, "popTask\n");
    if (queue == NULL) {
        return -1;
    }
    pthread_mutex_lock(&queue->queue_mutex);
    if (queue->head == queue->tail) {
        if (queue->head == NULL) {
            pthread_mutex_unlock(&queue->queue_mutex);
            return -1;
>>>>>>> Stashed changes
        }
        else {
            task_queue_node_t *first_node = queue->head;
            queue->head = NULL;
            queue->tail = NULL;
<<<<<<< Updated upstream
            pthread_mutex_unlock(&queue->head_mutex);
            pthread_mutex_unlock(&queue->tail_mutex);
            task_t *ret = first_node->task;
=======
            pthread_mutex_unlock(&queue->queue_mutex);
            int ret = first_node->arg;
>>>>>>> Stashed changes
            free(first_node);
            return ret;
        }
    }
    else {
<<<<<<< Updated upstream
        pthread_mutex_unlock(&queue->tail_mutex);
        task_queue_node_t *first_node = queue->head;
        queue->head = queue->head->next;
        pthread_mutex_unlock(&queue->head_mutex);
        task_t *ret = first_node->task;
=======
        task_queue_node_t *first_node = queue->head;
        queue->head = queue->head->next;
        pthread_mutex_unlock(&queue->queue_mutex);
        int ret = first_node->arg;
>>>>>>> Stashed changes
        free(first_node);
        return ret;
    }
}

void printTaskQueue(task_queue_t *queue) {
<<<<<<< Updated upstream
    pthread_mutex_lock(&queue->head_mutex);
    pthread_mutex_lock(&queue->tail_mutex);
=======
    pthread_mutex_lock(&queue->queue_mutex);
>>>>>>> Stashed changes

    task_queue_node_t *nodes = queue->head;
    while (nodes != NULL) {
        task_queue_node_t *next_node = nodes->next;
<<<<<<< Updated upstream
        fprintf(stderr, "in node: task->arg = %d\n", nodes->task->arg);
        nodes = next_node;
    }

    pthread_mutex_unlock(&queue->head_mutex);
    pthread_mutex_unlock(&queue->tail_mutex);
=======
        fprintf(stderr, "in node: task->arg = %d\n", nodes->arg);
        nodes = next_node;
    }

    pthread_mutex_unlock(&queue->queue_mutex);
>>>>>>> Stashed changes
}
