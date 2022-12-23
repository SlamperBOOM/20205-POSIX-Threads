.PHONY: clean

proxy: proxy.c socket_operations.o cache.o http_socket.o picohttpparser.o log.o
	clang -DLOG_USE_COLOR -Wall -pedantic -fsanitize=address proxy.c http_socket.o socket_operations.o cache.o picohttpparser.o log.o -o proxy

picohttpparser.o: http_socket/picohttpparser/picohttpparser.c http_socket/picohttpparser/picohttpparser.h
	clang -DLOG_USE_COLOR -Wall -pedantic -fsanitize=address http_socket/picohttpparser/picohttpparser.c -c



cache.o: http_socket/cache.c http_socket/cache.h
	clang -DLOG_USE_COLOR -Wall -pedantic -fsanitize=address http_socket/cache.c -c

http_socket.o: http_socket/http_socket.c http_socket/http_socket.h
	clang -DLOG_USE_COLOR -Wall -pedantic -fsanitize=address http_socket/http_socket.c -c

socket_operations.o: socket_operations/socket_operations.c socket_operations/socket_operations.h
	clang -DLOG_USE_COLOR -Wall -pedantic -fsanitize=address socket_operations/socket_operations.c -c

log.o: log/src/log.c log/src/log.h
	clang -DLOG_USE_COLOR -Wall -pedantic -fsanitize=address log/src/log.c -c

clean:
	rm ./*.o