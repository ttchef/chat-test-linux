
#ifndef WS_CLIENT_LIB_H
#define WS_CLIENT_LIB_H

#include "ws_defines.h"

int32_t wsInitClient(wsClient* client, const char* ip, const char* port, const char* username);
int32_t wsSendMessage(wsClient* client, const char* message);
int32_t wsSendMessageN(wsClient* client, const char* message, size_t n);
int32_t wsSendJson(wsClient* client, wsJson* obj);
int32_t wsSetMessageCallback(wsClient* client, wsMessageCallbackPFN functionPtr);
int32_t wsClientListen(wsClient* client);
int32_t wsDeinitClient(wsClient* client);


#endif
