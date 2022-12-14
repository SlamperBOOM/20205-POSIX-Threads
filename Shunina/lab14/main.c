#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h>
#include <string.h>

sem_t sem[2];

void semDestroy(int index) {
    for (int i = 0; i < index; ++i) {
        sem_destroy(&sem[i]);
    }
}

void errorMessage(char* errorMsg) {
    perror(errorMsg);
}

int semWait(int index) {
    if (sem_wait(&sem[index])) {
        errorMessage("Error sem wait");
        return -1;
    }
    return 0;
}

int semPost(int index) {
    if (sem_post(&sem[index])) {
        errorMessage("Error sem post");
        return -1;
    }
    return 0;
}

void* function() {
    for (int i = 0; i < 10; ++i) {
        pthread_testcancel();
        if (semWait(1)) {
            break;
        }
        printf("second - %d\n", i);
        if (semPost(0)) {
            break;
        }
    }
    return NULL;
}

void mainThread(pthread_t thread) {
     for (int i = 0; i < 10; ++i) {
     	if (semWait(0)) {
            pthread_cancel(thread);
            break;
        }
        printf("first - %d\n", i);
        
        if (semPost(1)) {
            pthread_cancel(thread);
            break;
        }
    }
}

int main() {
    pthread_t thread;
    
    if (sem_init(&sem[0], 1, 1)) {
        errorMessage("Error sem init");
        return EXIT_FAILURE;
    }
    if (sem_init(&sem[1], 1, 0)) {
        semDestroy(1);
        errorMessage("Error sem init");
        return EXIT_FAILURE;
    }

    if (pthread_create(&thread, NULL, function, NULL)) {
        errorMessage("pthread_create failed");
        semDestroy(2);
        return EXIT_FAILURE;
    }

    mainThread(thread);

    if (pthread_join(thread, NULL)){
        errorMessage("Error waiting thread");
    }
    semDestroy(2);

    return EXIT_SUCCESS;
}
