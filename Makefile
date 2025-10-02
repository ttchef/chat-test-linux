all:
	gcc src/ws_server.c -o ws_server
	gcc src/ws_client.c -o ws_client

	# Dynamic lib
	#gcc src/ws_client_lib.c -fPIC -shared -o libclient.so
	gcc -c -g -DWS_ENABLE_LOG_DEBUG -DWS_ENABLE_LOG_ERROR src/ws_client_lib.c -o ws_client_lib.o
	ar cr libclient.a ws_client_lib.o
	gcc -g src/ws_client_test.c -o ws_client_test -L. -lclient

clean:
	rm src/ws_server 
	rm src/ws_client
