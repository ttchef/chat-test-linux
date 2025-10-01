
all:
	gcc src/ws_server.c -o ws_server
	gcc src/ws_client.c -o ws_client

windows_ui:
	x86_64-w64-mingw32-gcc ui.c -o ui.exe -mwindows
