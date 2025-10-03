#define WS_ENABLE_LOG_DEBUG
#define WS_ENABLE_LOG_ERROR

#include "../lib/ws_client_lib.h"

void messageCallback(wsClient* client, const char* message, const char* username, time_t time) {
    printf("%s", message);
}

int main() {

    // JSON test 
    wsJson* root = wsJsonInitObject(NULL);
    wsJson* user = wsJsonInitObject("user");
    
    wsJsonAddChild(user, wsJsonInitString("name", "ttchef"));
    wsJsonAddChild(user, wsJsonInitNumber("age", 23));
    wsJsonAddChild(root, user);
    wsJsonAddChild(root, wsJsonInitString("msg", "Yo wsp"));
    
    char out[1024] = {0};
    wsJsonToString(root, out, sizeof(out));
    printf("%s\n", out);

    wsJsonFree(root);


    wsClient client = {0};
    fprintf(stderr, "[TEST] Starting client initialization...\n");
    if (wsInitClient(&client, "127.0.0.1", "9999", "ttchef") == WS_ERROR) {
        fprintf(stderr, "Failed to init wsClient!\n");
        return -1;
    }
    
    wsSetMessageCallback(&client, messageCallback);

    fprintf(stderr, "[TEST] Client initialized successfully!\n");

    wsSendMessage(&client, "Yo wsp guys");

    while (1) {
        
        wsClientListen(&client);
    }

    wsDeinitClient(&client);

    return 0;
}
