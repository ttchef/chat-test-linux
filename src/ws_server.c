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
#include <signal.h>     
#include "sha1.h"       

#include "../lib/ws_defines.h"

#define MAX_CLIENTS 10

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);

    wsClient clients[MAX_CLIENTS] = {0};
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].id = -1;
    }

    char *host = "0.0.0.0";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            host = argv[i + 1];
            i++;
        }
    }

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

        if (fds[0].revents & POLLIN) {
            int client_fd = accept(server_fd, NULL, NULL);

            if (client_fd >= 0 && nfds < MAX_CLIENTS + 1) {
                int client_slot = -1;
                for (int k = 0; k < MAX_CLIENTS; k++) {
                    if (clients[k].id == -1) {
                        client_slot = k;
                        break;
                    }
                }

                if (client_slot == -1) {
                    printf("Max clients reached, rejecting connection\n");
                    fflush(stdout);
                    close(client_fd);
                    continue;
                }

                int keepalive = 1;   
                int keepidle = 60;   
                int keepintvl = 10; 
                int keepcnt = 6;    

                setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
                setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
                setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
                setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));

                clients[client_slot].id = client_fd; 
                clients[client_slot].username = "Anonym\0";

                fds[nfds].fd = client_fd;    
                fds[nfds].events = POLLIN;  
                handshake_done[nfds - 1] = 0;
                nfds++;

                printf("Client connected (fd=%d, slot=%d)\n", client_fd, client_slot);
                fflush(stdout);
            }
        }

        // Check all client sockets for activity
        for (int i = 1; i < nfds; i++) {
            // Check if this client socket has incoming data
            if (fds[i].revents & POLLIN) {
                unsigned char buffer[WS_BUFFER_SIZE];
                int len = recv(fds[i].fd, buffer, WS_BUFFER_SIZE - 1, 0);

                if (len <= 0) {
                    printf("Client disconnected (fd=%d)\n", fds[i].fd);

                    for (int k = 0; k < MAX_CLIENTS; k++) {
                        if (clients[k].id == fds[i].fd) {
                            clients[k].id = -1;
                            break;
                        }
                    }

                    close(fds[i].fd);

                    // Remove this client from the poll array by shifting remaining entries
                    for (int j = i; j < nfds - 1; j++) {
                        fds[j] = fds[j + 1];  
                        handshake_done[j - 1] = handshake_done[j];
                    }
                    nfds--;
                    i--;
                    continue;
                }

                buffer[len] = '\0';

                // Check if this client has completed the WebSocket handshake
                if (!handshake_done[i - 1]) {
                    char response[WS_BUFFER_SIZE];

                    if (__ws_server_handshake(buffer, len) == 0) {
                        handshake_done[i - 1] = 1;
                        printf("WebSocket handshake complete (fd=%d)\n", fds[i].fd);
                        fflush(stdout);

                        char key[256] = {0};  // Buffer for the key
                        char *key_line = strcasestr((char*)buffer, "sec-websocket-key:");
                        if (key_line) {
                            char *key_start = strchr(key_line, ':');
                            if (key_start) {
                                sscanf(key_start + 1, " %[^\r\n]", key);
                                char *end = key + strlen(key) - 1;
                                while (end > key && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
                                    *end = '\0';
                                    end--;
                                }
                                printf("Extracted WebSocket key: '%s' (len=%zu)\n", key, strlen(key));
                                fflush(stdout);
                            }

                            char accept_key[512];
                            snprintf(accept_key, sizeof(accept_key), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);

                            unsigned char hash[20];  // SHA1 produces 20 bytes
                            sha1((unsigned char*)accept_key, strlen(accept_key), hash);

                            char b64[256];
                            base64_encode(hash, 20, b64);

                            // Build the HTTP response for successful WebSocket upgrade
                            snprintf(response, sizeof(response),
                                "HTTP/1.1 101 Switching Protocols\r\n"  // Status code 101
                                "Upgrade: websocket\r\n"                 // Upgrade header
                                "Connection: Upgrade\r\n"               // Connection header
                                "Sec-WebSocket-Accept: %s\r\n\r\n", b64); // Accept key

                            printf("Sending handshake response with key: %s\n", b64);
                            fflush(stdout);

                            // Send the handshake response to the client
                            int sent = send(fds[i].fd, response, strlen(response), 0);
                            if (sent < 0) {
                                printf("Failed to send handshake response (fd=%d)\n", fds[i].fd);
                                fflush(stdout);
                                close(fds[i].fd);
                                // Mark client slot as free
                                for (int k = 0; k < MAX_CLIENTS; k++) {
                                    if (clients[k].id == fds[i].fd) {
                                        clients[k].id = -1;
                                        break;
                                    }
                                }
                                continue;
                            }

                            // Debug: print number of bytes sent
                            printf("Sent %d bytes\n", sent);
                            fflush(stdout);
                        }
                    }
                } else {
                    // Client has completed handshake, this is a WebSocket frame

                    // Buffer for the decoded payload
                    char payload[WS_BUFFER_SIZE];
                    int payload_len = __ws_decode_frame(buffer, len, payload);

                    // Close connection
                    if (payload_len == -2) {
                        close(fds[i].fd);
                        continue;
                    }

                    const char* cp = payload;
                    printf("Message: %s\n", payload);
                    wsJson* root = wsStringToJson(&cp);

                    // If we successfully decoded a payload
                    if (payload_len > 0) {
                        wsJson* message = wsJsonGet(root, "message");
                        double info = wsJsonGetNumber(message, "info");
                        printf("Info: %d\n", info);

                        // Broadcast the message to all other connected clients
                        unsigned char frame[WS_BUFFER_SIZE];
                        int frame_len = __ws_encode_frame(payload, payload_len, frame);

                        // Send to all clients except the sender
                        for (int j = 1; j < nfds; j++) {
                            // Skip if this is the sender or if client hasn't completed handshake
                            if (j != i && handshake_done[j - 1]) {
                                // Send the encoded frame, check for errors
                                int sent = send(fds[j].fd, frame, frame_len, 0);
                                if (sent < 0) {
                                    printf("Failed to send to client (fd=%d), will be disconnected\n", fds[j].fd);
                                    fflush(stdout);
                                }
                            }
                        }
                    }
                    wsJsonFree(root);
                }
            }
        }
    }

    // This line is never reached (infinite loop above)
    return 0;
}

