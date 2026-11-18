#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include "net_common.h"

int main(void)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(TCP_PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_fd, (const struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        close(listen_fd);
        return 1;
    }
    if (listen(listen_fd, 1) == -1) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    printf("server: listening on 0.0.0.0:%d; listen_fd=%d\n", TCP_PORT, listen_fd);
    fflush(stdout);

    int connection_fd;
    do {
        connection_fd = accept(listen_fd, NULL, NULL);
    } while (connection_fd == -1 && errno == EINTR);
    if (connection_fd == -1) {
        perror("accept");
        close(listen_fd);
        return 1;
    }
    printf("server: accepted one connection; connection_fd=%d\n", connection_fd);

    size_t total = 0;
    int status = 0;
    char buffer[64];
    for (;;) {
        ssize_t n = read(connection_fd, buffer, sizeof(buffer));
        if (n > 0) {
            printf("server: read %zd bytes; echoing\n", n);
            if (send_all(connection_fd, buffer, (size_t)n) == -1) {
                perror("send echo");
                status = 1;
                break;
            }
            total += (size_t)n;
        } else if (n == 0) {
            printf("server: peer send side ended; echoed %zu bytes\n", total);
            break;
        } else if (errno != EINTR) {
            perror("read connection");
            status = 1;
            break;
        }
    }

    if (close(connection_fd) == -1) {
        perror("close connection");
        status = 1;
    }
    if (close(listen_fd) == -1) {
        perror("close listener");
        status = 1;
    }
    printf("server: sockets closed; exiting\n");
    return status;
}
