#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#define MAX_THREAD_COUNT 20000

typedef struct SumArgs {
	int offset;
} SumArgs;

typedef double ResultType;

int isRunning = 1;
int threadCount;
long long lastBlockNum;

void HandleSigint(int signum) {
	if (signum == SIGINT) {
		isRunning = 0;
	}
}

void *GetPartialSum(void *args) {
	SumArgs *params = (SumArgs *)args;
	int offset = params->offset;
	const long long kBlockSize = 100000;

	ResultType *part_sum = calloc(1, sizeof(ResultType));
	for (long long blockNumber = 0; isRunning || (blockNumber <= lastBlockNum);
		 blockNumber++) {
		for (long long iteration = kBlockSize * blockNumber; iteration <
			kBlockSize * (blockNumber + 1); iteration++) {
			long long pos = (iteration * threadCount + offset) * 4;
			*part_sum += 1. / ((double)pos + 1.) - 1. / ((double)pos + 3.);
		}
		if (lastBlockNum < blockNumber) {
			lastBlockNum = blockNumber;
		}
	}
	pthread_exit((void *)part_sum);
}

int main(int argc, char **argv) {
	struct sigaction sigact;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = HandleSigint;
	sigaction(SIGINT, &sigact, NULL);

	if (argc != 2) {
		fprintf(stderr, "usage: ./pi-counter.out threadCount\n");
		return 1;
	}
	threadCount = atoi(argv[1]);
	if (threadCount <= 0) {
		fprintf(stderr, "pi-counter.out: threadCount must be a positive "
						"number\n");
		return 1;
	}
	if (threadCount > MAX_THREAD_COUNT) {
		threadCount = MAX_THREAD_COUNT;
		fprintf(stderr, "pi-counter.out: thread count decreased to %d\n",
				MAX_THREAD_COUNT);
	}

	pthread_t *threads = (pthread_t *) malloc(sizeof(pthread_t) * threadCount);
	if (threads == NULL) {
		perror("pi-counter.out: couldn't allocate memory for threads");
		return 1;
	}

	SumArgs *sArgs = (SumArgs *)malloc(sizeof(SumArgs) * threadCount);
	if (sArgs == NULL) {
		perror("pi-counter.out: couldn't allocate memory for thread args");
		free(threads);
		return 1;
	}

	for (int index = 0; index < threadCount; index++) {
		sArgs[index].offset = index;
	}

	for (int index = 0; index < threadCount; index++) {
		if (pthread_create(threads + index, NULL, GetPartialSum,
						   sArgs + index) != 0) {
			perror("pi-counter.out: couldn't create once more thread");
			printf("created %d threads instead of %d\n", index,
				   threadCount);
			threadCount = index;
			break;
		}
	}

	ResultType result = 0;
	for (int index = 0; index < threadCount; index++) {
		ResultType *partialSum;
		if (pthread_join(threads[index], (void **)&partialSum) != 0) {
			perror("pi-counter.out: couldn't join a thread");
			printf("thread #%d\n refused to be joined", index);
		} else {
			result += *partialSum;
			free(partialSum);
		}
	}
	result *= 4;
	printf("\nPi=\t%.15f\n", result);
	printf("Pi_0=\t%.15f\n", M_PI);
	printf("d=\t%.15f\n", M_PI - result);

	free(threads);
	free(sArgs);
	pthread_exit(NULL);
}

