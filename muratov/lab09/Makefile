CC=gcc
CFLAGS=-c -Wall -Wextra

FILENAME=pi-counter

all: $(FILENAME).out

$(FILENAME).out: $(FILENAME).o
	$(CC) -pthread $(FILENAME).o -o $(FILENAME).out -lm

$(FILENAME).o: $(FILENAME).c
	$(CC) $(CFLAGS) $(FILENAME).c -o $(FILENAME).o

clean:
	rm -rf *.o
