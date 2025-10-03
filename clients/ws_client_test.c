#define WS_ENABLE_LOG_DEBUG
#define WS_ENABLE_LOG_ERROR

#include "../lib/ws_client_lib.h"

void messageCallback(wsClient* client, time_t time, wsJson* root) {
    char buffer[WS_BUFFER_SIZE];
    wsJsonToString(root, buffer, WS_BUFFER_SIZE);
    printf("%s\n", buffer);
}

int main() {
    wsClient client = {0};
    fprintf(stderr, "[TEST] Starting client initialization...\n");
    if (wsInitClient(&client, "127.0.0.1", "9999", "ttchef") == WS_ERROR) {
        fprintf(stderr, "Failed to init wsClient!\n");
        return -1;
    }
    
    wsSetOnMessageCallback(&client, (wsOnMessageCallbackPFN)messageCallback, WS_MESSAGE_CALLBACK_JSON);

    fprintf(stderr, "[TEST] Client initialized successfully!\n");

    while (1) {
        
        wsClientListen(&client);
    }

    wsDeinitClient(&client);

    return 0;
}
