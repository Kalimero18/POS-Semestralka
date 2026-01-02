CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -Werror -pthread -Iinclude
LDFLAGS = -pthread

SERVER = server
CLIENT = client

SERVER_SRCS = src/Smain.c src/net.c
CLIENT_SRCS = src/Cmain.c src/net.c

SERVER_OBJS = $(SERVER_SRCS:.c=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

all: $(SERVER) $(CLIENT)

$(SERVER): $(SERVER_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(CLIENT): $(CLIENT_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SERVER) $(CLIENT) $(SERVER_OBJS) $(CLIENT_OBJS)

.PHONY: all clean


