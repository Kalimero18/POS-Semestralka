# Compiler & flags
CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -pthread -Iinclude

SRC_DIR = src

SERVER_BIN = server
CLIENT_BIN = client

# Source files
SERVER_SRCS = \
	$(SRC_DIR)/Smain.c \
	$(SRC_DIR)/net.c \
	$(SRC_DIR)/persist.c

CLIENT_SRCS = \
	$(SRC_DIR)/Cmain.c \
	$(SRC_DIR)/net.c

# Object files
SERVER_OBJS = $(SERVER_SRCS:.c=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

# Phony targets
.PHONY: all server client clean

all: server client

# Server build
server: $(SERVER_OBJS)
	$(CC) -pthread -o $(SERVER_BIN) $(SERVER_OBJS)

# Client build
client: $(CLIENT_OBJS)
	$(CC) -pthread -o $(CLIENT_BIN) $(CLIENT_OBJS)

# Compile rule
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(SRC_DIR)/*.o $(SERVER_BIN) $(CLIENT_BIN)

