
#include "ws_client_lib.h"

#define WS_ENABLE_LOG_DEBUG
#define WS_ENABLE_LOG_ERROR

int main() {

    wsClient client = {0};
    if (wsInitClient(&client, "shit.nobeggar.com", "80", "ttchef") == WS_ERROR) {
        fprintf(stderr, "Failed to init wsClient!\n");
        return -1;
    }

    wsSendMessage(&client, "Yo wsp guys");

    wsDeinitClient(&client);

    return 0;
}
