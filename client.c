#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>

#include "ip.h"

int main(int argc, char *argv[]) {
    char *test_msg = NULL;
    int headless = 0;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            test_msg = argv[i + 1];
            headless = 1;
            i++;
        }
    }

    struct addrinfo hints = {0};
    struct addrinfo *result;

    hints.ai_family = AF_UNSPEC; // Support both IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(IP, "9999", &hints, &result) != 0) {
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

    printf("Connected to server at %s:9999\n", IP);
    fflush(stdout);

    // If headless mode, send test message immediately
    if (headless && test_msg) {
        int len = strlen(test_msg);
        send(sockfd, test_msg, len, 0);
        printf("Sent: %s", test_msg);
        fflush(stdout);
    }

    // stdin == 0
    struct pollfd fds[2] = {
        {
            0, POLLIN, 0,
        },
        {
            sockfd, POLLIN, 0
        }
    };

    int received_response = 0;
    for (;;) {
        char buffer[256] = {0};
        int timeout = headless ? 5000 : 50000;
        int ret = poll(fds, 2, timeout);

        if (ret == 0 && headless && received_response) {
            // Timeout in headless mode after receiving response
            printf("Test complete\n");
            break;
        }

        if (fds[0].revents & POLLIN) {
            int len = read(0, buffer, 255);
            if (len > 0) {
                send(sockfd, buffer, len, 0);
            }

        }
        else if (fds[1].revents & POLLIN) {
            int len = recv(sockfd, buffer, 255, 0);
            if (len == 0) {
                printf("Server disconnected\n");
                return 0;
            }
            if (len > 0) {
                printf("Received: ");
                write(1, buffer, len);
                fflush(stdout);
                received_response = 1;
            }
        }
    }

    close(sockfd);
    return 0;
}

