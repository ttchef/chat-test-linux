
#ifndef WS_DEFINES_H
#define WS_DEFINES_H

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
