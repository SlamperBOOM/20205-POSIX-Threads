.PHONY: all

all: proxy client

proxy: cache.o hashmap.o http_socket.o picohttpparser.o socket_operations.o proxy.c
	gcc -Werror -Wpedantic -Wall cache.o hashmap.o http_socket.o picohttpparser.o socket_operations.o proxy.c -o proxy

http_socket.o: picohttpparser.o http_socket/http_socket.c
	gcc -Werror -Wpedantic -Wall picohttpparser.o http_socket/http_socket.c -c

cache.o: picohttpparser.o hashmap.o http_socket/cache.c
	gcc -Werror -Wpedantic -Wall picohttpparser.o hashmap.o http_socket/cache.c -c

picohttpparser.o: http_socket/picohttpparser/picohttpparser.c
	gcc -Werror -Wpedantic -Wall http_socket/picohttpparser/picohttpparser.c -c

hashmap.o: http_socket/hashmap/hashmap.c
	gcc -Werror -Wpedantic -Wall http_socket/hashmap/hashmap.c -c

socket_operations.o: socket_operations/socket_operations.c
	gcc -Werror -Wpedantic -Wall socket_operations/socket_operations.c -c

client: socket_operations.o client.c
	gcc -Werror -Wpedantic -Wall socket_operations.o client.c -o client