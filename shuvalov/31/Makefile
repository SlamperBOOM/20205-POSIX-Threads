.PHONY: all

all: main parser

main: main.c
	gcc -Werror -Wpedantic -Wall main.c -o main

parser: picohttpparser.o parser.c
	gcc -Werror -Wpedantic -Wall picohttpparser.o parser.c -o parser

picohttpparser.o: picohttpparser-master/picohttpparser.c
	gcc -Werror -Wpedantic -Wall picohttpparser-master/picohttpparser.c -c
