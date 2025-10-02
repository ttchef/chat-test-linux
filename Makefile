all:
	gcc src/ws_server.c -o ws_server
	gcc clients/ws_client.c -o ws_client

	# Dynamic lib
	gcc -c -g -DWS_ENABLE_LOG_DEBUG -DWS_ENABLE_LOG_ERROR lib/ws_client_lib.c -o lib/ws_client_lib.o
	ar cr libclient.a lib/ws_client_lib.o
	gcc -g clients/ws_client_test.c -o ws_client_test -L. -lclient

clean:
	rm ws_server 
	rm ws_client
