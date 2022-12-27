#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

void* thr_fn(void* arg)
{
	int i;
	for (i = 0; i < 10; ++i)
	{
		printf("Child thread\n");
	}
	pthread_exit((void*)0);
}

int main()
{
	int err, i;
	void* tret;
	pthread_t ntid;
	err = pthread_create(&ntid, NULL, thr_fn, NULL);
	if (err != 0)
	{
		printf("невозможно создать поток");
		exit(1);
	}
	err = pthread_join(ntid, &tret);
	if (err != 0)
	{
		printf("unable to join thread");
		exit(1);
	}
	for (i = 0; i < 10; ++i)
	{
		printf("Parent thread\n");
	}
	exit(0);
}
