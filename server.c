
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

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

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(9999),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(sockfd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }

    if (listen(sockfd, 10) < 0) {
        perror("listen failed");
        return 1;
    }

    printf("Server listening on 0.0.0.0:9999\n");
    fflush(stdout);

    int clientfd = accept(sockfd, 0, 0);
    if (clientfd < 0) {
        perror("accept failed");
        return 1;
    }
    printf("Client connected (fd=%d)\n", clientfd);
    fflush(stdout);

    // stdin == 0
    struct pollfd fds[2] = {
        {
            0, POLLIN, 0,
        },
        {
            clientfd, POLLIN, 0
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
                send(clientfd, buffer, len, 0);
            }

        }
        else if (fds[1].revents & POLLIN) {
            int len = recv(clientfd, buffer, 255, 0);
            if (len == 0) {
                printf("Client disconnected\n");
                return 0;
            }
            if (len > 0) {
                printf("Received: ");
                write(1, buffer, len);
                fflush(stdout);

                // Echo back to client in headless mode
                if (headless && test_msg) {
                    int msg_len = strlen(test_msg);
                    send(clientfd, test_msg, msg_len, 0);
                    printf("Sent: %s", test_msg);
                    fflush(stdout);
                }
                received_response = 1;
            }
        }
    }

    close(clientfd);
    close(sockfd);
    return 0;
}

