all:
	gcc src/ws_server.c -o ws_server
	gcc src/ws_client.c -o ws_client

	# Dynamic lib 
	gcc src/ws_client_lib.c -fPIC -shared -o client_lib.so

clean:
	rm src/ws_server 
	rm src/ws_client
