#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>

#define NUM_LINES 10

#define ERROR_ALLOC 1

int SEM_NUM = 2;
sem_t *sems;
bool is_stop = false;

void printFunc(int threadNum) {
    //srand(time(NULL)); // for testing error handle
    for (int i = 0; i < NUM_LINES && !is_stop; i++) {
        int err = sem_wait(&sems[threadNum]);
        /*int chance = rand() % 10;  // for testing error handle
        if (chance == 0) {
            err = EINTR;
        }*/
        if (err == EINTR || is_stop) {
            if (is_stop) {
                break;
            }
            is_stop = true;
            fprintf(stderr, "Thread %d: error in sem_wait\nstopping program...\n", threadNum);
            for (int j = 0; j < NUM_LINES; j++) {
                sem_post(&sems[j]);
            }
            break;
        }
        else {
            if (threadNum == 0) {
                printf("Parent printing line %d\n", i + 1);
            }
            else {
                printf("Child %d printing line %d\n", threadNum, i + 1);
            }
            sem_post(&sems[(threadNum + 1) % SEM_NUM]);
        }
    }
}

void childThread(void *arg) {
    int *int_arg = (int *)arg;
    int threadNum = *int_arg;
    printFunc(threadNum);
    pthread_exit((void *)EXIT_SUCCESS);
}


int main(int argc, char *argv[]) {
    if (argc == 2) {
        SEM_NUM = atoi(argv[1]);
        if (SEM_NUM < 1) {
            SEM_NUM = 2;
        }
    }
    sems = (sem_t *) calloc(SEM_NUM, sizeof(sem_t));
    if (sems == NULL) {
        perror("Error calloc for sems");
        exit(ERROR_ALLOC);
    }

    int parentThreadNum = 0;
    for (int i = 0; i < SEM_NUM; i++) {
        if (i == 0) {
            sem_init(&sems[i], 0, 1);
        }
        else {
            sem_init(&sems[i], 0, 0);
        }
    }

    pthread_t tids[SEM_NUM - 1];
    int thread_nums[SEM_NUM - 1];
    bool was_created[SEM_NUM - 1];

    for (int i = 0; i < SEM_NUM - 1; i++) {
        thread_nums[i] = i + 1;
        int err = pthread_create(&tids[i], NULL, (void *)childThread, &thread_nums[i]);
        if (err != 0) {
            fprintf(stderr, "Error pthread create %s\n", strerror(err));
            was_created[i] = false;
        }
        else {
            was_created[i] = true;
        }
    }

    printFunc(parentThreadNum);

    for (int i = 0; i < SEM_NUM - 1; i++) {
        if (was_created[i]) {
            int err = pthread_join(tids[i], NULL);
            if (err != 0) {
                fprintf(stderr, "Error pthread join %s\n", strerror(err));
            }
        }
    }

    for (int i = 0; i < SEM_NUM; i++) {
        int err = sem_destroy(&sems[i]);
        if (err != 0) {
            perror("Error sem destroy");
        }
    }
    free(sems);
    pthread_exit((void *)EXIT_SUCCESS);
}
