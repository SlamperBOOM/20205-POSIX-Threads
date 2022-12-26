#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdatomic.h>
#include <fcntl.h>

atomic_bool finished = false;

typedef struct List{
	char* line;
	struct List* next;
	pthread_mutex_t mutex;
}List;

List* mainlist;

void printList(List* list){
	if(list == NULL){
		printf("*Empty*\n");
	}else{
		pthread_mutex_lock(&(list->mutex));
		if(list->line != NULL){
			printf("%s\n", list->line);
		}
		pthread_mutex_unlock(&(list->mutex));
		if(list->next != NULL){
			printList(list->next);
		}
	}
}

void pushLine(List* list, char* line){
	if(list == NULL){
		int length = strlen(line);
		mainlist = (List*)malloc(sizeof(List));
		
		mainlist->line = (char*)malloc(length);
		strcpy(mainlist->line, line);
		mainlist->next = NULL;
		pthread_mutex_init(&(mainlist->mutex), NULL);
		return;
	}
	pthread_mutex_lock(&(list->mutex));
	if(list->next == NULL){
		int length = strlen(line);
		List* node = (List*)malloc(sizeof(List));
		
		node->line = (char*)malloc(length);
		strcpy(node->line, line);
		node->next = NULL;
		pthread_mutex_init(&(node->mutex), NULL);
		
		list->next = node;
		pthread_mutex_unlock(&(list->mutex));
		return;
	}else{
		pthread_mutex_unlock(&(list->mutex));
		pushLine(list->next, line);
	}
}

void clearList(List* list){
	if(list == NULL){
		return;
	}
	if(list->next == NULL){
		free(list->line);
		free(list);
		pthread_mutex_destroy(&(list->mutex));
	}else{
		clearList(list->next);
	}
}

static void sig_handler(int signo) {
    if (signo == SIGINT) {
        finished = true;
    }
}

void* readThread(void* data){
	int fd;
	if((fd = open("input.txt", O_RDONLY)) == -1){
		perror("Input file doesn't exist");
		pthread_exit((void*)0);
	}
	char line[81];
	int pos = 0;
	while(true){
		if(finished){
			break;
		}
		if(lseek(fd, pos, SEEK_SET) == -1){
			perror("Failed to seek");
			break;
		}
		int readBytes = read(fd, line, 80);
		if(readBytes == 0){
			break;
		}
		if(line[0] == '\n'){
    		printf("List:\n");
    		printList(mainlist);
    		printf("\n");
    	}else{
    		for(int i=1; i<readBytes; ++i){
    			if(line[i] == '\n'){
    				readBytes = i;
    				break;
    			}
    		}
    		line[readBytes] = '\0';
    		pos += readBytes + 1;
    		pushLine(mainlist, line);
    	}
    	memset(line, 0, 81);
    	sleep(1);
	}
	pthread_exit((void*)0);
}

void* sortThread(void* data){
	bool working = true;
	while(working){
		if(finished){
			break;
		}
		while(true){
			List* node = mainlist;
			bool sorted = true;
			while(node != NULL){
				bool swapped = false;
				pthread_mutex_lock(&(node->mutex));
				if(node->next != NULL){
					pthread_mutex_lock(&(node->next->mutex));
					
					if(strcmp(node->line, node->next->line) > 0){
						char* buf = node->line;
						node->line = node->next->line;
						node->next->line = buf;
						sorted = false;
						swapped = true;
					}
					pthread_mutex_unlock(&(node->next->mutex));
				}
				pthread_mutex_unlock(&(node->mutex));
				node = node->next;
				if(finished){
					break;
				}
				if(swapped){
					sleep(1);
				}
			}
			if(sorted){
				break;
			}
		}
		sleep(1);
	}
	
	pthread_exit((void*)0);
}

int main(int argc, char** argv){
	struct sigaction sigact;
    memset(&sigact, 0, sizeof(sigact));
    sigemptyset(&sigact.sa_mask);
    sigact.sa_handler = sig_handler;
    int ret;
    if ((ret = sigaction(SIGINT, &sigact, NULL)) == -1) {
        perror("sigaction failed");
        return 0;
    }
    
    int threadCount = 5;
    pthread_t* thread = (pthread_t*)malloc(sizeof(pthread_t) * threadCount);
    pthread_t readerThread;
    
    mainlist = NULL;
    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    if(pthread_create(&readerThread, &attr, readThread, NULL) != 0){
    	fprintf(stderr, "Couldn't create thread");
    }
    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = 1000000000L / threadCount;
    for(int i=0; i<threadCount; ++i){
    	if( (pthread_create(&thread[i], &attr, sortThread, NULL)) != 0){
    		fprintf(stderr, "Couldn't create more than %d threads", i+1);
    		threadCount = i+1;
    	}
    	nanosleep(&delay, NULL);
    }
    pthread_attr_destroy(&attr);
    
    char line[81];
    while(!finished){
    	int readbytes = read(STDIN_FILENO, line, 80);
    	if(line[0] == '\n'){
    		printf("List:\n");
    		printList(mainlist);
    		printf("\n");
    	}else{
    		line[readbytes - 1] = '\0';
    		pushLine(mainlist, line);
    	}
    	memset(line, 0, 81);
    }
    if((pthread_join(readerThread, NULL)) != 0){
    		fprintf(stderr, "Error while joining thread");
    	}
    for(int i=0; i<threadCount; ++i){
    	if((pthread_join(thread[i], NULL)) != 0){
    		fprintf(stderr, "Error while joining threads");
    	}
    }
    
    printf("\nSorted list:\n");
    printList(mainlist);
    
    clearList(mainlist);
    pthread_exit(NULL);
}
