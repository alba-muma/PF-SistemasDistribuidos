CC=gcc
CFLAGS=-Wall -Werror -Wextra

server: server.c
	$(CC) $(CFLAGS) -o server server.c

clean:
	rm -f server