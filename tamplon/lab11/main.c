#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#define NUMBER_OF_MUTEXES 3
#define NUMBER_OF_ITERATIONS 10
pthread_mutex_t mutexes[NUMBER_OF_MUTEXES];

typedef struct {
	char* thread_name;
} thread_args;

bool second_thread_is_ready = false;

void destroy_mutex(int mutex_id) {
	int error = pthread_mutex_destroy(&mutexes[mutex_id]);
	if (error != 0) {
		fprintf(stderr, "Error while destroying mutex %d: %s\n", mutex_id, strerror(error));
		exit(EXIT_FAILURE);
	}
}

void destroy_mutexes() {
	for (int i = 0; i < NUMBER_OF_MUTEXES; ++i) {
		destroy_mutex(i);
	}
}

void lock_mutex(int mutex_id) {
	int error = pthread_mutex_lock(&mutexes[mutex_id]);
	if (error != 0) {
		fprintf(stderr, "Error while locking mutex %d: %s\n", mutex_id, strerror(error));
		destroy_mutexes();
		exit(EXIT_FAILURE);
	}
}

void unlock_mutex(int mutex_id) {
	int error = pthread_mutex_unlock(&mutexes[mutex_id]);
	if (error != 0) {
		fprintf(stderr, "Error while unlocking mutex %d: %s\n", mutex_id, strerror(error));
		destroy_mutexes();
		exit(EXIT_FAILURE);
	}
}

void init_mutexes() {
	pthread_mutexattr_t attr;
	if (pthread_mutexattr_init(&attr) != 0) {
		perror("pthread_mutexattr_init");
		exit(EXIT_FAILURE);
	}

	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK) != 0) {
		perror("pthread_mutexattr_settype");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < NUMBER_OF_MUTEXES; ++i) {
		if (pthread_mutex_init(&mutexes[i], &attr) != 0) {
			perror("pthread_mutex_init");
			for (int j = 0; j < i; ++j) {
				destroy_mutex(j);
			}
			exit(EXIT_FAILURE);
		}
	}
	/*
	 * After a mutex attributes object has been used to initialize one or more mutexes,
	 * any function affecting the attributes object (including destruction)
	 * shall not affect any previously initialized mutexes
	 */
	if (pthread_mutexattr_destroy(&attr) != 0) {
		perror("pthread_mutexattr_destroy");
		exit(EXIT_FAILURE);
	}
}

void wait_for_other_thread() {
	while (!second_thread_is_ready) {
		usleep(1000);
	}
}

void* printing_function(void* args) {
	char* thread_name = ((thread_args*)args)->thread_name;
	int current_mutex = 0, next_mutex = 1;

	if (!second_thread_is_ready) {
		second_thread_is_ready = true;
		current_mutex = 2;
		next_mutex = 0;
		lock_mutex(current_mutex);
	}

	for (int i = 0; i < NUMBER_OF_ITERATIONS; ++i) {
		lock_mutex(next_mutex);
		printf("%d %s\n", i, thread_name);
		unlock_mutex(current_mutex);

		current_mutex = next_mutex;
		next_mutex = (current_mutex + 1) % NUMBER_OF_MUTEXES;
	}

	unlock_mutex(current_mutex);
	pthread_exit(NULL);
}

int main() {
	init_mutexes();
	lock_mutex(0);
	pthread_t thread;
	thread_args child_args;
	child_args.thread_name = "Child";

	if (pthread_create(&thread, NULL, printing_function, &child_args) != 0) {
		perror("pthread_create");
		exit(EXIT_FAILURE);
	}
	wait_for_other_thread();
	thread_args parent_args;
	parent_args.thread_name = "Parent";
	printing_function(&parent_args);
	if(pthread_join(thread, NULL) != 0) {
		perror("pthread_join");
		exit(EXIT_FAILURE);
	}
	destroy_mutexes();
}