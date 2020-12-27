CC=gcc
all: run
run: server.c
	$(CC) -pthread -o server server.c -lsqlite3 -std=c99
