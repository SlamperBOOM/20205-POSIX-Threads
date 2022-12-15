#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

// [0] - A detail
// [1] - B detail
// [2] - C detail
// [4] - module
sem_t workflow_sems[4];
int running = 1;
struct timespec ts;

void DestroySems() {
    int sems_len = sizeof(workflow_sems) / sizeof(sem_t);
    for (int i = 0; i < sems_len; ++i) {
        int err = sem_destroy(&workflow_sems[i]);
        if (err != 0) {
            perror("Error while destoying semaphore: SEM_DESTROY(3C)\n");
        }
    }
}

void SigintHandle() {
    running = 0;
    sem_post(&workflow_sems[0]);
    sem_post(&workflow_sems[0]);
    sem_post(&workflow_sems[1]);
    sem_post(&workflow_sems[2]);
    DestroySems();
}

void* CreateA(void* arg) {
    while (running) {
        sleep(1);
        sem_post(&workflow_sems[0]);
        printf("Detail A created!\n");
    }
    return NULL;
}

void* CreateB(void* arg) {
    while (running) {
        sleep(2);
        sem_post(&workflow_sems[1]);
        printf("Detail B created!\n");
    }
    return NULL;
}

void* CreateC(void* arg) {
    while (running) {
        sleep(3);
        sem_post(&workflow_sems[2]);
        printf("Detail C created!\n");
    }
    return NULL;
}

void* CreateModule(void* arg) {
    while (running) {
        sem_wait(&workflow_sems[0]);
        sem_wait(&workflow_sems[1]);
        sem_post(&workflow_sems[3]);
        printf("Module created!\n");
    }
    return NULL;
}

void* CreateWidget() {
    while (running) {
        printf("====================\n");
        sem_wait(&workflow_sems[3]);
        sem_wait(&workflow_sems[2]);
        printf("Widget created!\n");
        printf("====================\n");
    }
    return NULL;
}

int InitSems() {
    int sems_len = sizeof(workflow_sems) / sizeof(sem_t);
    for (int i = 0; i < sems_len; ++i) {
        int err = sem_init(&workflow_sems[i], 0, 0);
        if (err != 0) {
            perror("Error while destoying semaphore: SEM_INIT(3C)\n");
            return -1;
        }
    }
    return 0;
}

int main() {
    struct sigaction sa;
    sa.sa_handler = SigintHandle;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    pthread_t workflow_threads[5];
    void* (*func_arr[5])(void*) = {CreateA, CreateB, CreateC, CreateModule, CreateWidget};

    ts.tv_sec += 4;

    int err = InitSems();
    if (err != -1) {
        DestroySems();
    }

    int threads_num = sizeof(workflow_threads) / sizeof(pthread_t);
    for (int i = 0; i < threads_num; ++i) {
        err = pthread_create(&workflow_threads[i], NULL, func_arr[i], NULL);
        if (err != 0) {
            fprintf(stderr, "Error while creating workflow_threads[%d]: PTHREAD_CREATE(3C): %s\n", i, strerror(err));
            DestroySems();
            pthread_exit(NULL);
        }
    }

    for (int i = 0; i < threads_num; ++i) {
        err = pthread_join(workflow_threads[i], NULL);
        if (err != 0) {
            fprintf(stderr, "Error while joining workflow_threads[%d]: PTHREAD_JOIN(3C): %s\n", i, strerror(err));
            DestroySems();
            pthread_exit(NULL);
        }
    }

    DestroySems();
    pthread_exit(NULL);
}