#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

pthread_mutex_t mutex1, mutex2, startMutex;
void* threadFunction(void* args) {
	(void)args;
	if (pthread_mutex_lock(&startMutex) != 0) {
		perror("pthread_mutex_lock: startMutex");
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < 10; ++i) {
		if (pthread_mutex_lock(&mutex2) != 0) {
			perror("pthread_mutex_lock: mutex2");
			exit(EXIT_FAILURE);
		}
		printf("child: %d\n", i);
		if (pthread_mutex_unlock(&mutex1) != 0) {
			perror("pthread_mutex_unlock: mutex1");
			exit(EXIT_FAILURE);
		}
	}
	if (pthread_mutex_unlock(&mutex2) != 0) {
		perror("pthread_mutex_unlock: mutex2");
		exit(EXIT_FAILURE);
	}
	if (pthread_mutex_unlock(&startMutex) != 0) {
		perror("pthread_mutex_unlock: startMutex");
		exit(EXIT_FAILURE);
	}
	pthread_exit(NULL);
}

void destroyMutexes() {
	if (pthread_mutex_destroy(&mutex1) != 0) {
		perror("Error destroying mutex1\n");
	}
	if (pthread_mutex_destroy(&mutex2) != 0) {
		perror("Error destroying mutex2\n");
	}
	if (pthread_mutex_destroy(&startMutex) != 0) {
		perror("Error destroying startMutex\n");
	}
}

int main() {
	pthread_t thread;
	if (pthread_mutex_init(&startMutex, NULL) != 0) {
		perror("Error creating mutex");
		return EXIT_FAILURE;
	}
	if (pthread_mutex_init(&mutex1, NULL) != 0) {
		perror("Error creating mutex");
		if (pthread_mutex_destroy(&startMutex) != 0) {
			perror("Error destroying startMutex\n");
		}
		return EXIT_FAILURE;
	}
	if (pthread_mutex_init(&mutex2, NULL) != 0) {
		perror("Error creating mutex");
		if (pthread_mutex_destroy(&mutex1) != 0) {
			perror("Error destroying mutex1\n");
		}
		if (pthread_mutex_destroy(&startMutex) != 0) {
			perror("Error destroying startMutex\n");
		}
		return EXIT_FAILURE;
	}
	pthread_mutex_lock(&startMutex);
	int err = pthread_create(&thread, NULL, threadFunction, NULL);
	if (err != 0) {
		perror("Error creating thread");
		return EXIT_FAILURE;
	}
	if (pthread_mutex_lock(&mutex2) != 0) {
		perror("pthread_mutex_lock: mutex2");
		destroyMutexes();
		return EXIT_FAILURE;
	}
	if (pthread_mutex_unlock(&startMutex) != 0) {
		perror("pthread_mutex_unlock: startMutex");
		destroyMutexes();
		return EXIT_FAILURE;
	}
	for (int i = 0; i < 10; ++i) {
		if (pthread_mutex_lock(&mutex1) != 0) {
			perror("pthread_mutex_lock: mutex1");
			return EXIT_FAILURE;
		}
		printf("parent: %d\n", i);
		if (pthread_mutex_unlock(&mutex2) != 0) {
			perror("pthread_mutex_unlock: mutex2");
			return EXIT_FAILURE;
		}
	}
	if (pthread_join(thread, NULL) != 0) {
		destroyMutexes();
		perror("Error joining thread");
		return EXIT_FAILURE;
	}
	destroyMutexes();
	pthread_exit(NULL);
}