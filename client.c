
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h> 
#include <unistd.h>

#include "ip.h"

int main() {

    int sockfd = socket(AF_INET6, SOCK_STREAM, 0);

    struct sockaddr_in6 address = {
        AF_INET6,
        htons(9999),
        0
    };

    inet_pton(AF_INET6, IP, &address.sin6_addr);
    connect(sockfd, (struct sockaddr*)&address, sizeof(address));


    // stdin == 0
    struct pollfd fds[2] = {
        {
            0, POLLIN, 0,
        },
        {
            sockfd, POLLIN, 0
        }
    };

    for (;;) {
        char buffer[256] = {0};
        poll(fds, 2, 50000);
        if (fds[0].revents & POLLIN) {
            read(0, buffer, 255);
            send(sockfd, buffer, 255, 0);

        }
        else if (fds[1].revents & POLLIN) {
            if (recv(sockfd, buffer, 255, 0) == 0) {
                return 0;
            }
            printf("%s", buffer);
        }
    }

    return 0;
}

