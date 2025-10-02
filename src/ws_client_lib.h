
#ifndef WS_CLIENT_LIB_H
#define WS_CLIENT_LIB_H

#define WS_ERROR -1
#define WS_OK 0

#define WS_BUFFER_SIZE 4096

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <string.h> 
#include <poll.h> 
#include <fcntl.h>
#include <unistd.h>

typedef struct {
    int32_t id;
    const char* ip;
    const char* port;
} wsClient;

int32_t initClient(wsClient* client, const char* ip, const char* port);
int32_t connectClient();
int32_t sendMessage(const char* message);
int32_t deinitClient(wsClient* client);

// Internal
int32_t __ws_encode_frame(const char* payload, int32_t len, uint8_t* frame);
int32_t __ws_decode_frame(uint8_t* data, int32_t len, char* payload);
int32_t __ws_client_handshake(int32_t sockfd, const char* ip);

#endif
