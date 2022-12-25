#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <float.h>

#define STEPS_NUM 100000

long int threads_count;
struct timespec start_writing_pipe;
pthread_barrier_t b;

typedef struct thread_parameters {
    int thread_number;
    double result;
    double time_waiting;
} param_t;

double calculate_pi_part(param_t* parameters) {

    struct timespec end_waiting;

    pthread_barrier_wait(&b);

    clock_gettime(CLOCK_MONOTONIC_RAW, &end_waiting);
    parameters->time_waiting = (double) (end_waiting.tv_sec) +
                               0.000000001 * (double) (end_waiting.tv_nsec);

//    double* pi_part = (double*) malloc(sizeof(double));
    for (long i = parameters->thread_number; i * 4 + 3 <= LONG_MAX &&
    i < STEPS_NUM * threads_count + parameters->thread_number; i += threads_count) {
        parameters->result += 1.0 / (double) (i * 4 + 1);
        parameters->result -= 1.0 / (double) (i * 4 + 3);
    }

//    printf("Thread %d Pi_part / 4 = %.15lf \n", parameters->thread_number, parameters->result);
}

int main(int argc, char* argv[]) {
    if (argv[1] == NULL) {
        threads_count = 1;
    } else {
        threads_count = strtol(argv[1], NULL, 0);
        if (threads_count == LONG_MIN) {
            perror("Error in strtol");
            pthread_exit((void*) EXIT_FAILURE);
        } else if (threads_count <= 0) {
            fprintf(stderr, "Wrong argument. Threads number should be above zero.\n");
            pthread_exit((void*) EXIT_FAILURE);
        }
    }

    pthread_t* threads = (pthread_t*) malloc(threads_count * sizeof(pthread_t));
    if (threads == NULL) {
        perror("threads malloc");
        pthread_exit((void*) EXIT_FAILURE);
    }
//    int* threads_numbers = (int*) malloc(threads_count * sizeof(int));
//    if (threads_numbers == NULL) {
//        perror("threads_numbers malloc");
//        pthread_exit((void*) EXIT_FAILURE);
//    }


    pthread_barrier_init(&b, NULL, threads_count);
    int return_value;
    struct timespec start, end;
    param_t threads_parameters[threads_count];

    for (int i = 0; i < threads_count; i++) {
//        threads_numbers[i] = i;
        threads_parameters[i].thread_number = i;
        threads_parameters[i].result = 0;
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);

    for (int i = 0; i < threads_count; i++) {
        return_value = pthread_create(threads + i, NULL, (void*) calculate_pi_part, (void*) &threads_parameters[i]);
        if (0 != return_value) {
            fprintf(stderr, "Cannot create a thread: %s", strerror(return_value));
            pthread_exit((void*) EXIT_FAILURE);
        }
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    fprintf(stderr, "THREADS CREATED\n");

    double pi_div_by_4 = 0;
    double* thread_return_value = NULL;
    for (int i = 0; i < threads_count; i++) {
        return_value = pthread_join(threads[i], (void**) &thread_return_value);
        if (0 != return_value) {
            fprintf(stderr, "Cannot join a thread: %s", strerror(return_value));
//            free(thread_return_value);
            pthread_exit((void*) EXIT_FAILURE);
        }
//        printf("check1\n");
        pi_div_by_4 += threads_parameters[i].result;
//        printf("check2\n");
//        free(thread_return_value);

    }

    printf("Pi = %.15lf\n", pi_div_by_4 * 4);

    printf("Time taken to create one thread: %lf sec.\n",
           ((double) (end.tv_sec - start.tv_sec) + 0.000000001 * (double) (end.tv_nsec - start.tv_nsec)) / (double) (threads_count));
    double max_time = -1;
    double min_time = DBL_MAX;
    for (int i = 0; i < threads_count; i++) {
        if (max_time < threads_parameters[i].time_waiting) {
            max_time = threads_parameters[i].time_waiting;
        }
        if (min_time > threads_parameters[i].time_waiting) {
            min_time = threads_parameters[i].time_waiting;
        }
    }
    printf("Max time waiting: \t%lf sec\n", max_time - min_time);

    pthread_barrier_destroy(&b);
    free(threads);
    pthread_exit((void*) EXIT_SUCCESS);
}
