#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include "myQueue.c"
#include <unistd.h>
#define BUFFER_SIZE 80
#include <stdarg.h>


void* producer_routine(void* args) {
    MyQueue* queue = (MyQueue*)args;

    while(!queue->dropped) {
        char* str = (char*)calloc(BUFFER_SIZE + 1, sizeof(char));
        for(int i = 0; i < BUFFER_SIZE; i++) {
            str[i] = (char)((rand() % ('z' - 'A')) + 'A');
        }
        str[BUFFER_SIZE] = 0;

        printf("put %s\n", str);
        int result = my_msg_put(queue, str);
        free(str);

        if(result == 0){
            pthread_exit((void*)NULL);
        }

    }

    pthread_exit((void*)NULL);
}

void* consumer_routine(void* args) {
    MyQueue* queue = (MyQueue*)args;

    while(!queue->dropped) {
        char* chr = malloc((BUFFER_SIZE + 1) *  sizeof(char));
        int result = my_msg_get(queue, chr, BUFFER_SIZE);
        printf("result is %d. Got %s\n", result, chr);
        free(chr);

        if(result == 0){
            pthread_exit(NULL);
        }
    }
    pthread_exit(NULL);
}

int join_threads(int n, ...) {
    va_list list;
    va_start(list, n);
    for (int i = 0; i < n; ++i) {
        pthread_t* tid = va_arg(list, pthread_t*);
        if (pthread_join(*tid, NULL) != 0) {
            printf("can't join thread %lu", *tid);
            pthread_exit(NULL);
        }
    }
    va_end(list);
    return 0;
}


int init_thread(pthread_t* tid, void *(*start_routine) (void *), MyQueue* queue) {
    if (pthread_create(tid, NULL, start_routine, (void*)queue) != 0) {
        queue->dropped = true;
        printf("can't create thread %lu", *tid);
        return -1;
    }
    return 0;
}


void init_all_threads(
        pthread_t* producer1,
        pthread_t* producer2,
        pthread_t* consumer1,
        pthread_t* consumer2,
        MyQueue* queue
        ) {
    if (init_thread(producer1, producer_routine, queue) != 0) {
        join_threads(0);
        pthread_exit(NULL);
    }

    if (init_thread(producer2, producer_routine, queue) != 0) {
        join_threads(1, producer1);
        pthread_exit(NULL);
    }

    if (init_thread(consumer1, consumer_routine, queue) != 0) {
        join_threads(2, producer1, producer2);
        pthread_exit(NULL);
    }

    if (init_thread(consumer2, consumer_routine, queue) != 0) {
        join_threads(3, producer1, producer2, consumer1);
        pthread_exit(NULL);
    }
}

int main() {
    MyQueue* queue = malloc(sizeof(MyQueue));
    my_msg_init(queue);

    pthread_t producer1;
    pthread_t producer2;
    pthread_t consumer1;
    pthread_t consumer2;
    init_all_threads(&producer1,&producer2, &consumer1, &consumer2, queue);

    sleep(5);
    my_msg_drop(queue);
    join_threads(4, producer1, producer2, consumer1, consumer2);
    printf("\nQueue was dropped\n");

    my_msg_destroy(queue);
    pthread_exit(NULL);
}