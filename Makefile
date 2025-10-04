CC = gcc
CFLAGS = -g -Wall -DWS_ENABLE_LOG_DEBUG -DWS_ENABLE_LOG_ERROR -D_GNU_SOURCE
AR = ar
ARFLAGS = cr

LIB_DIR = lib
CLIENTS_DIR = clients
BIN_DIR = bin
SERVERS_DIR = servers

CLIENT_BIN = $(BIN_DIR)/ws_client
TEST_BIN = $(BIN_DIR)/ws_client_test
JSON_TEST_BIN = $(BIN_DIR)/test_json
STATIC_LIB = libclient.a
SHARED_LIB = $(BIN_DIR)/libwsclient.so

LIB_SRC = $(wildcard $(LIB_DIR)/*.c)
LIB_OBJ = $(LIB_SRC:.c=.o)

all: $(STATIC_LIB) $(SHARED_LIB) $(CLIENT_BIN) $(TEST_BIN) $(JSON_TEST_BIN) c-server

c-server: $(STATIC_LIB)
	@$(MAKE) -C $(SERVERS_DIR)/c-server

$(CLIENT_BIN): $(CLIENTS_DIR)/c-client/ws_client.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(TEST_BIN): $(CLIENTS_DIR)/c-client/ws_client_test.c $(STATIC_LIB)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ -L. -lclient

$(JSON_TEST_BIN): test/test_json.c $(LIB_DIR)/ws_json.o $(LIB_DIR)/ws_globals.h
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(LIB_DIR)/ws_json.o -o $@

$(STATIC_LIB): $(LIB_OBJ)
	$(AR) $(ARFLAGS) $@ $^

$(SHARED_LIB): $(LIB_SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -fPIC -shared $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN_DIR) $(STATIC_LIB) $(LIB_OBJ)
	@$(MAKE) -C $(SERVERS_DIR)/c-server clean

test-json: $(JSON_TEST_BIN)
	@echo "Running JSON tests..."
	@./$(JSON_TEST_BIN)

.PHONY: all clean c-server test-json

