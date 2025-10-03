
#ifndef WS_DEFINES_H
#define WS_DEFINES_H

#include "ws_globals.h"
#include "ws_json.h"

typedef enum {  
    WS_NO_BROADCAST = (1 << 0),
    WS_SEND_BACK = (1 << 1),
    WS_CHANGE_USERNAME = (1 << 2),
} wsMessageInfo;

typedef enum {
    WS_MESSAGE_CALLBACK_JSON,
    WS_MESSAGE_CALLBACK_RAW,
} wsOnMessageCallbackType;

typedef struct wsClient wsClient;

typedef void (*wsOnMessageCallbackRawPFN)(wsClient* client, time_t time, const char* message);
typedef void (*wsOnMessageCallbackJsonPFN)(wsClient* client, time_t time, wsJson* root);

typedef union {
    wsOnMessageCallbackJsonPFN json;
    wsOnMessageCallbackRawPFN raw;
} wsOnMessageCallbackPFN;

struct wsClient {
    int32_t id;
    const char* ip;
    const char* port;
    struct pollfd fds[2];
    const char* username;
    wsOnMessageCallbackType onMessageCallbackType;
    wsOnMessageCallbackPFN onMessageCallback;
};

// Internal
static inline int32_t __ws_encode_frame(const char* payload, int32_t len, uint8_t* frame) {
    frame[0] = 0x81;

    unsigned char mask[4];
    srand(time(NULL));
    for (int i = 0; i < 4; i++) {
        mask[i] = rand() % 256;  // Random byte (0-255)
    }

    if (len <= 125) {
        frame[1] = 0x80 | len;
        memcpy(&frame[2], mask, 4);
        for (int i = 0; i < len; i++) {
            frame[6 + i] = payload[i] ^ mask[i % 4];
        }
        return len + 6;
    }
    else if (len <= 65535) {
        frame[1] = 0x80 | 126;
        frame[2] = (len >> 8) & 0xFF;  // High byte
        frame[3] = len & 0xFF;          // Low byte
        memcpy(&frame[4], mask, 4);
        for (int i = 0; i < len; i++) {
            frame[8 + i] = payload[i] ^ mask[i % 4];
        }
        return len + 8;
    }
    return WS_ERROR;
}

static inline int32_t __ws_decode_frame(uint8_t* data, int32_t len, char* payload) {
    if (len < 2) return WS_ERROR;

    int opcode = data[0] & 0x0F;
    if (opcode == 0x8) return WS_ERROR; // Close

    int masked = (data[1] & 0x80) != 0;
    uint64_t payload_len = data[1] & 0x7F;
    int pos = 2;

    if (payload_len == 126) {
        payload_len = (data[2] << 8) | data[3];
        pos = 4;
    }
    else if (payload_len == 127) {
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | data[2 + i];
        }
        pos = 10;
    }

    uint8_t mask[4];
    if (masked) {
        memcpy(mask, &data[pos], 4);
        pos += 4;
    }

    for (uint64_t i = 0; i < payload_len; i++) {
        if (masked)
            payload[i] = data[pos + i] ^ mask[i % 4];
        else
            payload[i] = data[pos + i];
    }
    payload[payload_len] = '\0';
    return payload_len;}

static inline int32_t __ws_client_handshake(int32_t sockfd, const char* ip) {
    char request[WS_BUFFER_SIZE];
    char key[] = "dGhlIHNhbXBsZSBub25jZQ==";
    snprintf(request, sizeof(request),
        "GET / HTTP/1.1\r\n"                     
        "Host: %s\r\n"                           
        "Upgrade: websocket\r\n"                
        "Connection: Upgrade\r\n"                
        "Sec-WebSocket-Key: %s\r\n"              
        "Sec-WebSocket-Version: 13\r\n\r\n",     
        ip, key);

    send(sockfd, request, strlen(request), 0);

    char buffer[WS_BUFFER_SIZE];
    int n = recv(sockfd, buffer, WS_BUFFER_SIZE - 1, 0);
    if (n <= 0) {
        fprintf(stderr, "No response from server\n");
        return WS_ERROR;
    }
    buffer[n] = '\0';

    fprintf(stderr, "Server response:\n%s\n", buffer);

    if (strstr(buffer, "101 Switching Protocols")) {
        return WS_OK;
    }
    return WS_ERROR;
}

static inline int __ws_server_handshake(unsigned char *buffer, int len) {
    printf("Received handshake:\n%s\n", buffer);
    fflush(stdout);

    if (strcasestr((char*)buffer, "upgrade: websocket") &&
        strcasestr((char*)buffer, "sec-websocket-key:")) {
        return 0;
    }

    printf("Invalid WebSocket handshake\n");
    fflush(stdout);
    return -1;
}

#endif

