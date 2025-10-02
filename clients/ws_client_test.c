#define WS_ENABLE_LOG_DEBUG
#define WS_ENABLE_LOG_ERROR

#include "../lib/ws_client_lib.h"

int main() {

    wsClient client = {0};
    fprintf(stderr, "[TEST] Starting client initialization...\n");
    if (wsInitClient(&client, "shit.nobeggar.com", "80", "ttchef") == WS_ERROR) {
        fprintf(stderr, "Failed to init wsClient!\n");
        return -1;
    }
    fprintf(stderr, "[TEST] Client initialized successfully!\n");

    wsSendMessage(&client, "Yo wsp guys");

    wsDeinitClient(&client);

    return 0;
}
