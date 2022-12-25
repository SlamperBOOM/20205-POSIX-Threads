#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

void *PrintLines(void  *arg) {
    int i = 1;
    while (1) {
        pthread_testcancel();
        printf("Child printing line %d\n", i);
        i += 1;
    }
}

int main() {
    pthread_t thread_id;

    int create_res = pthread_create(&thread_id, NULL, PrintLines, (void *)NULL);
    if (create_res != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(create_res));
        pthread_exit((void *)1);
    }

    sleep(2);
    printf("parent: try to cancel child...\n");
    int cancel_res = pthread_cancel(thread_id);
    if (cancel_res != 0) {
        fprintf(stderr, "pthread_cancel: %s\n", strerror(cancel_res));
        pthread_exit((void *)2);
    }
    else {
        printf("parent: pthread_cancel() success\n");
    }

    int join_res = pthread_join(thread_id, NULL);
    if (join_res != 0) {
        fprintf(stderr, "pthread_join: %s\n", strerror(join_res));
        pthread_exit((void *)3);
    }

    return 0;
}

