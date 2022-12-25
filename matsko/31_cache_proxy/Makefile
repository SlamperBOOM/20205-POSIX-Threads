CC=gcc

all: proxy.out

proxy.out: picohttpparser.o proxy.c
	$(CC) -Werror -Wpedantic -Wall picohttpparser.o proxy.c -o proxy.out

picohttpparser.o: picohttpparser-master/picohttpparser.c
	$(CC) -Werror -Wpedantic -Wall picohttpparser-master/picohttpparser.c -c

