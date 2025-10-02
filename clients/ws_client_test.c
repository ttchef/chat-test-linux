#define WS_ENABLE_LOG_DEBUG
#define WS_ENABLE_LOG_ERROR

#include "../lib/ws_client_lib.h"

void messageCallback(const char* message, const char* username, time_t time) {
    printf("%s", message);
}

int main() {

    wsClient client = {0};
    fprintf(stderr, "[TEST] Starting client initialization...\n");
    if (wsInitClient(&client, "shit.nobeggar.com", "80", "ttchef") == WS_ERROR) {
        fprintf(stderr, "Failed to init wsClient!\n");
        return -1;
    }
    
    wsSetMessageCallback(&client, messageCallback);

    fprintf(stderr, "[TEST] Client initialized successfully!\n");

    wsSendMessage(&client, "Yo wsp guys");

    while (1) {
        
        printf("yoo\n");
        wsClientListen(&client);
    }

    wsDeinitClient(&client);

    return 0;
}
