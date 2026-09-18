CC = gcc
CFLAGS = -Wall -Wextra -g

build: tema1

tema1: tema1.c
	$(CC) $(CFLAGS) -o tema1 tema1.c

clean:
	rm -f tema1
