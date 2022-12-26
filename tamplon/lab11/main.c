#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#define NUMBER_OF_MUTEXES 3
#define NUMBER_OF_ITERATIONS 10
#define SUCCESS 0
pthread_mutex_t mutexes[NUMBER_OF_MUTEXES];

typedef struct {
	char* thread_name;
} thread_args;

typedef int error_t;

bool second_thread_is_ready = false;

error_t destroy_mutex(int mutex_id) {
	error_t error = pthread_mutex_destroy(&mutexes[mutex_id]);
	if (error != 0) {
		fprintf(stderr, "Error while destroying mutex %d: %s\n", mutex_id, strerror(error));
		return error;
	}
	return SUCCESS;
}

error_t destroy_mutexes() {
	for (int i = 0; i < NUMBER_OF_MUTEXES; ++i) {
		error_t error = destroy_mutex(i);
		if (error != SUCCESS) {
			return error;
		}
	}
	return SUCCESS;
}

error_t lock_mutex(int mutex_id) {
	error_t error = pthread_mutex_lock(&mutexes[mutex_id]);
	if (error != 0) {
		fprintf(stderr, "Error while locking mutex %d: %s\n", mutex_id, strerror(error));
		return error;
	}
	return SUCCESS;
}

error_t unlock_mutex(int mutex_id) {
	error_t error = pthread_mutex_unlock(&mutexes[mutex_id]);
	if (error != 0) {
		fprintf(stderr, "Error while unlocking mutex %d: %s\n", mutex_id, strerror(error));
		return error;
	}
	return SUCCESS;
}

error_t init_mutexes() {
	pthread_mutexattr_t attr;
	error_t error;
	error = pthread_mutexattr_init(&attr);
	if (error != SUCCESS) {
		perror("pthread_mutexattr_init");
		return error;
	}

	error = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	if (error != SUCCESS) {
		pthread_mutexattr_destroy(&attr);
		perror("pthread_mutexattr_settype");
		return error;
	}

	for (int i = 0; i < NUMBER_OF_MUTEXES; ++i) {
		error = pthread_mutex_init(&mutexes[i], &attr);
		if (error != 0) {
			perror("pthread_mutex_init");
			for (int j = 0; j < i; ++j) {
				if (destroy_mutex(j) != SUCCESS) {
					break;
				}
			}
			pthread_mutexattr_destroy(&attr);
			return error;
		}
	}
	/*
	 * After a mutex attributes object has been used to initialize one or more mutexes,
	 * any function affecting the attributes object (including destruction)
	 * shall not affect any previously initialized mutexes
	 */
	error = pthread_mutexattr_destroy(&attr);
	if (error != 0) {
		perror("pthread_mutexattr_destroy");
		return error;
	}
	return SUCCESS;
}

void wait_for_other_thread() {
	while (!second_thread_is_ready) {
		usleep(1000);
	}
}

void* printing_function(void* args) {
	char* thread_name = ((thread_args*)args)->thread_name;
	int current_mutex = 0;
	int next_mutex = 1;
	error_t error;

	if (!second_thread_is_ready) {
		second_thread_is_ready = true;
		current_mutex = 2;
		next_mutex = 0;
		error = lock_mutex(current_mutex);
		if (error != SUCCESS) {
			pthread_exit(NULL);
		}
	}

	for (int i = 0; i < NUMBER_OF_ITERATIONS; ++i) {
		error = lock_mutex(next_mutex);
		if (error != SUCCESS) {
			pthread_exit(NULL);
		}
		printf("%d %s\n", i, thread_name);
		error = unlock_mutex(current_mutex);
		if (error != SUCCESS) {
			pthread_exit(NULL);
		}

		current_mutex = next_mutex;
		next_mutex = (current_mutex + 1) % NUMBER_OF_MUTEXES;
	}

	unlock_mutex(current_mutex);
	pthread_exit(NULL);
}

int main() {
	if (init_mutexes() != SUCCESS) {
		return EXIT_FAILURE;
	}
	if (lock_mutex(0) != SUCCESS) {
		destroy_mutexes();
		return EXIT_FAILURE;
	}
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