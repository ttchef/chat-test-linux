
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
#include <netdb.h>
#include <errno.h>

typedef struct wsClient wsClient;

typedef void (*wsMessageCallbackPFN)(wsClient* client, const char* message, const char* username, time_t time);

struct wsClient {
    int32_t id;
    const char* ip;
    const char* port;
    struct pollfd fds[2];
    const char* username;
    wsMessageCallbackPFN messageFunc;
};

int32_t wsInitClient(wsClient* client, const char* ip, const char* port, const char* username);
int32_t wsSendMessage(wsClient* client, const char* message);
int32_t wsSetMessageCallback(wsClient* client, wsMessageCallbackPFN functionPtr);
int32_t wsClientListen(wsClient* client);
int32_t wsDeinitClient(wsClient* client);

// Internal
int32_t __ws_encode_frame(const char* payload, int32_t len, uint8_t* frame);
int32_t __ws_decode_frame(uint8_t* data, int32_t len, char* payload);
int32_t __ws_client_handshake(int32_t sockfd, const char* ip);

// LOGGING
#ifdef WS_ENABLE_LOG_DEBUG
    #define WS_LOG_DEBUG(msg, ...) \
        do { \
            fprintf(stdout, "[WS DEBUG] "); \
            fprintf(stdout, msg, ##__VA_ARGS__); \
        } while(0) 
#else 
    #define WS_LOG_DEBUG(msg, ...) 
#endif

#ifdef WS_ENABLE_LOG_ERROR
    #define WS_LOG_ERROR(msg, ...) \
        do { \
            fprintf(stderr, "[WS ERROR]"); \
            fprintf(stderr, msg, ##__VA_ARGS__); \
        } while(0)
#else 
    #define WS_LOG_ERROR(msg, ...)
#endif

#endif
