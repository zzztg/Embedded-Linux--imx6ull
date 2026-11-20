#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "echo_common.h"

static int send_all(int fd, const unsigned char *data, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        ssize_t n = send(fd, data + sent, length - sent, MSG_NOSIGNAL);
        if (n > 0) {
            sent += (size_t)n;
        } else if (n == -1 && errno == EINTR) {
            continue;
        } else {
            if (n == 0) {
                errno = EIO;
            }
            return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 3 ||
        (argc == 3 && strcmp(argv[2], "hold") != 0 && strcmp(argv[2], "empty") != 0)) {
        fprintf(stderr, "Usage: %s <server-ipv4> [hold|empty]\n", argv[0]);
        return 2;
    }
    int hold = argc == 3 && strcmp(argv[2], "hold") == 0;
    size_t request_length = argc == 3 && strcmp(argv[2], "empty") == 0 ? 0 : TEST_BYTES;
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
    if (connect(fd, (const struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("connect");
        close(fd);
        return 1;
    }
    printf("client: connected to %s:%d\n", argv[1], TCP_PORT);
    unsigned char request[TEST_BYTES];
    for (size_t i = 0; i < request_length; ++i) {
        request[i] = (unsigned char)(i % 251U);
    }
    if (send_all(fd, request, request_length) == -1) {
        perror("send request");
        close(fd);
        return 1;
    }
    printf("client: sent %zu bytes\n", request_length);
    if (hold) {
        printf("client: write side still open; press ENTER to shutdown SHUT_WR\n");
        fflush(stdout);
        if (getchar() == EOF) {
            fprintf(stderr, "client: stdin ended; proceeding to shutdown\n");
        }
    }
    if (shutdown(fd, SHUT_WR) == -1) {
        perror("shutdown write side");
        close(fd);
        return 1;
    }
    printf("client: write side ended; receiving echo until EOF\n");
    size_t received = 0;
    int matches = 1;
    unsigned char reply[256];
    for (;;) {
        ssize_t n = read(fd, reply, sizeof(reply));
        if (n > 0) {
            if ((size_t)n > request_length - received) {
                fprintf(stderr, "client: unexpected extra echo bytes\n");
                close(fd);
                return 2;
            }
            for (size_t i = 0; i < (size_t)n; ++i) {
                if (reply[i] != request[received + i]) {
                    matches = 0;
                }
            }
            received += (size_t)n;
        } else if (n == 0) {
            break;
        } else if (errno != EINTR) {
            perror("read echo");
            close(fd);
            return 1;
        }
    }
    matches = matches && received == request_length;
    printf("client: EOF; received=%zu; echo matches=%s\n", received, matches ? "yes" : "no");
    if (close(fd) == -1) {
        perror("close connection");
        return 1;
    }
    return matches ? 0 : 2;
}
