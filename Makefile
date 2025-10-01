# Makefile for WebSocket Chat Application
# Builds both server and client executables for Linux

# Default target: build both server and client
all:
	# Compile WebSocket server
	# - Input: src/ws_server.c
	# - Output: ws_server executable in current directory
	# - Links standard C library and networking libraries
	gcc src/ws_server.c -o ws_server

	# Compile WebSocket client
	# - Input: src/ws_client.c
	# - Output: ws_client executable in current directory
	# - Links standard C library and networking libraries
	gcc src/ws_client.c -o ws_client
