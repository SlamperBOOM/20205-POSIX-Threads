all: server client

server: server.c cache.c cache.h
	clang server.c cache.c -Wall -o server
client: client.c
	clang client.c -Wall -o client
clean:
	rm -f server client