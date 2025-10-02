// Enable GNU extensions for strcasestr function
#define _GNU_SOURCE
#include <stdio.h>      // Standard I/O: printf, fprintf, perror, etc.
#include <stdlib.h>     // Standard library: exit, malloc, etc.
#include <string.h>     // String operations: strcmp, strcpy, memcpy, etc.
#include <unistd.h>     // UNIX standard: close, read, write
#include <sys/socket.h> // Socket API: socket, bind, listen, accept, send, recv
#include <netinet/in.h> // Internet protocol: sockaddr_in, INADDR_ANY
#include <netinet/tcp.h>// TCP protocol options: TCP_KEEPIDLE, TCP_KEEPINTVL
#include <arpa/inet.h>  // Internet operations: inet_addr, htons
#include <netdb.h>      // Network database operations: getaddrinfo, freeaddrinfo
#include <poll.h>       // Poll system call for multiplexing I/O
#include "sha1.h"       // SHA1 and Base64 encoding for WebSocket handshake

// Maximum number of simultaneous client connections the server can handle
#define MAX_CLIENTS 10
// Size of buffers used for receiving and sending data
#define BUFFER_SIZE 4096

// Structure to store client information
typedef struct {
    int id;         // File descriptor (socket) for this client connection
    char name[64];  // Username/display name for this client
} Client;

/**
 * Validates whether received data is a valid WebSocket upgrade request
 * @param buffer - Pointer to the received HTTP request data
 * @param len - Length of the received data in bytes
 * @return 0 if valid WebSocket handshake, -1 otherwise
 */
int ws_handshake(unsigned char *buffer, int len) {
    // Print the received handshake for debugging purposes
    printf("Received handshake:\n%s\n", buffer);
    // Ensure output is immediately written (not buffered)
    fflush(stdout);

    // Check if the request contains required WebSocket headers (case-insensitive)
    // Must have "Upgrade: websocket" header
    // Must have "Sec-WebSocket-Key:" header for key exchange
    if (strcasestr((char*)buffer, "upgrade: websocket") &&
        strcasestr((char*)buffer, "sec-websocket-key:")) {
        // Valid WebSocket handshake detected
        return 0;
    }

    // If validation fails, print error message
    printf("Invalid WebSocket handshake\n");
    fflush(stdout);
    // Return error code
    return -1;
}

/**
 * Decodes a WebSocket frame to extract the payload data
 * WebSocket frames have a specific format:
 * - Byte 0: FIN bit (1 bit) + RSV (3 bits) + Opcode (4 bits)
 * - Byte 1: MASK bit (1 bit) + Payload length (7 bits)
 * - Optional: Extended payload length (2 or 8 bytes)
 * - Optional: Masking key (4 bytes if MASK bit is set)
 * - Payload data (masked if MASK bit is set)
 *
 * @param data - Pointer to the raw WebSocket frame data
 * @param len - Length of the frame data in bytes
 * @param payload - Output buffer to store the decoded payload
 * @return Length of decoded payload, -1 on error, -2 if close frame
 */
int ws_decode_frame(unsigned char *data, int len, char *payload) {
    // Minimum frame size is 2 bytes (opcode + mask/length)
    if (len < 2) return -1;

    // Extract opcode from first byte (lower 4 bits)
    // Opcode determines frame type: 0x1=text, 0x2=binary, 0x8=close, etc.
    int opcode = data[0] & 0x0F;
    // Check if this is a close frame (opcode 0x8)
    if (opcode == 0x8) return -2; // Signal connection should close

    // Check if payload is masked (bit 7 of second byte)
    int masked = data[1] & 0x80;
    // Extract payload length from second byte (lower 7 bits)
    uint64_t payload_len = data[1] & 0x7F;
    // Position in frame data (starts at byte 2 after basic header)
    int pos = 2;

    // If payload_len is 126, next 2 bytes contain the actual 16-bit length
    if (payload_len == 126) {
        // Combine two bytes into 16-bit length (network byte order: big-endian)
        payload_len = (data[2] << 8) | data[3];
        // Move position past the extended length field
        pos = 4;
    }
    // If payload_len is 127, next 8 bytes contain the actual 64-bit length
    else if (payload_len == 127) {
        // Initialize length to zero
        payload_len = 0;
        // Build 64-bit length from 8 bytes (network byte order)
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | data[2 + i];
        }
        // Move position past the extended length field
        pos = 10;
    }

    // If payload is masked, we need to unmask it using the 4-byte masking key
    if (masked) {
        // Extract the 4-byte masking key
        unsigned char mask[4];
        memcpy(mask, &data[pos], 4);
        // Move position past the masking key
        pos += 4;

        // Unmask each byte of the payload by XORing with the masking key
        // The masking key cycles every 4 bytes (i % 4)
        for (uint64_t i = 0; i < payload_len; i++) {
            payload[i] = data[pos + i] ^ mask[i % 4];
        }
    } else {
        // If not masked, just copy the payload directly
        memcpy(payload, &data[pos], payload_len);
    }

    // Null-terminate the payload string
    payload[payload_len] = '\0';
    // Return the length of the decoded payload
    return payload_len;
}

/**
 * Encodes data into a WebSocket frame format for sending to clients
 * Creates an unmasked text frame (servers don't mask their frames)
 *
 * @param payload - Pointer to the data to send
 * @param len - Length of the data in bytes
 * @param frame - Output buffer to store the encoded frame
 * @return Length of the encoded frame in bytes, -1 on error
 */
int ws_encode_frame(const char *payload, int len, unsigned char *frame) {
    // Byte 0: FIN bit (1) + RSV bits (000) + Opcode (0001 for text frame)
    // 0x81 = 10000001 binary = FIN + text frame
    frame[0] = 0x81;

    // For small payloads (≤125 bytes), length fits in byte 1
    if (len <= 125) {
        // Byte 1: MASK bit (0) + 7-bit payload length
        frame[1] = len;
        // Copy payload data starting at byte 2
        memcpy(&frame[2], payload, len);
        // Total frame size: 2-byte header + payload
        return len + 2;
    }
    // For medium payloads (126-65535 bytes), use extended 16-bit length
    else if (len <= 65535) {
        // Byte 1: MASK bit (0) + length indicator (126)
        frame[1] = 126;
        // Bytes 2-3: 16-bit payload length in network byte order
        frame[2] = (len >> 8) & 0xFF;  // High byte
        frame[3] = len & 0xFF;          // Low byte
        // Copy payload data starting at byte 4
        memcpy(&frame[4], payload, len);
        // Total frame size: 4-byte header + payload
        return len + 4;
    }
    // Payloads larger than 65535 bytes are not supported
    return -1;
}

/**
 * Main server function
 * Sets up a WebSocket server that:
 * - Listens for client connections
 * - Performs WebSocket handshake with new clients
 * - Receives messages from clients and broadcasts to all other clients
 * - Handles client disconnections
 */
int main(int argc, char *argv[]) {
    // Array to store information about all connected clients
    Client clients[MAX_CLIENTS];
    // Track the number of clients that have ever connected (used as array index)
    int clients_index = 0;

    // Default host to bind to (0.0.0.0 means all network interfaces)
    char *host = "0.0.0.0";

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        // Check for -h flag (host specification)
        if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            // Next argument is the host address
            host = argv[i + 1];
            // Skip the next argument since we already processed it
            i++;
        }
    }

    // Prepare hints for getaddrinfo to resolve the host address
    struct addrinfo hints = {0};  // Zero-initialize the structure
    struct addrinfo *result;       // Will point to the resolved address info
    hints.ai_family = AF_INET;     // Use IPv4 addresses
    hints.ai_socktype = SOCK_STREAM; // Use TCP (stream sockets)
    hints.ai_flags = AI_PASSIVE;   // Socket address is for bind() (server)

    // Resolve the host address (convert hostname to IP address)
    // Port "9999" is the service we're binding to
    if (getaddrinfo(host, "9999", &hints, &result) != 0) {
        // If resolution fails, print error and exit
        fprintf(stderr, "Failed to resolve host: %s\n", host);
        return 1;
    }

    // Create a socket using the resolved address information
    // socket() returns a file descriptor for the new socket
    int server_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (server_fd < 0) {
        // If socket creation fails, print error
        perror("socket failed");
        // Free the address info structure allocated by getaddrinfo
        freeaddrinfo(result);
        return 1;
    }

    // Set socket option to allow reusing the address immediately
    // This prevents "Address already in use" errors when restarting the server
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind the socket to the resolved address and port
    if (bind(server_fd, result->ai_addr, result->ai_addrlen) < 0) {
        // If bind fails, print error
        perror("bind failed");
        // Close the socket before exiting
        close(server_fd);
        // Free the address info structure
        freeaddrinfo(result);
        return 1;
    }

    // Free the address info structure (no longer needed after bind)
    freeaddrinfo(result);

    // Mark the socket as passive (listening for incoming connections)
    // The second parameter (10) is the maximum length of the connection queue
    listen(server_fd, 10);

    // Print status message indicating server is ready
    printf("WebSocket server listening on %s:9999\n", host);
    // Ensure the message is immediately displayed
    fflush(stdout);

    // Array of pollfd structures for poll() system call
    // Index 0 is the server socket, indices 1+ are client sockets
    struct pollfd fds[MAX_CLIENTS + 1];
    // Track which clients have completed the WebSocket handshake
    // handshake_done[i] corresponds to fds[i+1] (offset by 1 because fds[0] is server)
    int handshake_done[MAX_CLIENTS] = {0};

    // Initialize the server socket in the poll array
    fds[0].fd = server_fd;      // File descriptor to monitor
    fds[0].events = POLLIN;     // Monitor for incoming data/connections
    // Track the number of file descriptors in the poll array
    int nfds = 1;  // Start with just the server socket

    // Main server loop (runs forever)
    for (;;) {
        // Wait for activity on any of the file descriptors
        // -1 timeout means wait indefinitely until something happens
        poll(fds, nfds, -1);

        // Check if the server socket has activity (new connection)
        if (fds[0].revents & POLLIN) {
            // Accept the new connection
            // Returns a new socket file descriptor for the client
            int client_fd = accept(server_fd, NULL, NULL);

            // Check if accept succeeded and we have room for more clients
            if (client_fd >= 0 && nfds < MAX_CLIENTS + 1) {
                // Enable TCP keepalive to detect dead connections
                // This prevents idle connections from timing out
                int keepalive = 1;  // Enable keepalive
                int keepidle = 60;  // Start sending keepalive probes after 60 seconds of idle time
                int keepintvl = 10; // Send keepalive probes every 10 seconds
                int keepcnt = 6;    // Drop connection after 6 failed probes (60 seconds total)

                // Set socket options for keepalive
                setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
                setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
                setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
                setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));

                // Initialize client information
                clients[clients_index].id = client_fd;  // Store the socket file descriptor
                // Set default username to "Anonym" (7 bytes + null terminator)
                char name[] = "Anonym\0";
                memcpy(clients[clients_index].name, name, sizeof(name));
                // Increment the client index for the next client
                clients_index++;

                // Add client socket to the poll array
                fds[nfds].fd = client_fd;    // File descriptor to monitor
                fds[nfds].events = POLLIN;   // Monitor for incoming data
                // Mark that this client hasn't completed handshake yet
                handshake_done[nfds - 1] = 0;
                // Increment the count of file descriptors
                nfds++;

                // Print debug message
                printf("Client connected (fd=%d)\n", client_fd);
                fflush(stdout);
            }
        }

        // Check all client sockets for activity
        // Start at index 1 (skip server socket at index 0)
        for (int i = 1; i < nfds; i++) {
            // Check if this client socket has incoming data
            if (fds[i].revents & POLLIN) {
                // Buffer to store received data
                unsigned char buffer[BUFFER_SIZE];
                // Receive data from the client socket
                int len = recv(fds[i].fd, buffer, BUFFER_SIZE, 0);

                // If recv returns 0 or negative, client has disconnected or error occurred
                if (len <= 0) {
                    // Print disconnection message
                    printf("Client disconnected (fd=%d)\n", fds[i].fd);
                    // Close the client socket
                    close(fds[i].fd);

                    // Remove this client from the poll array by shifting remaining entries
                    for (int j = i; j < nfds - 1; j++) {
                        fds[j] = fds[j + 1];  // Shift poll entries left
                        // Also shift the handshake_done array (accounting for offset)
                        handshake_done[j - 1] = handshake_done[j];
                    }
                    // Decrease the count of file descriptors
                    nfds--;
                    // Decrement i to account for the shift (recheck current position)
                    i--;
                    // Skip to next iteration
                    continue;
                }

                // Check if this client has completed the WebSocket handshake
                // handshake_done array is offset by 1 from fds array
                if (!handshake_done[i - 1]) {
                    // Buffer for the handshake response
                    char response[BUFFER_SIZE];

                    // Validate the WebSocket handshake request
                    if (ws_handshake(buffer, len) == 0) {
                        // Mark handshake as complete for this client
                        handshake_done[i - 1] = 1;
                        printf("WebSocket handshake complete (fd=%d)\n", fds[i].fd);
                        fflush(stdout);

                        // Extract the Sec-WebSocket-Key from the request
                        char key[256] = {0};  // Buffer for the key
                        // Find the line containing the key (case-insensitive search)
                        char *key_line = strcasestr((char*)buffer, "sec-websocket-key:");
                        if (key_line) {
                            // Find the colon separator
                            char *key_start = strchr(key_line, ':');
                            if (key_start) {
                                // Parse the key value (skip colon and whitespace)
                                sscanf(key_start + 1, " %s", key);
                            }

                            // Create the accept key by concatenating client key with WebSocket GUID
                            // This is part of the WebSocket protocol (RFC 6455)
                            char accept_key[512];
                            snprintf(accept_key, sizeof(accept_key), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);

                            // Compute SHA1 hash of the concatenated string
                            unsigned char hash[20];  // SHA1 produces 20 bytes
                            sha1((unsigned char*)accept_key, strlen(accept_key), hash);

                            // Encode the hash in Base64
                            char b64[256];
                            base64_encode(hash, 20, b64);

                            // Build the HTTP response for successful WebSocket upgrade
                            snprintf(response, sizeof(response),
                                "HTTP/1.1 101 Switching Protocols\r\n"  // Status code 101
                                "Upgrade: websocket\r\n"                 // Upgrade header
                                "Connection: Upgrade\r\n"               // Connection header
                                "Sec-WebSocket-Accept: %s\r\n\r\n", b64); // Accept key

                            // Debug: print the accept key
                            printf("Sending handshake response with key: %s\n", b64);
                            fflush(stdout);

                            // Send the handshake response to the client
                            int sent = send(fds[i].fd, response, strlen(response), 0);

                            // Debug: print number of bytes sent
                            printf("Sent %d bytes\n", sent);
                            fflush(stdout);
                        }
                    }
                } else {
                    // Client has completed handshake, this is a WebSocket frame

                    // Buffer for the decoded payload
                    char payload[BUFFER_SIZE];
                    // Decode the WebSocket frame
                    int payload_len = ws_decode_frame(buffer, len, payload);

                    // Check if this was a close frame
                    if (payload_len == -2) {
                        // Close the connection
                        close(fds[i].fd);
                        continue;
                    }

                    // If we successfully decoded a payload
                    if (payload_len > 0) {
                        // Print the received message
                        for (int p = 0; p < clients_index; p++) {
                            printf("%s: %s", clients[p].name, payload);
                            fflush(stdout);
                        }

                        // Check if this is a special [ID] message for setting username
                        // Format: "[ID]username" (5 chars prefix: [ID] + first char of name)
                        if (strncmp(payload, "[ID]", 4) == 0) {
                            // Find this client in the clients array
                            for (int k = 0; k < clients_index; k++) {
                                if (clients[k].id == fds[i].fd) {
                                    // Update the client's name (skip "[ID]" prefix)
                                    snprintf(clients[k].name, sizeof(clients[k].name), "%s", payload + 5);
        
                                    // Remove \n 
                                    size_t len = strlen(clients[k].name);
                                    if (len > 0 && clients[k].name[len - 1] == '\n') {
                                        clients[k].name[len - 1] = '\0';
                                    }

                                    printf("[ID] %d %s\n", clients[k].id, clients[k].name);
                                    fflush(stdout);
                                }
                            }
                        }

                        // Broadcast the message to all other connected clients
                        // First, encode the payload into a WebSocket frame
                        unsigned char frame[BUFFER_SIZE];
                        int frame_len = ws_encode_frame(payload, payload_len, frame);

                        // Send to all clients except the sender
                        for (int j = 1; j < nfds; j++) {
                            // Skip if this is the sender or if client hasn't completed handshake
                            if (j != i && handshake_done[j - 1]) {
                                
                                char name[64] = "Anonym\0";
                                // Check which client 
                                for (int n = 0; n < clients_index; n++) {
                                    if (clients[n].id == fds[i].fd) {
                                        strcpy(name, clients[n].name);
                                    }
                                }

                                char buffer[BUFFER_SIZE];
                                snprintf(buffer, BUFFER_SIZE, "%s: %s", name, payload);
                                size_t buffer_len = strlen(buffer);
                                frame_len = ws_encode_frame(buffer, buffer_len, frame);

                                // Send the encoded frame
                                send(fds[j].fd, frame, frame_len, 0);
                            }
                        }
                    }
                }
            }
        }
    }

    // This line is never reached (infinite loop above)
    return 0;
}
