#define WS_ENABLE_LOG_DEBUG
#define WS_ENABLE_LOG_ERROR

#include "../lib/ws_client_lib.h"

void messageCallback(wsClient* client, const char* message, const char* username, time_t time) {
    printf("%s", message);
}

int main() {

    // JSON test 
    wsJson* root = wsJsonInitChild(NULL);
    wsJson* user = wsJsonInitChild("user");
    
    wsJsonAddField(user, wsJsonInitString("name", "ttchef"));
    wsJsonAddField(user, wsJsonInitNumber("age", 23));
    wsJsonAddField(root, user);
    wsJsonAddField(root, wsJsonInitString("msg", "Yo wsp"));
    
    char out[1024] = {0};
    wsJsonToString(root, out, sizeof(out));
    printf("%s\n", out);

    // Get Componennts
    wsJson* userObj = wsJsonGet(root, "user");
    printf("name: %s\n", wsJsonGetString(userObj, "name"));
    printf("age: %.0f\n", wsJsonGetNumber(userObj, "age"));

    wsJsonFree(root);

    // Test converting to json
    const char* cp = out;
    wsJson* conv = wsStringToJson(&cp);
    wsJsonToString(conv, out, sizeof(out));
    printf("%s\n", out);
    wsJsonFree(conv);

    wsClient client = {0};
    fprintf(stderr, "[TEST] Starting client initialization...\n");
    if (wsInitClient(&client, "127.0.0.1", "9999", "ttchef") == WS_ERROR) {
        fprintf(stderr, "Failed to init wsClient!\n");
        return -1;
    }
    
    wsSetMessageCallback(&client, messageCallback);

    fprintf(stderr, "[TEST] Client initialized successfully!\n");

    while (1) {
        
        wsClientListen(&client);
    }

    wsDeinitClient(&client);

    return 0;
}
