.PHONY: all

all: main proxy client

main: main.c
	gcc -Werror -Wpedantic -Wall main.c -o main

proxy: picohttpparser.o socket_operations.o proxy.c
	gcc -Werror -Wpedantic -Wall picohttpparser.o socket_operations.o proxy.c -o proxy

picohttpparser.o: picohttpparser-master/picohttpparser.c
	gcc -Werror -Wpedantic -Wall picohttpparser-master/picohttpparser.c -c

socket_operations.o: socket_operations/socket_operations.c
	gcc -Werror -Wpedantic -Wall socket_operations/socket_operations.c -c

client: socket_operations.o client.c
	gcc -Werror -Wpedantic -Wall socket_operations.o client.c -o client
