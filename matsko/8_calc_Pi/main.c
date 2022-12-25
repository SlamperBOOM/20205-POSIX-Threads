#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

#define NUM_STEPS 2000000000

int start_fd;

typedef struct thread_parameters {
    int num_iterations;
    int start_index;
    double result;
    double time_waiting;
} param_t;

struct timespec start_writing_pipe;

void *CalcPartOfPi(void *arg) {
    param_t *parameters = (param_t *)arg;
    struct timespec end_waiting;
    //fprintf(stderr, "Thread with start_index = %d\n", parameters->start_index);
    long long int stop = parameters->start_index + parameters->num_iterations;
    double pi = 0;
    char buf;
    ssize_t was_read = read(start_fd, &buf, 1);
    if (was_read < 0) {
        perror("Error read for synchronization");
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_waiting);
    parameters->time_waiting = end_waiting.tv_sec-start_writing_pipe.tv_sec +
            0.000000001*(end_waiting.tv_nsec-start_writing_pipe.tv_nsec);
    for (int i = parameters->start_index; i < stop; i++) {
        pi += 1.0/(i*4.0 + 1.0);
        pi -= 1.0/(i*4.0 + 3.0);
    }
    //fprintf(stderr, "Thread with start_index = %d FINISHED CALCULATION\n", parameters->start_index);
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
    printf("\nNUM_THREADS = %d\n", NUM_THREADS);

    param_t threads_parameters[NUM_THREADS];
    bool was_created[NUM_THREADS - 1];

    int part = NUM_STEPS / NUM_THREADS;

    threads_parameters[0].start_index = 0;
    threads_parameters[0].num_iterations = part + (0 < NUM_STEPS % NUM_THREADS);

    for (int i = 1; i < NUM_THREADS; i++) {
        threads_parameters[i].start_index = threads_parameters[i - 1].start_index + threads_parameters[i - 1].num_iterations;
        threads_parameters[i].num_iterations = part + (i < NUM_STEPS % NUM_THREADS);
        threads_parameters[i].result = 0;
        threads_parameters[i].time_waiting = 0;
    }

    int pipe_fds[2];
    int pipe_res = pipe(pipe_fds);
    if (pipe_res != 0) {
        perror("Error pipe():");
    }
    start_fd = pipe_fds[0];
    int write_fd = pipe_fds[1];

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    pthread_t thread_id[NUM_THREADS - 1];
    int really_created = 1;
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        int create_res = pthread_create(&thread_id[i], NULL, CalcPartOfPi, (void *)&threads_parameters[i + 1]);
        was_created[i] = true;
        if (create_res != 0) {
            fprintf(stderr, "Error in pthread_create on iteration %d: %s\n", i, strerror(create_res));
            was_created[i] = false;
        }
        else {
            really_created += 1;
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    fprintf(stderr, "THREADS CREATED\n");
    char start_buf[BUFSIZ];
    ssize_t bytes_written = 0;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_writing_pipe);
    while (bytes_written < really_created) {
        ssize_t written = 0;
        if (really_created - bytes_written <= BUFSIZ) {
            written = write(write_fd, start_buf, really_created - bytes_written);
        }
        else {
            written = write(write_fd, start_buf, BUFSIZ);
        }
        if (written < 0) {
            perror("Error write");
            fprintf(stderr, "bytes_written: %ld / %d\n", bytes_written, really_created);
        }
        else {
            bytes_written += written;
        }
    }
    CalcPartOfPi((void *)&threads_parameters[0]);
    //fprintf(stderr, "\n====================MAIN THREAD FINISHED CALCULATION====================\n");

    for (int i = 0; i < NUM_THREADS - 1; i++) {
        if (was_created[i]) {
            //fprintf(stderr, "Try join thread start_index = %d\n", threads_parameters[i + 1].start_index);
            int join_res = pthread_join(thread_id[i], NULL);
            if (join_res != 0) {
                fprintf(stderr, "Error in pthread_join on iteration %d: %s\n", i, strerror(join_res));
            }
        }
    }
    close(start_fd);
    close(write_fd);
    printf("Time taken to create one thread: %lf sec.\n",
           (end.tv_sec-start.tv_sec + 0.000000001*(end.tv_nsec-start.tv_nsec)) / (really_created - 1));
    printf("canonic pi = \t%.15g\n", M_PI);
    double max_time_wait = -1;
    for (int i = 0; i < NUM_THREADS; i++) {
        if (max_time_wait < threads_parameters[i].time_waiting) {
            max_time_wait = threads_parameters[i].time_waiting;
        }
    }
    printf("Max time waiting: \t%lf sec\n", max_time_wait);
    double pi = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        pi += threads_parameters[i].result;
    }
    pi = pi * 4.0;
    printf("pi done = \t%.15g \n\n", pi);
    return 0;
}

