
#ifndef WS_CLIENT_LIB_H
#define WS_CLIENT_LIB_H

#include "ws_defines.h"

int32_t wsInitClient(wsClient* client, const char* ip, const char* port, const char* username);
int32_t wsSendMessage(wsClient* client, const char* message);
int32_t wsSendMessageN(wsClient* client, const char* message, size_t n);
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
