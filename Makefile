CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -pthread -Iinclude

SRC_DIR = src
OBJ_DIR = src

SERVER_BIN = server
CLIENT_BIN = client

SERVER_SRCS = \
	$(SRC_DIR)/Smain.c \
	$(SRC_DIR)/net.c \
	$(SRC_DIR)/persist.c

CLIENT_SRCS = \
	$(SRC_DIR)/Cmain.c \
	$(SRC_DIR)/net.c

SERVER_OBJS = $(SERVER_SRCS:.c=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

.PHONY: all clean server client

all: server client

server: $(SERVER_OBJS)
	$(CC) -pthread -o $(SERVER_BIN) $(SERVER_OBJS)

client: $(CLIENT_OBJS)
	$(CC) -pthread -o $(CLIENT_BIN) $(CLIENT_OBJS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o $(SERVER_BIN) $(CLIENT_BIN)

