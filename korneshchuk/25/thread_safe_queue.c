#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

typedef struct Node {
    struct Node* next;
    char* value;
} Node;

typedef struct Queue {
    Node* head;
    Node* tail;
    pthread_mutex_t mutex;
} Queue;

Queue* create() {
    Queue* queue = malloc(sizeof(*queue));
    queue->head = NULL;
    queue->tail = NULL;

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    queue->mutex = mutex;

    return queue;
}

void push(Queue* queue, char* elem) {
    Node* element = malloc(sizeof(*element));
    element->value = elem;
    element->next = NULL;

    int status;
    if ((status = pthread_mutex_lock(&queue->mutex)) != 0) {
        printf("error in push.pthread_mutex_lock() %d", status);
        return;
    }

    if (queue->head == NULL) {
        queue->head = element;
        queue->tail = element;
    }
    else {
        Node* oldTail = queue->tail;
        oldTail->next = element;
        queue->tail = element;
    }
    pthread_mutex_unlock(&queue->mutex);
}

char* pop(Queue *queue) {
    int status;
    if ((status = pthread_mutex_lock(&queue->mutex)) != 0) {
        printf("error in pop.pthread_mutex_lock() %d", status);
        return NULL;
    }

    Node *head = queue->head;

    if (head == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    else {
        queue->head = head->next;

        char* value = head->value;
        free(head);

        pthread_mutex_unlock(&queue->mutex);
        return value;
    }
}

void destroy(Queue *queue) {
    while (queue->head != NULL) {
        char* tmp = pop(queue);
        free(tmp);
    }

    pthread_mutex_destroy(&queue->mutex);
    free(queue);
}
