#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>

#define num_steps 200000000
#define thread_limit 32768
int numThreads;
atomic_bool finished = false;
long long stepFinished = 0;

static void sig_handler(int signo) {
    if (signo == SIGINT) {
        finished = true;
    }
}

typedef struct iterationsData
{
    int number;
    double partSum;
}tIterationsData;

void* calcPartSum(void* data)
{
    int id = ((tIterationsData*)data)->number;
    long long step = 1, i = id;
    double sum = 0.0;
    bool working = true;
    while (working)
    {
        if (step % 100000 == 0) {
            if (finished) {
                if (stepFinished == 0) {
                    stepFinished = step;
                    working = false;
                }
                else if (step >= stepFinished) {
                    working = false;
                }
            }
            if (!working) {
                break;
            }
            
        }
        sum += 1.0 / (i * 4.0 + 1.0);
        sum -= 1.0 / (i * 4.0 + 3.0);
        i += numThreads;
        step++;
    }
    ((tIterationsData*)data)->partSum = sum;
    printf("%d %.15lf\n", id, sum);
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

    struct sigaction sigact;
    memset(&sigact, 0, sizeof(sigact));
    sigemptyset(&sigact.sa_mask);
    sigact.sa_handler = sig_handler;
    int ret;
    if ((ret = sigaction(SIGINT, &sigact, NULL)) == -1) {
        perror("sigaction failed");
        return 0;
    }

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
            numThreads = i+1;
            break;
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
            threadsData[i].partSum = 0;
        }
        pi += threadsData[i].partSum;
    }
    pi *= 4;
    printf("pi = %.15lf\n%lld\n", pi, stepFinished*numThreads);
    free(threadsData);
    pthread_exit((void*)0);
}
