CC = gcc
CFLAGS = -g -Wall -DWS_ENABLE_LOG_DEBUG -DWS_ENABLE_LOG_ERROR
AR = ar
ARFLAGS = cr

LIB_DIR = ../../lib
BIN_DIR = ../../bin

SERVER_BIN = $(BIN_DIR)/ws_server
STATIC_LIB = ../../libclient.a

LIB_SRC = $(wildcard $(LIB_DIR)/*.c)
LIB_OBJ = $(LIB_SRC:.c=.o)

all: $(STATIC_LIB) $(SERVER_BIN)

$(SERVER_BIN): ws_server.c $(STATIC_LIB)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ -L../.. -lclient

$(STATIC_LIB): $(LIB_OBJ)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SERVER_BIN) $(STATIC_LIB) $(LIB_OBJ)

.PHONY: all clean
