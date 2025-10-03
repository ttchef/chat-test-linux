
#ifndef WS_DEFINES_H
#define WS_DEFINES_H

#include "ws_globals.h"
#include "ws_json.h"

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


#endif
