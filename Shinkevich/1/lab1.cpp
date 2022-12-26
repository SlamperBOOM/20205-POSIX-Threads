//Напишите программу, которая создает нить. 
//Используйте атрибуты по умолчанию. 
//Родительская и вновь созданная нити должны 
//распечатать десять строк текста.

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
	return((void*)0);
}

int main()
{
	int err, i;
	pthread_t ntid;
	err = pthread_create(&ntid, NULL, thr_fn, NULL);
	if (err != 0)
	{
		printf("unable to create thread\n");
		exit(1);
	}
	for (i = 0; i < 10; ++i)
	{
		printf("Parent thread\n");
	}
	pthread_exit(0);
}
