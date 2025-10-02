
#include "ws_client_lib.h"
#include <asm-generic/errno.h>
#include <netdb.h>

int32_t wsInitClient(wsClient* client, const char* ip, const char* port, const char* username) {

    struct addrinfo hints = {0};
    struct addrinfo* result = NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Convert URL to ip
    WS_LOG_DEBUG("[WS CLIENT] Attempting to resolve %s:%s\n", ip, port);
    fflush(stdout);
    fflush(stderr);
    if (getaddrinfo(ip, port, &hints, &result) != 0) {
        WS_LOG_ERROR("[WS CLIENT] Failed to convert URL to valid IP address %s!\n", ip);
        fflush(stderr);
        return WS_ERROR;
    }
    WS_LOG_DEBUG("[WS CLIENT] Successfully resolved %s:%s\n", ip, port);
    fflush(stdout);

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

    return WS_OK;
}

int32_t wsSendMessage(wsClient* client, const char *message) {

    char buffer[WS_BUFFER_SIZE];
    snprintf(buffer, WS_BUFFER_SIZE, "%s: %s", client->username, message);
    size_t len = strlen(buffer);

    uint8_t frame[WS_BUFFER_SIZE];
    int32_t frameLen = __ws_encode_frame(buffer, len, frame);
    send(client->id, frame, frameLen, 0);

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


// ==================
// Internal functions
// ==================

/**
 * Encodes data into a WebSocket frame format with masking (required for client->server)
 * WebSocket protocol requires clients to mask all frames they send to the server
 *
 * @param payload - Pointer to the data to send
 * @param len - Length of the data in bytes
 * @param frame - Output buffer to store the encoded frame
 * @return Length of the encoded frame in bytes, -1 on error
 */
int32_t __ws_encode_frame(const char *payload, int32_t len, uint8_t *frame) {
    // Byte 0: FIN bit (1) + RSV bits (000) + Opcode (0001 for text frame)
    // 0x81 = 10000001 binary = FIN + text frame
    frame[0] = 0x81;

    // Generate a random 4-byte masking key
    // WebSocket spec requires clients to mask frames with a random key
    unsigned char mask[4];
    // Seed random number generator with current time
    srand(time(NULL));
    // Generate 4 random bytes for the mask
    for (int i = 0; i < 4; i++) {
        mask[i] = rand() % 256;  // Random byte (0-255)
    }

    // For small payloads (≤125 bytes), length fits in byte 1
    if (len <= 125) {
        // Byte 1: MASK bit (1) + 7-bit payload length
        // 0x80 sets the mask bit, then OR with the length
        frame[1] = 0x80 | len;
        // Copy the 4-byte masking key to bytes 2-5
        memcpy(&frame[2], mask, 4);
        // Mask and copy the payload data starting at byte 6
        for (int i = 0; i < len; i++) {
            // XOR each payload byte with the corresponding mask byte (cycling every 4)
            frame[6 + i] = payload[i] ^ mask[i % 4];
        }
        // Total frame size: 2-byte header + 4-byte mask + payload
        return len + 6;
    }
    // For medium payloads (126-65535 bytes), use extended 16-bit length
    else if (len <= 65535) {
        // Byte 1: MASK bit (1) + length indicator (126)
        frame[1] = 0x80 | 126;
        // Bytes 2-3: 16-bit payload length in network byte order (big-endian)
        frame[2] = (len >> 8) & 0xFF;  // High byte
        frame[3] = len & 0xFF;          // Low byte
        // Copy the 4-byte masking key to bytes 4-7
        memcpy(&frame[4], mask, 4);
        // Mask and copy the payload data starting at byte 8
        for (int i = 0; i < len; i++) {
            // XOR each payload byte with the corresponding mask byte (cycling every 4)
            frame[8 + i] = payload[i] ^ mask[i % 4];
        }
        // Total frame size: 4-byte header + 4-byte mask + payload
        return len + 8;
    }
    // Payloads larger than 65535 bytes are not supported
    return WS_ERROR;
}

/**
 * Decodes a WebSocket frame received from the server
 * Server-to-client frames are not masked (only client-to-server frames are masked)
 *
 * @param data - Pointer to the raw WebSocket frame data
 * @param len - Length of the frame data in bytes
 * @param payload - Output buffer to store the decoded payload
 * @return Length of decoded payload, -1 on error, -2 if close frame
 */
int32_t __ws_decode_frame(uint8_t *data, int32_t len, char *payload) {
    // Minimum frame size is 2 bytes (opcode + mask/length)
    if (len < 2) return WS_ERROR;

    // Extract opcode from first byte (lower 4 bits)
    // Opcode determines frame type: 0x1=text, 0x2=binary, 0x8=close, etc.
    int opcode = data[0] & 0x0F;
    // Check if this is a close frame (opcode 0x8)
    if (opcode == 0x8) return WS_ERROR; // Signal connection should close

    // Extract payload length from second byte (lower 7 bits)
    // We ignore the mask bit since server frames should not be masked
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

    // Copy the payload data directly (server frames are not masked)
    memcpy(payload, &data[pos], payload_len);
    // Null-terminate the payload string
    payload[payload_len] = '\0';
    // Return the length of the decoded payload
    return payload_len;
}


/**
 * Performs the WebSocket client handshake with the server
 * Sends an HTTP Upgrade request and validates the response
 *
 * @param sockfd - Connected socket file descriptor
 * @param host - Hostname/IP to include in the Host header
 * @return 0 on success, -1 on failure
 */
int32_t __ws_client_handshake(int32_t sockfd, const char *ip) {
    // Buffer for the HTTP upgrade request
    char request[WS_BUFFER_SIZE];
    // Static WebSocket key (a real implementation would generate this randomly)
    char key[] = "dGhlIHNhbXBsZSBub25jZQ==";

    // Build the HTTP upgrade request according to WebSocket protocol (RFC 6455)
    snprintf(request, sizeof(request),
        "GET / HTTP/1.1\r\n"                     // Request line: GET method, root path, HTTP/1.1
        "Host: %s\r\n"                           // Host header (required by HTTP/1.1)
        "Upgrade: websocket\r\n"                 // Upgrade header (indicates WebSocket upgrade)
        "Connection: Upgrade\r\n"                // Connection header (must contain "Upgrade")
        "Sec-WebSocket-Key: %s\r\n"              // Random key for handshake validation
        "Sec-WebSocket-Version: 13\r\n\r\n",     // WebSocket protocol version (13 is current)
        ip, key);

    // Send the handshake request to the server
    send(sockfd, request, strlen(request), 0);

    // Buffer to receive the server's response
    char buffer[WS_BUFFER_SIZE];
    // Receive the response (blocking call)
    int n = recv(sockfd, buffer, WS_BUFFER_SIZE - 1, 0);
    // Check if we received any data
    if (n <= 0) {
        // No response or error occurred
        fprintf(stderr, "No response from server\n");
        return WS_ERROR;
    }
    // Null-terminate the received data for string operations
    buffer[n] = '\0';

    // Debug: print the server's response to stderr
    fprintf(stderr, "Server response:\n%s\n", buffer);

    // Check if the response contains the expected "101 Switching Protocols" status
    // This indicates successful WebSocket upgrade
    if (strstr(buffer, "101 Switching Protocols")) {
        // Handshake successful
        return WS_OK;
    }
    // Handshake failed (wrong status code or invalid response)
    return WS_ERROR;


}
