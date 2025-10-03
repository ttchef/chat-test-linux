
CC = gcc
CFLAGS = -g -Wall -DWS_ENABLE_LOG_DEBUG -DWS_ENABLE_LOG_ERROR

SRC_DIR = src 
LIB_DIR = lib 
CLIENTS = clients 
BIN_DIR = bin

SERVER_BIN = $(BIN_DIR)/ws_server
CLIENT_BIN = $(BIN_DIR)/ws_client 
TEST_BIN = $(BIN_DIR)/ws_client_test
STATIC_LIB = libclient.a

# LIB 
LIB_SRC = $(wildcard $(LIB_DIR)/*.c)
LIB_OBJ = $(LIB_SRC:.c=.o)



all:
	gcc src/ws_server.c -o ws_server
	gcc clients/ws_client.c -o ws_client

	gcc -c -g -DWS_ENABLE_LOG_DEBUG -DWS_ENABLE_LOG_ERROR lib/ws_client_lib.c -o lib/ws_client_lib.o
	gcc -c -g -DWS_ENABLE_LOG_DEBUG -DWS_ENABLE_LOG_ERROR lib/ws_json.c -o lib/ws_json.o
	ar cr libclient.a lib/ws_client_lib.o lib/ws_json.o
	gcc -g clients/ws_client_test.c -o ws_client_test -L. -lclient

clean:
	rm ws_server 
	rm ws_client
