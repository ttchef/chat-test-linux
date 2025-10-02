all:
	gcc src/ws_server.c -o ws_server
	gcc src/ws_client.c -o ws_client


clean:
	rm src/ws_server 
	rm src/ws_client