#include <stdio.h>      // Standard I/O: printf, fprintf, perror, fopen, fclose
#include <stdlib.h>     // Standard library: exit, malloc, etc.
#include <string.h>     // String operations: strcmp, strcpy, strlen, memcpy
#include <unistd.h>     // UNIX standard: close, read, write
#include <sys/socket.h> // Socket API: socket, connect, send, recv, getsockopt
#include <netdb.h>      // Network database operations: getaddrinfo, freeaddrinfo
#include <poll.h>       // Poll system call for multiplexing I/O
#include <time.h>       // Time functions: time() for seeding random number generator
#include <fcntl.h>      // File control: fcntl() for non-blocking sockets
#include <errno.h>      // Error numbers: errno, EINPROGRESS


// Size of buffers used for receiving and sending data
#define BUFFER_SIZE 4096

// Message flags matching server
#define WS_NO_BROADCAST (1 << 0)
#define WS_SEND_BACK (1 << 1)
#define WS_CHANGE_USERNAME (1 << 2)

/**
 * Creates a JSON formatted message for the server
 * Format: {"user": {"name": "username"},"message": {"text": "text","text_len": len,"info": flags}}
 *
 * @param username - The username to include
 * @param text - The message text
 * @param flags - Message flags (WS_NO_BROADCAST, WS_CHANGE_USERNAME, etc.)
 * @param output - Buffer to write the JSON string to
 * @param output_size - Size of the output buffer
 * @return Length of the JSON string, or -1 on error
 */
int create_json_message(const char* username, const char* text, int flags, char* output, size_t output_size) {
    // Remove newline from text if present
    char clean_text[BUFFER_SIZE];
    strncpy(clean_text, text, sizeof(clean_text) - 1);
    clean_text[sizeof(clean_text) - 1] = '\0';

    // Remove trailing newline/carriage return
    size_t text_len = strlen(clean_text);
    while (text_len > 0 && (clean_text[text_len - 1] == '\n' || clean_text[text_len - 1] == '\r')) {
        clean_text[text_len - 1] = '\0';
        text_len--;
    }

    int written = snprintf(output, output_size,
        "{\"user\": {\"name\": \"%s\"},\"message\": {\"text\": \"%s\",\"text_len\": %zu,\"info\": %d}}",
        username, clean_text, text_len, flags);

    if (written < 0 || (size_t)written >= output_size) {
        return -1;
    }

    return written;
}

/**
 * Encodes data into a WebSocket frame format with masking (required for client->server)
 * WebSocket protocol requires clients to mask all frames they send to the server
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
    return -1;
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
int ws_decode_frame(unsigned char *data, int len, char *payload) {
    // Minimum frame size is 2 bytes (opcode + mask/length)
    if (len < 2) return -1;

    // Extract opcode from first byte (lower 4 bits)
    // Opcode determines frame type: 0x1=text, 0x2=binary, 0x8=close, etc.
    int opcode = data[0] & 0x0F;
    // Check if this is a close frame (opcode 0x8)
    if (opcode == 0x8) return -2; // Signal connection should close

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
int ws_client_handshake(int sockfd, const char *host) {
    // Buffer for the HTTP upgrade request
    char request[BUFFER_SIZE];
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
        host, key);

    // Send the handshake request to the server
    send(sockfd, request, strlen(request), 0);

    // Buffer to receive the server's response
    char buffer[BUFFER_SIZE];
    // Receive the response (blocking call)
    int n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
    // Check if we received any data
    if (n <= 0) {
        // No response or error occurred
        fprintf(stderr, "No response from server\n");
        return -1;
    }
    // Null-terminate the received data for string operations
    buffer[n] = '\0';

    // Debug: print the server's response to stderr
    fprintf(stderr, "Server response:\n%s\n", buffer);

    // Check if the response contains the expected "101 Switching Protocols" status
    // This indicates successful WebSocket upgrade
    if (strstr(buffer, "101 Switching Protocols")) {
        // Handshake successful
        return 0;
    }
    // Handshake failed (wrong status code or invalid response)
    return -1;
}

/**
 * Main client function
 * Connects to a WebSocket server and allows sending/receiving messages
 * Supports both interactive mode and headless mode for automated testing
 */
int main(int argc, char *argv[]) {
    // Pointer to test message (for headless mode)
    char *test_msg = NULL;
    // Flag indicating headless mode (no interactive input)
    int headless = 0;
    // Flag indicating whether to log chat messages to file
    int chat_log = 0;
    // Name of the log file
    const char *log_file_name = "chat_log.log";
    // File pointer for the log file
    FILE* file = NULL;
    // Pointer to username/display name
    char *name = NULL;
    // Flag indicating whether username was provided
    int name_type = 0;
    // Current username (default to "Anonym" if not provided)
    char current_username[256] = "Anonym";
    // Pointer to server hostname/IP
    char *host = NULL;
    // Pointer to server port
    char *port = NULL;
    // Flag indicating whether port was explicitly specified
    int port_specified = 0;
    // Flag indicating whether host was explicitly specified
    int host_specified = 0;

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        // Check for -m flag (test message for headless mode)
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            // Next argument is the test message
            test_msg = argv[i + 1];
            // Enable headless mode
            headless = 1;
            // Skip the next argument since we already processed it
            i++;
        }
        // Check for -n flag (username/display name)
        else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            // Mark that username was provided
            name_type = 1;
            // Next argument is the username
            name = argv[i + 1];
            // Skip the next argument
            i++;
        }
        // Check for -s flag (enable chat logging)
        else if (strcmp(argv[i], "-s") == 0) {
            // Print status message
            printf("Running client with logging\n");
            // Enable chat logging
            chat_log = 1;
        }
        // Check for -h flag (server host)
        else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            // Next argument is the host
            host = argv[i + 1];
            // Mark that host was explicitly specified
            host_specified = 1;
            // Skip the next argument
            i++;
        }
        // Check for -p flag (server port)
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            // Next argument is the port
            port = argv[i + 1];
            // Mark that port was explicitly specified
            port_specified = 1;
            // Skip the next argument
            i++;
        }
    }

    // Set default host if not specified by user
    if (!host_specified) {
        // Use default host from ip.h
        host = "127.0.0.1";
    }

    // Set default port based on host if not specified by user
    if (!port_specified) {
        // Check if connecting to localhost or loopback address
        if (host && (strcmp(host, "localhost") == 0 ||
                     strcmp(host, "127.0.0.1") == 0 ||
                     strcmp(host, "0.0.0.0") == 0)) {
            // Use port 9999 for local connections
            port = "9999";
        } else {
            // Use port 80 for remote connections (for Cloudflare tunnels)
            port = "80";
        }
    }

    // Create log file if chat logging is enabled
    if (chat_log) {
        // Open file for writing (creates if doesn't exist, truncates if exists)
        file = fopen(log_file_name, "w");
        // Check if file creation succeeded
        if (!file) {
            // Print error message
            fprintf(stderr, "Failed to create chat_log.log file try running withoug -s flag!\n");
            return -1;
        }
        // Print success message
        printf("Created file\n");
        // Close the file (we'll reopen it in append mode when logging)
        fclose(file);
    }

    // Prepare hints for getaddrinfo to resolve the server address
    struct addrinfo hints = {0};  // Zero-initialize the structure
    struct addrinfo *result;       // Will point to the resolved address info

    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // Use TCP (stream sockets)

    // Resolve the server address (convert hostname to IP address)
    if (getaddrinfo(host, port, &hints, &result) != 0) {
        // If resolution fails, print error
        perror("getaddrinfo failed");
        return 1;
    }

    // Create a socket using the resolved address information
    int sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sockfd < 0) {
        // If socket creation fails, print error
        perror("socket failed");
        // Free the address info structure
        freeaddrinfo(result);
        return 1;
    }

    // Set socket to non-blocking mode for connection timeout
    // This allows us to implement a timeout for the connect() call
    int flags = fcntl(sockfd, F_GETFL, 0);  // Get current socket flags
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK); // Add non-blocking flag

    // Attempt to connect to the server
    int conn_result = connect(sockfd, result->ai_addr, result->ai_addrlen);
    // Check if connection failed (non-blocking connect returns -1 with EINPROGRESS)
    if (conn_result < 0 && errno != EINPROGRESS) {
        // Real error occurred (not just "in progress")
        perror("connect failed");
        close(sockfd);
        freeaddrinfo(result);
        return 1;
    }

    // Wait for connection to complete with timeout
    if (errno == EINPROGRESS) {
        // Connection is in progress, use poll() to wait with timeout
        struct pollfd pfd = {sockfd, POLLOUT, 0};  // Wait for socket to become writable
        // Poll with 10-second timeout (10000 milliseconds)
        int poll_result = poll(&pfd, 1, 10000);

        // Check if poll timed out (returned 0)
        if (poll_result == 0) {
            // Timeout occurred
            fprintf(stderr, "Connection timeout to %s:%s\n", host, port);
            close(sockfd);
            freeaddrinfo(result);
            return 1;
        }

        // Check if poll failed
        if (poll_result < 0) {
            perror("poll failed");
            close(sockfd);
            freeaddrinfo(result);
            return 1;
        }

        // Connection completed, check if it succeeded or failed
        int error;  // Will hold the socket error code
        socklen_t len = sizeof(error);  // Length of the error value
        // Get the socket error status
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            // Connection failed, print error with details
            fprintf(stderr, "Connection failed to %s:%s: %s\n", host, port, strerror(error));
            close(sockfd);
            freeaddrinfo(result);
            return 1;
        }
    }

    // Set socket back to blocking mode (restore original flags)
    fcntl(sockfd, F_SETFL, flags);

    // Free the address info structure (no longer needed after connect)
    freeaddrinfo(result);

    // Print connection success message
    printf("Connected to server at %s:%s\n", host, port);
    fflush(stdout);

    // Perform WebSocket handshake with the server
    if (ws_client_handshake(sockfd, host) < 0) {
        // Handshake failed
        fprintf(stderr, "WebSocket handshake failed\n");
        close(sockfd);
        return 1;
    }

    // Print handshake success message
    printf("WebSocket handshake complete\n");
    fflush(stdout);

    // Send username to server if provided
    if (name_type && name) {
        // Copy username to current_username
        strncpy(current_username, name, sizeof(current_username) - 1);
        current_username[sizeof(current_username) - 1] = '\0';

        // Buffer for the encoded frame
        unsigned char frame[BUFFER_SIZE];
        // Buffer for the JSON message
        char json_msg[BUFFER_SIZE];

        // Create JSON message with WS_CHANGE_USERNAME flag
        int json_len = create_json_message(current_username, "null",
                                          WS_CHANGE_USERNAME | WS_NO_BROADCAST,
                                          json_msg, sizeof(json_msg));
        if (json_len > 0) {
            // Encode the JSON message into a WebSocket frame
            int frame_len = ws_encode_frame(json_msg, json_len, frame);
            // Send the frame to the server
            send(sockfd, frame, frame_len, 0);
        }
    }

    // Send test message if in headless mode
    if (headless && test_msg) {
        // Buffer for the encoded frame
        unsigned char frame[BUFFER_SIZE];
        // Buffer for the JSON message
        char json_msg[BUFFER_SIZE];

        // Create JSON message with current username
        int json_len = create_json_message(current_username, test_msg, 0,
                                          json_msg, sizeof(json_msg));
        if (json_len > 0) {
            // Encode the JSON message into a WebSocket frame
            int frame_len = ws_encode_frame(json_msg, json_len, frame);
            // Send the frame to the server
            send(sockfd, frame, frame_len, 0);
            // Print what was sent
            printf("Sent: %s\n", test_msg);
            fflush(stdout);
        }
    }

    // Set up poll array for multiplexing stdin and socket
    struct pollfd fds[2] = {
        {0, POLLIN, 0},        // Index 0: stdin (file descriptor 0)
        {sockfd, POLLIN, 0}    // Index 1: server socket
    };

    // Flag to track if we've received at least one response (for headless mode)
    int received_response = 0;

    // Main client loop (runs until disconnection or timeout in headless mode)
    for (;;) {
        // Set timeout based on mode
        // Headless: 5 seconds (5000ms) for testing
        // Interactive: 50 seconds (50000ms) for keeping connection alive
        int timeout = headless ? 5000 : 50000;
        // Wait for activity on stdin or socket
        int ret = poll(fds, 2, timeout);

        // In headless mode, exit after receiving response and timeout expires
        if (ret == 0 && headless && received_response) {
            printf("Test complete\n");
            // Exit the loop
            break;
        }

        // Check if stdin has data (user typed something)
        if (fds[0].revents & POLLIN) {
            // Buffer to read user input
            char buffer[256];
            // Read from stdin (up to 255 characters)
            int len = read(0, buffer, 255);
            // If we read something
            if (len > 0) {
                // Null-terminate the input
                buffer[len] = '\0';

                // Buffer for the JSON message
                char json_msg[BUFFER_SIZE];
                // Create JSON message with current username
                int json_len = create_json_message(current_username, buffer, 0,
                                                  json_msg, sizeof(json_msg));
                if (json_len > 0) {
                    // Encode the JSON message into a WebSocket frame
                    unsigned char frame[BUFFER_SIZE];
                    int frame_len = ws_encode_frame(json_msg, json_len, frame);
                    // Send the frame to the server
                    send(sockfd, frame, frame_len, 0);
                }
            }
        }

        // Check if socket has data (server sent a message)
        if (fds[1].revents & POLLIN) {
            // Buffer to receive data
            unsigned char buffer[BUFFER_SIZE];
            // Receive data from the server
            int len = recv(sockfd, buffer, BUFFER_SIZE, 0);

            // If recv returns 0, server disconnected gracefully
            if (len == 0) {
                printf("Server disconnected\n");
                // Exit the program
                return 0;
            }

            // If we received data
            if (len > 0) {
                // Buffer for the decoded payload
                char payload[BUFFER_SIZE];
                // Decode the WebSocket frame
                int payload_len = ws_decode_frame(buffer, len, payload);

                // If we successfully decoded a payload
                if (payload_len > 0) {
                    // Log to file if chat logging is enabled and payload is not empty
                    if (chat_log && strcmp(payload, "") != 0) {
                        // Open log file in append mode
                        FILE* file = fopen(log_file_name, "a");
                        // Check if file open succeeded
                        if (!file) {
                            fprintf(stderr, "Failed to open %s try running without -s flag!\n", log_file_name);
                            return -1;
                        }
                        // Write the payload to the log file
                        fprintf(file, "%s", payload);
                        // Close the log file
                        fclose(file);
                    }

                    // Print the received message to stdout
                    printf("%s", payload);
                    fflush(stdout);
                    // Mark that we've received at least one response
                    received_response = 1;
                }
            }
        }
    }

    // Close the socket before exiting
    close(sockfd);
    return 0;
}
