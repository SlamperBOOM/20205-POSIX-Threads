#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>

sem_t sem1;
sem_t sem2;

void* thr_fn(void* arg)
{
	int i;
	for (i = 0; i < 10; ++i)
	{
		sem_wait(&sem2);
		printf("Child thread\t@@@@@@@@@\t[%d]\n", i);
		sem_post(&sem1);
	}
	pthread_exit((void*)0);
}

int main()
{
	int err, i;
	sem_init(&sem1, 0, 1);
	sem_init(&sem2, 0, 0);

	pthread_t ntid;

	err = pthread_create(&ntid, NULL, thr_fn, NULL);
	if (err != 0)
	{
		fprintf(stderr, "unable to create thread\n");
		pthread_exit((void*)1);
	}

	for (i = 0; i < 10; ++i)
	{
		sem_wait(&sem1);
		printf("Parent thread\t~~~~~~~~~~~~\t[%d]\n", i);
		sem_post(&sem2);
	}

	err = pthread_join(ntid, NULL);
	if (err != 0)
	{
		fprintf(stderr, "unable to join thread\n");
		pthread_exit((void*)1);
	}
	sem_destroy(&sem1);
	sem_destroy(&sem2);
	pthread_exit((void*)0);
}
