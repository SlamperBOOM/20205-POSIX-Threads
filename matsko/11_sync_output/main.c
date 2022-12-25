#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#define NUM_LINES 10

#define ERROR_ALLOC 1
#define ERROR_MUTEXATTR_INIT 2
#define ERROR_MUTEX_INIT 3

int MUTEX_NUM = 3;
pthread_mutex_t *mutex;
int *mutex_owner;

void acquireMutex(int index, int threadNum) {
    if (index >= MUTEX_NUM) {
        index = index % MUTEX_NUM;
    }
    int err = pthread_mutex_lock(&mutex[index]);
    if (err != 0) {
        fprintf(stderr, "Error mutex lock %s\n", strerror(err));
    }
    else {
        mutex_owner[index] = threadNum;
    }
}

void releaseMutex(int index) {
    if (index >= MUTEX_NUM) {
        index = index % MUTEX_NUM;
    }
    int prev_owner = mutex_owner[index];
    mutex_owner[index] = -1;
    int err = pthread_mutex_unlock(&mutex[index]);
    if (err != 0) {
        fprintf(stderr, "Error mutex unlock %s\n", strerror(err));
        mutex_owner[index] = prev_owner;
    }
}

void printFunc(int threadNum, int num_first_locked_mutex) {
    for (int i = num_first_locked_mutex; i < NUM_LINES + num_first_locked_mutex; i++) {
        acquireMutex(i % MUTEX_NUM, threadNum);
        if (threadNum == 0) {
            printf("Parent printing line %d\n", i - num_first_locked_mutex + 1);
        }
        else {
            printf("Child %d printing line %d\n", threadNum, i - num_first_locked_mutex + 1);
        }
        releaseMutex((i + MUTEX_NUM - 1) % MUTEX_NUM);
    }
    for (int i = 0; i < MUTEX_NUM; i++) {
        if (mutex_owner[i] == threadNum) {
            releaseMutex(i);
        }
    }
}

void childThread(void *arg) {
    int *int_arg = (int *)arg;
    int threadNum = *int_arg;
    acquireMutex(MUTEX_NUM - threadNum, threadNum);
    usleep(10 * MUTEX_NUM);
    printFunc(threadNum, (MUTEX_NUM - threadNum + 1) % MUTEX_NUM);
    pthread_exit((void *)EXIT_SUCCESS);
}


int main(int argc, char *argv[]) {
    if (argc == 2) {
        MUTEX_NUM = atoi(argv[1]) + 1;
        if (MUTEX_NUM <= 1) {
            MUTEX_NUM = 2;
        }
    }
    mutex = (pthread_mutex_t *) calloc(MUTEX_NUM, sizeof(pthread_mutex_t));
    if (mutex == NULL) {
        perror("Error calloc for mutexes");
        exit(ERROR_ALLOC);
    }
    mutex_owner = (int *) calloc(MUTEX_NUM, sizeof(int));
    if (mutex_owner == NULL) {
        perror("Error calloc for mutex owners");
        exit(ERROR_ALLOC);
    }
    pthread_mutexattr_t mutexattr;
    int err = pthread_mutexattr_init(&mutexattr);
    if (err != 0) {
        fprintf(stderr, "Error mutex attr init %s\n", strerror(err));
        exit(ERROR_MUTEXATTR_INIT);
    }
    pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK);

    int parentThreadNum = 0;
    for (int i = 0; i < MUTEX_NUM; i++) {
        mutex_owner[i] = -1;
        err = pthread_mutex_init(&mutex[i], &mutexattr);
        if (err != 0) {
            fprintf(stderr, "Error mutex init %s\n", strerror(err));
            exit(ERROR_MUTEX_INIT);
        }
    }

    pthread_mutexattr_destroy(&mutexattr);
    acquireMutex(0, parentThreadNum);
    pthread_t tids[MUTEX_NUM - 2];
    int thread_nums[MUTEX_NUM - 2];
    bool was_created[MUTEX_NUM - 2];

    for (int i = 0; i < MUTEX_NUM - 2; i++) {
        thread_nums[i] = i + 1;
        err = pthread_create(&tids[i], NULL, (void *)childThread, &thread_nums[i]);
        if (err != 0) {
            fprintf(stderr, "Error pthread create %s\n", strerror(err));
            was_created[i] = false;
        }
        else {
            was_created[i] = true;
        }
    }

    usleep(10 * MUTEX_NUM);
    printFunc(parentThreadNum, 1);


    for (int i = 0; i < MUTEX_NUM - 2; i++) {
        if (was_created[i]) {
            err = pthread_join(tids[i], NULL);
            if (err != 0) {
                fprintf(stderr, "Error pthread join %s\n", strerror(err));
            }
        }
    }

    for (int i = 0; i < MUTEX_NUM; i++) {
        err = pthread_mutex_destroy(&mutex[i]);
        if (err != 0) {
            fprintf(stderr, "Error mutex destroy %s\n", strerror(err));
        }
    }
    free(mutex);
    free(mutex_owner);
    pthread_exit((void *)EXIT_SUCCESS);
}
