all: server client

server: server.c cache.c cache.h
	gcc server.c cache.c -Wall -o server
client: client.c
	gcc client.c -Wall -o client
clean:
	rm -f server client
