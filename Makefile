CC = gcc
CFLAGS = -g -Wall -DWS_ENABLE_LOG_DEBUG -DWS_ENABLE_LOG_ERROR
AR = ar
ARFLAGS = cr

SRC_DIR = src
LIB_DIR = lib
CLIENTS = clients
BIN_DIR = bin

SERVER_BIN = $(BIN_DIR)/ws_server
CLIENT_BIN = $(BIN_DIR)/ws_client
TEST_BIN   = $(BIN_DIR)/ws_client_test
STATIC_LIB = libclient.a

LIB_SRC = $(wildcard $(LIB_DIR)/*.c)
LIB_OBJ = $(LIB_SRC:.c=.o)

all: $(SERVER_BIN) $(CLIENT_BIN) $(STATIC_LIB) $(TEST_BIN)

$(SERVER_BIN): $(SRC_DIR)/ws_server.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(CLIENT_BIN): $(CLIENTS)/ws_client.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(TEST_BIN): $(CLIENTS)/ws_client_test.c $(STATIC_LIB)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ -L. -lclient

$(STATIC_LIB): $(LIB_OBJ)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN_DIR) $(STATIC_LIB) $(LIB_OBJ)

.PHONY: all clean

