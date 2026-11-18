#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "net_common.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server-ipv4>\n", argv[0]);
        return 2;
    }

    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(TCP_PORT);
    int parsed = inet_pton(AF_INET, argv[1], &server.sin_addr);
    if (parsed != 1) {
        if (parsed == -1) {
            perror("inet_pton");
        } else {
            fprintf(stderr, "client: invalid IPv4 address: %s\n", argv[1]);
        }
        return 2;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return 1;
    }
    printf("client: connecting to %s:%d; fd=%d\n", argv[1], TCP_PORT, fd);
    if (connect(fd, (const struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("connect");
        close(fd);
        return 1;
    }

    const char message[] = "Hello from WSL!\n";
    const size_t length = sizeof(message) - 1;
    if (send_all(fd, message, length) == -1) {
        perror("send message");
        close(fd);
        return 1;
    }
    printf("client: sent %zu bytes\n", length);

    /* One extra zero-initialized byte is reserved for printf, not sent by TCP. */
    char reply[sizeof(message)] = {0};
    size_t received = 0;
    while (received < length) {
        ssize_t n = read(fd, reply + received, length - received);
        if (n > 0) {
            received += (size_t)n;
        } else if (n == -1 && errno == EINTR) {
            continue;
        } else {
            if (n == 0) {
                fprintf(stderr, "client: server ended before the complete echo\n");
            } else {
                perror("read echo");
            }
            close(fd);
            return 1;
        }
    }

    int matches = memcmp(message, reply, length) == 0;
    printf("client: received %zu bytes; echo follows:\n%s", received, reply);
    printf("client: echo matches: %s\n", matches ? "yes" : "no");

    if (close(fd) == -1) {
        perror("close connection");
        return 1;
    }
    return matches ? 0 : 2;
}
