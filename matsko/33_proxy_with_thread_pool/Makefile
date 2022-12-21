CC=gcc

all: proxy.out

proxy.out: picohttpparser.o proxy.c cache.c task_queue.o sync_pipe.o
	$(CC) -Werror -Wpedantic -Wall picohttpparser.o task_queue.o sync_pipe.o cache.c proxy.c -lpthread -o proxy.out

picohttpparser.o: picohttpparser-master/picohttpparser.c
	$(CC) -Werror -Wpedantic -Wall picohttpparser-master/picohttpparser.c -c

task_queue.o: task_queue.c
	$(CC) -Werror -Wpedantic -Wall task_queue.c -lpthread -c

sync_pipe.o: sync_pipe/sync_pipe.c
	$(CC) -Werror -Wpedantic -Wall sync_pipe/sync_pipe.c -c
