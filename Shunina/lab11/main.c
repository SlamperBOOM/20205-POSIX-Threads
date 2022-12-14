#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

pthread_mutexattr_t attr;
pthread_mutex_t m[3];
int mut[3];

void destroyMutexes(int index) {
    for (int i = 0; i < index; ++i) {
        pthread_mutex_destroy(&m[i]);
    }
}

void errorMessage(char* errorMsg) {
    perror(errorMsg);
}

int lockMutex(int index) {
    if (pthread_mutex_lock(&m[index])) {
        errorMessage("Error lock mutex");
        return -1;
    }
    return 0;
}

int unlockMutex(int index) {
    if (pthread_mutex_unlock(&m[index])) {
        errorMessage("Error unlock mutex");
        return -1;
    }
    return 0;
}

void unlockAllThreadMutexes(int num) {
    for (int i = 0; i < 3; ++i) {
    	if (mut[i] == num) {
            unlockMutex(i);
        }
    }
}

void initMutexes() {
    pthread_mutexattr_init(&attr);
    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK)) {
        perror("Error creating attributes\n");
        pthread_exit(NULL);
    }

    for (int i = 0; i < 3; ++i) {
        if (pthread_mutex_init(&m[i], &attr)) {
            destroyMutexes(i);
            perror("Error creating mutex");
            pthread_exit(NULL);
        }
    }
}

void* function() {
    int n = 0;
    if (lockMutex(1)) {
        return NULL;
    }
    mut[1] = 2;
    for (int i = 0; i < 10; ++i) {
    	pthread_testcancel();
        if (lockMutex(n)) {
            break;
        }
    	mut[n] = 2;
        printf("second - %d\n", i);
        if (unlockMutex((n + 1) % 3)) {
            break;
        }
    	mut[(n + 1) % 3] = 0;
        n = (n + 2) % 3;
    }
    unlockAllThreadMutexes(2);
    return NULL;
}

void first() {
    int n = 0;
    for (int i = 0; i < 10; ++i) {
        printf("first - %d\n", i);
	if (unlockMutex(n)) {
            break;
        }
    	mut[n] = 0;
        if (lockMutex((n + 1) % 3)) {
            break;
        }
    	mut[(n + 1) % 3] = 1;
        n = (n + 2) % 3;
    }
    unlockAllThreadMutexes(1);
}

int main(int arc, char** argv) {
    pthread_t thread;

    initMutexes();
    if(lockMutex(0)) {
    	destroyMutexes(3);
    	return EXIT_FAILURE;
    }
    mut[0] = 1;
    if(lockMutex(2)) {
    	unlockAllThreadMutexes(1);
    	destroyMutexes(3);
    	return EXIT_FAILURE;
    }
    mut[2] = 1;
    
    if (pthread_create(&thread, NULL, function, NULL)) {
        errorMessage("Error creating thread");
        unlockAllThreadMutexes(1);
        destroyMutexes(3);
    	return EXIT_FAILURE;
    }

    if (sleep(1)) {
        pthread_cancel(thread);
        errorMessage("Sleep was interrupted");
        unlockAllThreadMutexes(1);
        destroyMutexes(3);
    	return EXIT_FAILURE;
    }

    first();

    if (pthread_join(thread, NULL)) {
        errorMessage("Error waiting thread");
    }
    destroyMutexes(3);
    return EXIT_SUCCESS;
}
