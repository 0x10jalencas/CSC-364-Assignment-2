# Used CSC-453 conventions (Professor Nico) for Makefiles where we separate
# compile/link variables, explicit dependency rules, and PHONY targets for build
# helpers.

CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g

LD = gcc
LDFLAGS =

CLIENT_OBJS = client.o protocol.o net.o client_state.o
SERVER_OBJS = server.o protocol.o net.o server_state.o

.PHONY: all clean

all: client server

client: $(CLIENT_OBJS)
	$(LD) $(LDFLAGS) -o client $(CLIENT_OBJS)

server: $(SERVER_OBJS)
	$(LD) $(LDFLAGS) -o server $(SERVER_OBJS)

client.o: client.c client_state.h net.h protocol.h
	$(CC) $(CFLAGS) -c -o client.o client.c

server.o: server.c server_state.h net.h protocol.h
	$(CC) $(CFLAGS) -c -o server.o server.c

protocol.o: protocol.c protocol.h
	$(CC) $(CFLAGS) -c -o protocol.o protocol.c

net.o: net.c net.h
	$(CC) $(CFLAGS) -c -o net.o net.c

client_state.o: client_state.c client_state.h protocol.h
	$(CC) $(CFLAGS) -c -o client_state.o client_state.c

server_state.o: server_state.c server_state.h protocol.h net.h
	$(CC) $(CFLAGS) -c -o server_state.o server_state.c

clean:
	rm -f *.o client server