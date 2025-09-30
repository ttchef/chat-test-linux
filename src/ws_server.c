#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include "sha1.h"

#define MAX_CLIENTS 10
#define BUFFER_SIZE 4096

// WebSocket handshake validation
int ws_handshake(unsigned char *buffer, int len) {
    printf("Received handshake:\n%s\n", buffer);
    fflush(stdout);

    // Validate it's a WebSocket upgrade request (case insensitive)
    if (strcasestr((char*)buffer, "upgrade: websocket") &&
        strcasestr((char*)buffer, "sec-websocket-key:")) {
        return 0;
    }

    printf("Invalid WebSocket handshake\n");
    fflush(stdout);
    return -1;
}

// Decode WebSocket frame
int ws_decode_frame(unsigned char *data, int len, char *payload) {
    if (len < 2) return -1;

    int opcode = data[0] & 0x0F;
    if (opcode == 0x8) return -2; // Close frame

    int masked = data[1] & 0x80;
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

    if (masked) {
        unsigned char mask[4];
        memcpy(mask, &data[pos], 4);
        pos += 4;

        for (uint64_t i = 0; i < payload_len; i++) {
            payload[i] = data[pos + i] ^ mask[i % 4];
        }
    } else {
        memcpy(payload, &data[pos], payload_len);
    }

    payload[payload_len] = '\0';
    return payload_len;
}

// Encode WebSocket frame
int ws_encode_frame(const char *payload, int len, unsigned char *frame) {
    frame[0] = 0x81; // FIN + text frame

    if (len <= 125) {
        frame[1] = len;
        memcpy(&frame[2], payload, len);
        return len + 2;
    } else if (len <= 65535) {
        frame[1] = 126;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        memcpy(&frame[4], payload, len);
        return len + 4;
    }
    return -1;
}

int main(int argc, char *argv[]) {
    char *test_msg = NULL;

    // Parse args
    if (argc > 2 && strcmp(argv[1], "-m") == 0) {
        test_msg = argv[2];
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(9999),
        .sin_addr.s_addr = INADDR_ANY
    };

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    printf("WebSocket server listening on 0.0.0.0:9999\n");
    fflush(stdout);

    struct pollfd fds[MAX_CLIENTS + 1];
    int handshake_done[MAX_CLIENTS] = {0};
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    int nfds = 1;

    for (;;) {
        poll(fds, nfds, -1);

        // Accept new connections
        if (fds[0].revents & POLLIN) {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd >= 0 && nfds < MAX_CLIENTS + 1) {
                fds[nfds].fd = client_fd;
                fds[nfds].events = POLLIN;
                handshake_done[nfds - 1] = 0;
                nfds++;
                printf("Client connected (fd=%d)\n", client_fd);
                fflush(stdout);
            }
        }

        // Handle client messages
        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                unsigned char buffer[BUFFER_SIZE];
                int len = recv(fds[i].fd, buffer, BUFFER_SIZE, 0);

                if (len <= 0) {
                    printf("Client disconnected (fd=%d)\n", fds[i].fd);
                    close(fds[i].fd);
                    for (int j = i; j < nfds - 1; j++) {
                        fds[j] = fds[j + 1];
                        handshake_done[j - 1] = handshake_done[j];
                    }
                    nfds--;
                    i--;
                    continue;
                }

                if (!handshake_done[i - 1]) {
                    char response[BUFFER_SIZE];
                    if (ws_handshake(buffer, len) == 0) {
                        handshake_done[i - 1] = 1;
                        printf("WebSocket handshake complete (fd=%d)\n", fds[i].fd);
                        fflush(stdout);

                        // Send the handshake response
                        char key[256] = {0};
                        char *key_line = strcasestr((char*)buffer, "sec-websocket-key:");
                        if (key_line) {
                            // Skip "Sec-WebSocket-Key:" (or any case variation)
                            char *key_start = strchr(key_line, ':');
                            if (key_start) {
                                sscanf(key_start + 1, " %s", key);
                            }
                            char accept_key[512];
                            snprintf(accept_key, sizeof(accept_key), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
                            unsigned char hash[20];
                            sha1((unsigned char*)accept_key, strlen(accept_key), hash);
                            char b64[256];
                            base64_encode(hash, 20, b64);
                            snprintf(response, sizeof(response),
                                "HTTP/1.1 101 Switching Protocols\r\n"
                                "Upgrade: websocket\r\n"
                                "Connection: Upgrade\r\n"
                                "Sec-WebSocket-Accept: %s\r\n\r\n", b64);
                            printf("Sending handshake response with key: %s\n", b64);
                            fflush(stdout);
                            int sent = send(fds[i].fd, response, strlen(response), 0);
                            printf("Sent %d bytes\n", sent);
                            fflush(stdout);
                        }
                    }
                } else {
                    char payload[BUFFER_SIZE];
                    int payload_len = ws_decode_frame(buffer, len, payload);

                    if (payload_len == -2) {
                        close(fds[i].fd);
                        continue;
                    }

                    if (payload_len > 0) {
                        printf("Received: %s", payload);
                        fflush(stdout);

                        // Broadcast to all other clients
                        unsigned char frame[BUFFER_SIZE];
                        int frame_len = ws_encode_frame(payload, payload_len, frame);

                        for (int j = 1; j < nfds; j++) {
                            if (j != i && handshake_done[j - 1]) {
                                send(fds[j].fd, frame, frame_len, 0);
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}