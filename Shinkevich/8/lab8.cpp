#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define num_steps 200000000
#define thread_limit 380 //experimentally found limit on ccfit
int numThreads;

typedef struct iterationsData
{
	int number;
	double partSum;
}tIterationsData;

void* calcPartSum(void* data)
{
	int id = ((tIterationsData*)data)->number;
	int i;
	double sum = 0.0;
	for (i = id; i < num_steps; i += numThreads)
	{
		sum += 1.0 / (i * 4.0 + 1.0);
		sum -= 1.0 / (i * 4.0 + 3.0);
	}
	((tIterationsData*)data)->partSum = sum;
	pthread_exit((void*)0);
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "not enough arguments\n");
		exit(-1);
	}
	numThreads = atoi(argv[1]);
	if (numThreads < 1)
	{
		fprintf(stderr, "incorrect number of threads\n");
		exit(-1);
	}
	if (numThreads > thread_limit)
	{
		fprintf(stderr, "impossible to create such a number of threads\n");
		exit(-1);
	}
	tIterationsData* threadsData = (tIterationsData*)malloc(sizeof(tIterationsData) * numThreads);
	int i;
	pthread_t threads[numThreads];
	int status;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	for (i = 0; i < numThreads; ++i)
	{
		threadsData[i].number = i;
		status = pthread_create(&threads[i], &attr, calcPartSum, (void*)(threadsData + i));
		if (status != 0)
		{
			fprintf(stderr, "the pthread_create function returned an error code: %d\n", status);
			free(threadsData);
			exit(-1);
		}
	}

	pthread_attr_destroy(&attr);

	double pi = 0.0;
	for (i = 0; i < numThreads; ++i)
	{
		status = pthread_join(threads[i], NULL);
		if (status != 0)
		{
			fprintf(stderr, "the pthread_join function returned an error code: %d\n", status);
			free(threadsData);
			exit(-1);
		}
		pi += threadsData[i].partSum;
	}
	pi *= 4;
	printf("pi = %lf\n", pi);
	free(threadsData);
	pthread_exit((void*)0);
}
