CC = gcc
CFLAGS = -g -Wall -DWS_ENABLE_LOG_DEBUG -DWS_ENABLE_LOG_ERROR
AR = ar
ARFLAGS = cr

LIB_DIR = lib
CLIENTS_DIR = clients
BIN_DIR = bin
SERVERS_DIR = servers

CLIENT_BIN = $(BIN_DIR)/ws_client
TEST_BIN = $(BIN_DIR)/ws_client_test
STATIC_LIB = libclient.a

LIB_SRC = $(wildcard $(LIB_DIR)/*.c)
LIB_OBJ = $(LIB_SRC:.c=.o)

all: $(STATIC_LIB) $(CLIENT_BIN) $(TEST_BIN) c-server

c-server: $(STATIC_LIB)
	@$(MAKE) -C $(SERVERS_DIR)/c-server

$(CLIENT_BIN): $(CLIENTS_DIR)/ws_client.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(TEST_BIN): $(CLIENTS_DIR)/ws_client_test.c $(STATIC_LIB)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ -L. -lclient

$(STATIC_LIB): $(LIB_OBJ)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN_DIR) $(STATIC_LIB) $(LIB_OBJ)
	@$(MAKE) -C $(SERVERS_DIR)/c-server clean

.PHONY: all clean c-server

