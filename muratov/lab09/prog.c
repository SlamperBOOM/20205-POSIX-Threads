#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>

#define MAX_THREAD_COUNT 20000

typedef struct sum_args {
	int offset;
	int is_first_thread;
} sum_args;

typedef double res_t;

int is_running = 1;
int thread_count;
long long last_block_num;

void handle_int(int signum) {
	if(signum == SIGINT) {
		is_running = 0;
	}
}

void *partical_sum(void *args) {
	//sigset_t sigset;
	//sigemptyset(&sigset);
	//pthread_sigmask(SIG_SETMASK, &sigset, NULL);

	sum_args *params = (sum_args*) args;
	int offset = params->offset;
	const long long block_size = 100000;
    
	res_t* part_sum = calloc(1, sizeof(res_t));
	for(long long block_num = 0; is_running || (block_num <= last_block_num);
			block_num++) {
		for(long long iteration = block_size * block_num; iteration <
				block_size * (block_num + 1); iteration++) {
			long long pos = (iteration * thread_count + offset) * 4;
			*part_sum += 1. / (pos + 1.) - 1. / (pos + 3.);
		}
		if(last_block_num < block_num) {
			last_block_num = block_num;
		}
	}
	pthread_exit((void*) part_sum);
}

int main(int argc, char **argv) {
	struct sigaction sigact;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = handle_int;
	sigaction(SIGINT, &sigact, NULL);

	if(argc != 2) {
		fprintf(stderr, "Wrong argc\n");
		pthread_exit(NULL);
	}
	thread_count = atoi(argv[1]);
	if(thread_count > MAX_THREAD_COUNT) {
		thread_count = MAX_THREAD_COUNT;
		fprintf(stderr, "thread count decreased to %d\n", MAX_THREAD_COUNT);
	}

	pthread_t *threads = (pthread_t*) malloc(sizeof(pthread_t) *
			thread_count);
	if(threads == NULL) {
		perror("thread_t malloc");
		pthread_exit(NULL);
	}

	sum_args *sargs = (sum_args*) malloc(sizeof(sum_args) * thread_count);
	if(sargs == NULL) {
		perror("sargs malloc");
		free(threads);
		pthread_exit(NULL);
	}

	for(int index = 0; index < thread_count; index++) {
		sargs[index].offset = index;
	}

	for(int index = 0; index < thread_count; index++) {
		if(pthread_create(threads + index, NULL, partical_sum,
				sargs + index) != 0) {
			perror("couldn't create a new thread");
			printf("created %d threads instead of %d\n", index,
					thread_count);
			thread_count = index;
			break;
		}
	}

	res_t result = 0;
	for(int index = 0; index < thread_count; index++) {
		res_t *part_sum;
		if(pthread_join(threads[index], (void**) &part_sum) != 0) {
			perror("pthread join");
			printf("thread #%d\n", index);
		}
		else {
			result += *part_sum;
			free(part_sum);
		}
	}
	result *= 4;
	printf("\nPi=\t%.15f\n", result);
	printf("Pi_0=\t%.15f\n", M_PI);
	printf("d=\t%.15f\n", M_PI - result);

	free(threads);
	free(sargs);
	pthread_exit(NULL);
}

