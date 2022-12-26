#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdatomic.h>

atomic_bool finished = false;
pthread_mutex_t mutex;

typedef struct List{
	char* line;
	struct List* next;
}List;

List* mainlist;

void printList(List* list){
	if(list->line != NULL){
		printf("%s\n", list->line);
	}
	
	if(list->next != NULL){
		printList(list->next);
	}
}

void pushLine(List* list, char* line){
	if(list == NULL){
		int length = strlen(line);
		mainlist = (List*)malloc(sizeof(List));
		mainlist->line = (char*)malloc(length);
		strcpy(mainlist->line, line);
		mainlist->next = NULL;
	}else if(list->next == NULL){
		int length = strlen(line);
		List* node = (List*)malloc(sizeof(List));
		node->line = (char*)malloc(length);
		strcpy(node->line, line);
		node->next = NULL;
		list->next = node;
	}else{
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
	}else{
		clearList(list->next);
	}
}

static void sig_handler(int signo) {
    if (signo == SIGINT) {
        finished = true;
    }
}

void* sortThread(void* data){
	bool working = true;
	while(working){
		sleep(5);
		if(finished){
			break;
		}
		pthread_mutex_lock(&mutex);
		while(true){
			List* node = mainlist;
			bool sorted = true;
			while(node != NULL){
				if(node->next != NULL){
					if(strcmp(node->line, node->next->line) > 0){
						char* buf = node->line;
						node->line = node->next->line;
						node->next->line = buf;
						sorted = false;
					}
				}
				node = node->next;
			}
			if(sorted){
				break;
			}
		}
		pthread_mutex_unlock(&mutex);
	}
	printf("Sorted list:\n");
	pthread_mutex_lock(&mutex);
	printList(mainlist);
	pthread_mutex_unlock(&mutex);
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
    
    pthread_mutex_init(&mutex, NULL);
    pthread_t thread;
    
    mainlist = NULL;
    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&thread, &attr, sortThread, NULL);
    pthread_attr_destroy(&attr);
    
    char line[81];
    while(!finished){
    	int readbytes = read(STDIN_FILENO, line, 80);
    	if(line[0] == '\n'){
    		printf("List:\n");
    		pthread_mutex_lock(&mutex);
    		printList(mainlist);
    		pthread_mutex_unlock(&mutex);
    	}else{
    		line[readbytes - 1] = '\0';
    		pthread_mutex_lock(&mutex);
    		pushLine(mainlist, line);
    		pthread_mutex_unlock(&mutex);
    	}
    	memset(line, 0, 81);
    }
    pthread_join(thread, NULL);
    clearList(mainlist);
    pthread_mutex_destroy(&mutex);
    pthread_exit(NULL);
}
