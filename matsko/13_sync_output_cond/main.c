#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#define ERROR_MUTEXATTR_INIT 2
#define ERROR_MUTEX_INIT 3
#define ERROR_COND_INIT 4
#define ERROR_THREAD_CREATE 5

int NUM_LINES = 10;

bool valid_mutexattr = false;
bool valid_cond = false;
bool valid_mutex = false;

pthread_mutexattr_t mutexattr;
pthread_mutex_t mutex;
pthread_cond_t cond;
bool is_main_printing = true;
bool is_stop = false;

void cleanUp() {
    if (valid_mutexattr) {
        pthread_mutexattr_destroy(&mutexattr);
        valid_mutexattr = false;
    }
    if (valid_mutex) {
        pthread_mutex_destroy(&mutex);
        valid_mutex = false;
    }
    if (valid_cond) {
        pthread_cond_destroy(&cond);
        valid_cond = false;
    }
}

void initMutex(pthread_mutex_t *m, pthread_mutexattr_t *attr) {
    int err = pthread_mutex_init(m, attr);
    if (err != 0) {
        fprintf(stderr, "Error mutex init %s\n", strerror(err));
        cleanUp();
        exit(ERROR_MUTEX_INIT);
    }
    valid_mutex = true;
}

void initMutexAttr(pthread_mutexattr_t *attr) {
    int err = pthread_mutexattr_init(attr);
    if (err != 0) {
        fprintf(stderr, "Error mutex attr init %s\n", strerror(err));
        cleanUp();
        exit(ERROR_MUTEXATTR_INIT);
    }
    valid_mutexattr = true;
}

void initCond(pthread_cond_t *c) {
    int err = pthread_cond_init(c, NULL);
    if (err != 0) {
        fprintf(stderr, "Error cond init %s\n", strerror(err));
        exit(ERROR_COND_INIT);
    }
    valid_cond = true;
}

void* childFunc(void* arg) {
    for (int i = 0; i < NUM_LINES && !is_stop; i++) {
        int err = pthread_mutex_lock(&mutex);
        if (err != 0) {
            fprintf(stderr, "Error mutex lock %s\n", strerror(err));
            is_stop = true;
            break;
        }

        err = 0;
        while (is_main_printing && err == 0) {
            err = pthread_cond_wait(&cond, &mutex);
            if (err != 0) {
                fprintf(stderr, "Error cond wait %s\n", strerror(err));
            }
        }
        printf("Child printing line %d\n", i + 1);
        is_main_printing = true;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);

        if (err != 0) {
            is_stop = true;
            break;
        }
    }
    pthread_exit((void *)0);
}

int main() {
    initMutexAttr(&mutexattr);
    pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK);

    initMutex(&mutex, &mutexattr);
    pthread_mutexattr_destroy(&mutexattr);
    valid_mutexattr = false;

    initCond(&cond);

    pthread_t tid;
    int err = pthread_create(&tid, NULL, childFunc, NULL);
    if (err != 0) {
        fprintf(stderr, "Error pthread create %s\n", strerror(err));
        cleanUp();
        exit(ERROR_THREAD_CREATE);
    }
    for (int i = 0; i < NUM_LINES && !is_stop; i++) {
        err = pthread_mutex_lock(&mutex);
        if (err != 0) {
            fprintf(stderr, "Error mutex lock %s\n", strerror(err));
            is_stop = true;
            break;
        }

        err = 0;
        while (!is_main_printing && err == 0) {
            err = pthread_cond_wait(&cond, &mutex);
            if (err != 0) {
                fprintf(stderr, "Error cond wait %s\n", strerror(err));
            }
        }
        printf("Parent printing line %d\n", i + 1);
        is_main_printing = false;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);

        if (err != 0) {
            is_stop = true;
            break;
        }
    }

    err = pthread_join(tid, NULL);
    if (err != 0) {
        fprintf(stderr, "Error pthread join %s\n", strerror(err));
    }

    cleanUp();
    return 0;
}
