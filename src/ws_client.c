#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <time.h>

#include "ip.h"

#define BUFFER_SIZE 4096

// Encode WebSocket frame (with masking for client)
int ws_encode_frame(const char *payload, int len, unsigned char *frame) {
    frame[0] = 0x81; // FIN + text frame

    // Generate random mask
    unsigned char mask[4];
    srand(time(NULL));
    for (int i = 0; i < 4; i++) {
        mask[i] = rand() % 256;
    }

    if (len <= 125) {
        frame[1] = 0x80 | len; // Masked + length
        memcpy(&frame[2], mask, 4);
        for (int i = 0; i < len; i++) {
            frame[6 + i] = payload[i] ^ mask[i % 4];
        }
        return len + 6;
    } else if (len <= 65535) {
        frame[1] = 0x80 | 126;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        memcpy(&frame[4], mask, 4);
        for (int i = 0; i < len; i++) {
            frame[8 + i] = payload[i] ^ mask[i % 4];
        }
        return len + 8;
    }
    return -1;
}

// Decode WebSocket frame
int ws_decode_frame(unsigned char *data, int len, char *payload) {
    if (len < 2) return -1;

    int opcode = data[0] & 0x0F;
    if (opcode == 0x8) return -2; // Close frame

    uint64_t payload_len = data[1] & 0x7F;
    int pos = 2;

    if (payload_len == 126) {
        payload_len = (data[2] << 8) | data[3];
        pos = 4;
    } else if (payload_len == 127) {
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | data[2 + i];
        }
        pos = 10;
    }

    memcpy(payload, &data[pos], payload_len);
    payload[payload_len] = '\0';
    return payload_len;
}

// WebSocket client handshake
int ws_client_handshake(int sockfd, const char *host) {
    char request[BUFFER_SIZE];
    char key[] = "dGhlIHNhbXBsZSBub25jZQ=="; // Sample key

    snprintf(request, sizeof(request),
        "GET / HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n", host, key);

    send(sockfd, request, strlen(request), 0);

    // Read response
    char buffer[BUFFER_SIZE];
    int n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
    if (n <= 0) {
        fprintf(stderr, "No response from server\n");
        return -1;
    }
    buffer[n] = '\0';

    // Debug: print response
    fprintf(stderr, "Server response:\n%s\n", buffer);

    if (strstr(buffer, "101 Switching Protocols")) {
        return 0;
    }
    return -1;
}

int main(int argc, char *argv[]) {
    char *test_msg = NULL;
    int headless = 0;

    // Parse args
    if (argc > 2 && strcmp(argv[1], "-m") == 0) {
        test_msg = argv[2];
        headless = 1;
    }

    struct addrinfo hints = {0};
    struct addrinfo *result;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Use port 80 for Cloudflare tunnel HTTP
    if (getaddrinfo(IP, "80", &hints, &result) != 0) {
        perror("getaddrinfo failed");
        return 1;
    }

    int sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sockfd < 0) {
        perror("socket failed");
        freeaddrinfo(result);
        return 1;
    }

    if (connect(sockfd, result->ai_addr, result->ai_addrlen) < 0) {
        perror("connect failed");
        close(sockfd);
        freeaddrinfo(result);
        return 1;
    }

    freeaddrinfo(result);

    printf("Connected to server at %s:80\n", IP);
    fflush(stdout);

    // Perform WebSocket handshake
    if (ws_client_handshake(sockfd, IP) < 0) {
        fprintf(stderr, "WebSocket handshake failed\n");
        close(sockfd);
        return 1;
    }

    printf("WebSocket handshake complete\n");
    fflush(stdout);

    // Send test message if in headless mode
    if (headless && test_msg) {
        unsigned char frame[BUFFER_SIZE];
        int frame_len = ws_encode_frame(test_msg, strlen(test_msg), frame);
        send(sockfd, frame, frame_len, 0);
        printf("Sent: %s", test_msg);
        fflush(stdout);
    }

    struct pollfd fds[2] = {
        {0, POLLIN, 0},
        {sockfd, POLLIN, 0}
    };

    int received_response = 0;
    for (;;) {
        int timeout = headless ? 5000 : 50000;
        int ret = poll(fds, 2, timeout);

        if (ret == 0 && headless && received_response) {
            printf("Test complete\n");
            break;
        }

        if (fds[0].revents & POLLIN) {
            char buffer[256];
            int len = read(0, buffer, 255);
            if (len > 0) {
                unsigned char frame[BUFFER_SIZE];
                int frame_len = ws_encode_frame(buffer, len, frame);
                send(sockfd, frame, frame_len, 0);
            }
        }

        if (fds[1].revents & POLLIN) {
            unsigned char buffer[BUFFER_SIZE];
            int len = recv(sockfd, buffer, BUFFER_SIZE, 0);
            if (len == 0) {
                printf("Server disconnected\n");
                return 0;
            }
            if (len > 0) {
                char payload[BUFFER_SIZE];
                int payload_len = ws_decode_frame(buffer, len, payload);
                if (payload_len > 0) {
                    printf("Received: %s", payload);
                    fflush(stdout);
                    received_response = 1;
                }
            }
        }
    }

    close(sockfd);
    return 0;
}
