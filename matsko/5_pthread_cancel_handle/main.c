#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>

#define NUM_STEPS 200000000

typedef struct thread_parameters {
    int num_iterations;
    int start_index;
    double result;
} param_t;


void *CalcPartOfPi(void *arg) {
    param_t *parameters = (param_t *)arg;
    long long int stop = parameters->start_index + parameters->num_iterations;
    double pi = 0;
    for (int i = parameters->start_index; i < stop; i++) {
        pi += 1.0/(i*4.0 + 1.0);
        pi -= 1.0/(i*4.0 + 3.0);
    }
    parameters->result = pi;
    if (parameters->start_index != 0) {
        pthread_exit(NULL);
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Wrong amount of arguments:\n expected - 1, has - %d\n", argc - 1);
        exit(1);
    }

    const int NUM_THREADS = atoi(argv[1]);
    if (NUM_THREADS <= 0) {
        fprintf(stderr, "Error: Number of threads should be 1 or greater\n");
        exit(2);
    }

    param_t threads_parameters[NUM_THREADS];
    bool was_created[NUM_THREADS];

    int part = NUM_STEPS / NUM_THREADS;

    threads_parameters[0].start_index = 0;
    threads_parameters[0].num_iterations = part + (0 < NUM_STEPS % NUM_THREADS);
    was_created[0] = true;

    for (int i = 1; i < NUM_THREADS; i++) {
        threads_parameters[i].start_index = threads_parameters[i - 1].start_index + threads_parameters[i - 1].num_iterations;
        threads_parameters[i].num_iterations = part + (i < NUM_STEPS % NUM_THREADS);
        threads_parameters[i].result = 0;
    }

    pthread_t thread_id[NUM_THREADS - 1];
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        int create_res = pthread_create(&thread_id[i], NULL, CalcPartOfPi, (void *)&threads_parameters[i + 1]);
        was_created[i] = true;
        if (create_res != 0) {
            fprintf(stderr, "Error in pthread_create on iteration %d: %s", i, strerror(create_res));
            was_created[i] = false;
        }
    }

    CalcPartOfPi((void *)&threads_parameters[0]);

    for (int i = 0; i < NUM_THREADS - 1; i++) {
        if (was_created[i]) {
            int join_res = pthread_join(thread_id[i], NULL);
            if (join_res != 0) {
                fprintf(stderr, "Error in pthread_join on iteration %d: %s", i, strerror(join_res));
                pthread_exit((void *)5);
            }
        }
    }

    double pi = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        pi += threads_parameters[i].result;
    }
    pi = pi * 4.0;
    printf("pi done = %.15g \n", pi);
    return 0;
}

