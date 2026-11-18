#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <stdio.h>
#include "stream_io.h"

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
    printf("server: listening on 0.0.0.0:%d\n", TCP_PORT);
    fflush(stdout);

    int fd;
    do {
        fd = accept(listen_fd, NULL, NULL);
    } while (fd == -1 && errno == EINTR);
    if (fd == -1) {
        perror("accept");
        close(listen_fd);
        return 1;
    }
    if (close(listen_fd) == -1) {
        perror("close listener");
        close(fd);
        return 1;
    }
    printf("server: accepted; waiting for request EOF before replying\n");

    char request[MAX_REQUEST];
    size_t length = 0;
    int status = 0;
    if (receive_until_eof(fd, request, sizeof(request), &length, "server") == -1) {
        perror("receive request");
        status = 1;
    } else {
        printf("server: request ended; sending %zu-byte reply AFTER EOF\n", length);
        if (send_all(fd, request, length) == -1) {
            perror("send reply");
            status = 1;
        }
    }

    if (close(fd) == -1) {
        perror("close connection");
        status = 1;
    }
    printf("server: connection closed; exiting\n");
    return status;
}
