#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

pthread_mutex_t mutexes[3];

#define COMMON 0
#define PARENT 1
#define CHILD  2

void* thr_fn(void* arg)
{
	pthread_mutex_lock(&mutexes[CHILD]);
	int i;
	for (i = 0; i < 10; ++i)
	{
		pthread_mutex_lock(&mutexes[PARENT]);
		printf("Child thread\t@@@@@@@@@\t[%d]\n", i);
		pthread_mutex_unlock(&mutexes[CHILD]);
		pthread_mutex_lock(&mutexes[COMMON]);
		pthread_mutex_unlock(&mutexes[PARENT]);
		pthread_mutex_lock(&mutexes[CHILD]);
		pthread_mutex_unlock(&mutexes[COMMON]);
	}
	pthread_mutex_unlock(&mutexes[CHILD]);
	return((void*)0);
}

int main()
{
	int err, i;

	pthread_mutexattr_t attr;
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	for (i = 0; i < 3; i++) 
	{
		pthread_mutex_init(&mutexes[i], &attr);
	}

	pthread_t ntid;

	pthread_mutex_lock(&mutexes[PARENT]);
	err = pthread_create(&ntid, NULL, thr_fn, NULL);
	if (err != 0)
	{
		printf("unable to create thread\n");
		for (i = 0; i < 3; i++)
		{
			pthread_mutex_destroy(&mutexes[i]);
		}
		exit(1);
	}

	for (i = 0; i < 10; ++i)
	{
		printf("Parent thread\t~~~~~~~~~~~~\t[%d]\n", i);
		pthread_mutex_lock(&mutexes[COMMON]);
		pthread_mutex_unlock(&mutexes[PARENT]);
		pthread_mutex_lock(&mutexes[CHILD]);
		pthread_mutex_unlock(&mutexes[COMMON]);
		pthread_mutex_lock(&mutexes[PARENT]);
		pthread_mutex_unlock(&mutexes[CHILD]);
	}
	pthread_mutex_unlock(&mutexes[PARENT]);

	for (i = 0; i < 3; i++) 
	{
		pthread_mutex_destroy(&mutexes[i]);
	}
	pthread_exit(0);
}
