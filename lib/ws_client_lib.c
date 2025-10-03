
#include "ws_client_lib.h"
#include "ws_defines.h"
#include "ws_globals.h"
#include "ws_json.h"
#include <asm-generic/errno.h>
#include <netdb.h>

int32_t wsInitClient(wsClient* client, const char* ip, const char* port, const char* username) {

    struct addrinfo hints = {0};
    struct addrinfo* result = NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Convert URL to ip
    WS_LOG_DEBUG("[WS CLIENT] Attempting to resolve %s:%s\n", ip, port);
    if (getaddrinfo(ip, port, &hints, &result) != 0) {
        WS_LOG_ERROR("[WS CLIENT] Failed to convert URL to valid IP address %s!\n", ip);
        return WS_ERROR;
    }
    WS_LOG_DEBUG("[WS CLIENT] Successfully resolved %s:%s\n", ip, port);

    // Create socket
    int32_t sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sockfd < 0) {
        WS_LOG_ERROR("[WS CLIENT] Failed to create socket connection to the server %s:%s!\n", ip, port);
        freeaddrinfo(result);
        return WS_ERROR;
    }

    // Set socket to non blocking (for connect timeout)
    int32_t flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    // Try to connect to server 
    int32_t connectResult = connect(sockfd, result->ai_addr, result->ai_addrlen);
    if (connectResult < 0 && errno != EINPROGRESS) {
        WS_LOG_ERROR("[WS CLIENT] Failed to connect to the server %s:%s!\n", ip, port);
        close(sockfd);
        freeaddrinfo(result);
        return WS_ERROR;
    }

    // Wait for connection (with timeout)
    if (errno == EINPROGRESS) {
        struct pollfd pfd = {sockfd, POLLOUT, 0};
        int32_t pollResult = poll(&pfd, 1, 10000); // 10000 ms
        if (pollResult == 0) {
            WS_LOG_ERROR("[WS CLIENT] Connection timeout to %s:%s\n", ip, port);
            close(sockfd);
            freeaddrinfo(result);
            return WS_ERROR;
        }

        if (pollResult < 0) {
            WS_LOG_ERROR("[WS CLIENT] Poll Failed!\n");
            close(sockfd);
            freeaddrinfo(result);
            return WS_ERROR;
        }

        // Connected check for errors
        int32_t error;
        socklen_t len = sizeof(error);
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            WS_LOG_ERROR("[WS CLIENT] Failed to connect to %s:%s\n", ip, port);
            close(sockfd);
            freeaddrinfo(result);
            return WS_ERROR;
        }
    }
    
    // Set socket back to blocking mode 
    fcntl(sockfd, F_SETFL, flags);
    freeaddrinfo(result);

    WS_LOG_DEBUG("Connected to server at %s:%s\n", ip, port);

    // websocket handshake 
    if (__ws_client_handshake(sockfd, ip) == WS_ERROR) {
        WS_LOG_ERROR("Websocket handshake failed\n");
        close(sockfd);
        return WS_ERROR;
    }

    WS_LOG_DEBUG("WebSocket handshake complete\n");
    
    client->id = sockfd;
    client->ip = ip;
    client->port = port;
    client->fds[0] = (struct pollfd){0, POLLIN, 0};
    client->fds[1] = (struct pollfd){sockfd, POLLIN, 0};

    if (!username) client->username = "Anonym\0";
    else client->username = username;

    wsJson* root = wsJsonInitChild(NULL);

    wsJson* user = wsJsonInitChild("user");
    wsJsonAddField(user, wsJsonInitString("name", client->username));
    wsJsonAddField(root, user);
    
    wsJson* message = wsJsonInitChild("message");
    wsJsonAddField(message, wsJsonInitString("text", "null"));
    wsJsonAddField(message, wsJsonInitNumber("info", WS_NO_BROADCAST | WS_CHANGE_USERNAME));
    wsJsonAddField(root, message);

    //wsSendJson(client, root);
    
    char buffer[WS_BUFFER_SIZE];
    wsJsonToString(root, buffer, WS_BUFFER_SIZE);
    printf("Message: %s\n", buffer);
    wsSendMessage(client, buffer);

    wsJsonFree(root);

    return WS_OK;
}

int32_t wsSendMessage(wsClient* client, const char *message) {
    size_t len = strlen(message);
    uint8_t frame[WS_BUFFER_SIZE];
    int32_t frameLen = __ws_encode_frame(message, len, frame);
    send(client->id, frame, frameLen, 0);

    return WS_OK;
}

int32_t wsSendMessageN(wsClient *client, const char *message, size_t n) {
    char tmp[WS_BUFFER_SIZE];
    if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
    strncpy(tmp, message, n);
    tmp[n] = '\0';

    size_t len = strlen(tmp);

    uint8_t frame[WS_BUFFER_SIZE];
    int32_t frameLen = __ws_encode_frame(tmp, len, frame);
    send(client->id, frame, frameLen, 0);

    return WS_OK;
}

int32_t wsSendJson(wsClient *client, wsJson *obj) {
    if (!client || !obj) {
        WS_LOG_ERROR("Invalid Input parameters are NULL\n");
        return WS_ERROR;
    }

    char buffer[WS_BUFFER_SIZE];
    wsJsonToString(obj, buffer, WS_BUFFER_SIZE);
    wsSendMessage(client, buffer);

    return WS_OK;
}

int32_t wsSetMessageCallback(wsClient* client, wsMessageCallbackPFN functionPtr) {
    if (!functionPtr) {
        WS_LOG_ERROR("Input function pointer is NULL\n");
        return WS_ERROR;
    }

    client->messageFunc = functionPtr;    
    
    return WS_OK;
}

int32_t wsClientListen(wsClient *client) {
    int32_t pollResult = poll(client->fds, 2, 50000);
    if (pollResult < 0) {
        WS_LOG_ERROR("Poll Error\n");
        return WS_ERROR;
    }
    if (pollResult == 0) {
        return WS_OK;
    }

    // Socket has data
    if (client->fds[1].revents & POLLIN) {
        uint8_t buffer[WS_BUFFER_SIZE];
        int32_t len = recv(client->id, buffer, WS_BUFFER_SIZE, 0);
        if (len == 0) {
            WS_LOG_DEBUG("Server disconnected\n");
            return WS_OK;
        }
        if (len > 0) {
            char msg[WS_BUFFER_SIZE];
            int32_t msgLen = __ws_decode_frame(buffer, len, msg);
            const char* colon = strchr(msg, ':');
            if (colon && msgLen > 0) {
                size_t i = colon - msg;
                char buffer[64];
                strncpy(buffer, msg, i);
                buffer[i] = '\0';
                client->messageFunc(client, msg, buffer, time(NULL));
            }
            else {
                WS_LOG_ERROR("Couldnt find username or had problems decoding the message\n");
                return WS_ERROR;
            }
        }
    } 

    return WS_OK;
}

int32_t wsDeinitClient(wsClient* client) {
    close(client->id);
    return WS_OK;
}



