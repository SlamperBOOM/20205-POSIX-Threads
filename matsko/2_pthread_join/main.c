#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

const int NUM_LINES_TO_PRINT = 10;

void *PrintLines(void  *arg) {
    for (int i = 0; i < NUM_LINES_TO_PRINT; i++) {
        printf("Child printing line %d\n", i + 1);
    }
    pthread_exit(NULL);
}

int main() {
    pthread_t thread_id;

    int create_res = pthread_create(&thread_id, NULL, PrintLines, NULL);
    if (create_res != 0) {
        fprintf(stderr, "%s\n", strerror(create_res));
        pthread_exit((void *)1);
    }

    int join_res = pthread_join(thread_id, NULL);
    if (join_res != 0) {
        fprintf(stderr, "%s\n", strerror(create_res));
        pthread_exit((void *)2);
    }

    for (int i = 0; i < NUM_LINES_TO_PRINT; i++) {
        printf("Parent printing line %d\n", i + 1);
    }

    pthread_exit(0);
}
