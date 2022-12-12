.PHONY: all

all: main proxy client

main: main.c
	gcc -Werror -Wpedantic -Wall main.c -o main

proxy: picohttpparser.o socket_operations.o hashmap.o proxy.c
	gcc -Werror -Wpedantic -Wall picohttpparser.o socket_operations.o hashmap.o proxy.c -o proxy

picohttpparser.o: picohttpparser/picohttpparser.c
	gcc -Werror -Wpedantic -Wall picohttpparser/picohttpparser.c -c

hashmap.o: hashmap/hashmap.c
	gcc -Werror -Wpedantic -Wall hashmap/hashmap.c -c

socket_operations.o: socket_operations/socket_operations.c
	gcc -Werror -Wpedantic -Wall socket_operations/socket_operations.c -c

client: socket_operations.o client.c
	gcc -Werror -Wpedantic -Wall socket_operations.o client.c -o client
