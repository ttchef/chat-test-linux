#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include "sha1.h"

#define MAX_CLIENTS 10
#define BUFFER_SIZE 4096

typedef struct {
    int id;
    char name[64];
} Client;

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
    // Allocate Clients
    Client clients[MAX_CLIENTS];
    int clients_index = 0;

    char *test_msg = NULL;
    char *host = "0.0.0.0";  // Default to all interfaces

    // Parse args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            test_msg = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            host = argv[i + 1];
            i++;
        }
    }

    // Resolve host address
    struct addrinfo hints = {0};
    struct addrinfo *result;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(host, "9999", &hints, &result) != 0) {
        fprintf(stderr, "Failed to resolve host: %s\n", host);
        return 1;
    }

    int server_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (server_fd < 0) {
        perror("socket failed");
        freeaddrinfo(result);
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, result->ai_addr, result->ai_addrlen) < 0) {
        perror("bind failed");
        close(server_fd);
        freeaddrinfo(result);
        return 1;
    }

    freeaddrinfo(result);
    listen(server_fd, 10);

    printf("WebSocket server listening on %s:9999\n", host);
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
                // Enable TCP keepalive to prevent idle timeout
                int keepalive = 1;
                int keepidle = 60;    // Start probes after 60s idle
                int keepintvl = 10;   // Probe every 10s
                int keepcnt = 6;      // Drop after 6 failed probes
                setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
                setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
                setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
                setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));

                // Init Client
                clients[clients_index].id = client_fd;
                char name[] = "Anonym\0";
                memcpy(clients[clients_index].name, name, sizeof(name));
                clients_index++;

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

                        // very safe btw
                        // is a id verification
                        if (strncmp(payload, "[ID]", 4) == 0) {
                            for (int k = 0; k < clients_index; k++) {
                                if (clients[k].id == fds[i].fd) {
                                    snprintf(clients[k].name, sizeof(clients[k].name), "%s", payload + 5);
                                }
                            }
                        }

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
