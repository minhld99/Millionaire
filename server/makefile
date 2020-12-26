CC=gcc
all: run
run: server.c server_.c
	$(CC) -pthread -o server server.c -lsqlite3 -std=c99
	$(CC) -pthread -o server_ server_.c -lsqlite3 -std=c99