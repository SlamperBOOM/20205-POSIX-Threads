#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#define LINES_NUMBER 10
#define MAIN_THREAD 0
#define CHILD_THREAD 1

pthread_mutex_t mutex;
pthread_cond_t cond;
bool mains_turn = true;
bool error_occurred = false;

void print_line(int thread_num, int iteration) {
    if (thread_num == MAIN_THREAD) {
        printf("%d. Main thread\n", iteration);
    } else {
        printf("%d. Child thread\n", iteration);
    }
}

void print_error(int err, int thread_num, char* function_name) {
    if (thread_num == MAIN_THREAD) {
        fprintf(stderr, "Error in main thread: %s: %s\n", function_name, strerror(err));
    } else {
        fprintf(stderr, "Error in child thread: %s: %s\n", function_name, strerror(err));
    }
}

void* print_lines(int thread_num) {
    struct timespec timeout;
    for (int i = 0; i < LINES_NUMBER; i++) {
        if (error_occurred) {
            break;
        }

        int err = pthread_mutex_lock(&mutex);
        if (err != 0) {
            print_error(err, thread_num, "mutex_lock");
            error_occurred = true;
            return (void*) EXIT_FAILURE;
        }

        // pthread_cond_timedwait(3) используется для того, чтобы проверить error_occured,
        // так как первый поток мог получить ошибку и изменить значение error_occured
        // уже после того, как второй поток проверил условие while.

        while (((mains_turn && thread_num == CHILD_THREAD) ||
                (!mains_turn && thread_num != CHILD_THREAD)) &&
               (err == 0 || err == ETIMEDOUT) &&
               !error_occurred) {
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 5;
            err = pthread_cond_timedwait(&cond, &mutex, &timeout);
        }

        if (err != 0 && err != ETIMEDOUT) {
            print_error(err, thread_num, "cond_timedwait");
            error_occurred = true;
            return (void*) EXIT_FAILURE;
        }

        print_line(thread_num, i);
        mains_turn = !mains_turn;
        pthread_cond_signal(&cond);

        err = pthread_mutex_unlock(&mutex);
        if (err != 0) {
            print_error(err, thread_num, "mutex_unlock");
            error_occurred = true;
            return (void*) EXIT_FAILURE;
        }
    }
    return (void*) EXIT_SUCCESS;
}

int main() {
    pthread_mutexattr_t mutexattr;
    int err = pthread_mutexattr_init(&mutexattr);
    if (err != 0) {
        print_error(err, MAIN_THREAD, "mutexattr_init");
        error_occurred = true;
        goto EXIT;
    }
    pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK);

    err = pthread_mutex_init(&mutex, &mutexattr);
    pthread_mutexattr_destroy(&mutexattr);
    if (err != 0) {
        print_error(err, MAIN_THREAD, "mutex_init");
        error_occurred = true;
        goto EXIT;
    }

    err = pthread_cond_init(&cond, NULL);
    if (err != 0) {
        print_error(err, MAIN_THREAD, "cond_init");
        error_occurred = true;
        goto MUTEX_DESTROY;
    }

    pthread_t child_thread;
    err = pthread_create(&child_thread, NULL, (void* (*)(void*)) print_lines, (void*) CHILD_THREAD);
    if (err != 0) {
        print_error(err, MAIN_THREAD, "create");
        error_occurred = true;
        goto CLEANUP;
    }

    print_lines(MAIN_THREAD);

    int thread_return;
    err = pthread_join(child_thread, (void**) &thread_return);
    if (err != 0) {
        print_error(err, MAIN_THREAD, "join");
        error_occurred = true;
    }

    CLEANUP:
    err = pthread_cond_destroy(&cond);
    if (err != 0) {
        print_error(err, MAIN_THREAD, "cond_destroy");
        error_occurred = true;
    }

    MUTEX_DESTROY:
    err = pthread_mutex_destroy(&mutex);
    if (err != 0) {
        print_error(err, MAIN_THREAD, "mutex_destroy");
        error_occurred = true;
    }

    EXIT:
    if (error_occurred) {
        pthread_exit((void*) EXIT_FAILURE);
    }
    pthread_exit((void*) EXIT_SUCCESS);
}
