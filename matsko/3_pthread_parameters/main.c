#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

const int NUM_THREADS = 4;

void *PrintLines(void  *arg) {
    char **lines = (char **)arg;
    int i = 0;
    printf("TIDS: %d\n", gettid());
    while (lines[i] != (char *)NULL) {
        ssize_t written = write(1, lines[i], strlen(lines[i]));
        if (written == -1) {
            perror("write");
            pthread_exit(NULL);
        }
        i += 1;
    }
    pthread_exit(NULL);
}



int main() {
    char *first[] = {"first\n", "11111\n", "111\n", "first\n", "11111\n", "111\n",
                     "first\n", "11111\n", "111\n", "first\n", "11111\n", "111\n",
                     "first\n", "11111\n", "111\n", "first\n", "11111\n", "111\n",
                     NULL};
    char *second[] = {"second\n", "second\n", "second\n", "second\n", "second\n",
                      "second\n", "second\n", "second\n", "second\n", "second\n",
                      "second\n", NULL};
    char *third[] = {"third\n", "33333333\n", "third\n", "33333333\n", "third\n",
                     "33333333\n", "third\n", "33333333\n", "third\n", "33333333\n",
                     "third\n", "33333333\n", "third\n", "33333333\n", "third\n",
                     "33333333\n", "third\n", "33333333\n", "third\n", "33333333\n",
                     "third\n", "33333333\n", "third\n", "33333333\n", "third\n",
                     "33333333\n", "third\n", "33333333\n", "third\n", "33333333\n",
                     "third\n", "33333333\n", "third\n", "33333333\n", NULL};
    char *forth[] = {"forth\n", "4444444444\n", "44\n", "4444\n", "forth\n",
                     "4444444444\n", "44\n", "4444\n", "forth\n", "4444444444\n",
                     "44\n", "4444\n", "forth\n", "4444444444\n", "44\n", "4444\n",
                     "forth\n", "4444444444\n", "44\n", "4444\n", "forth\n",
                     "4444444444\n", "44\n", "4444\n", "forth\n", "4444444444\n",
                     "44\n", "4444\n", "forth\n", "4444444444\n", "44\n", "4444\n",
                     "forth\n", "4444444444\n", "44\n", "4444\n", "forth\n",
                     "4444444444\n", "44\n", "4444\n", "forth\n", "4444444444\n",
                     "44\n", "4444\n", "forth\n", "4444444444\n", "44\n", "4444\n",
                     "forth\n", "4444444444\n", "44\n", "4444\n", "forth\n",
                     "4444444444\n", "44\n", "4444\n", "forth\n", "4444444444\n",
                     "44\n", "4444\n", "forth\n", "4444444444\n", "44\n", "4444\n",
                     "forth\n", "4444444444\n", "44\n", "4444\n", NULL};
    char **LINES[] = {first, second, third, forth};

    pthread_t thread_id[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        int res = pthread_create(&thread_id[i], NULL, PrintLines, (void *)LINES[i]);
        if (res != 0) {
            fprintf(stderr, "pthread_create %d: %s\n", i, strerror(res));
            pthread_exit((void *)1);
        }
    }

    pthread_exit(0);
}

