#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "transfer_common.h"

int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "hold") != 0)) {
        fprintf(stderr, "Usage: %s <server-ipv4> [hold]\n", argv[0]);
        return 2;
    }
    int hold = argc == 3;
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
    if (hold) {
        int receive_buffer_bytes = 4096;
        if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
                       &receive_buffer_bytes, sizeof(receive_buffer_bytes)) == -1) {
            perror("set receive buffer");
            close(fd);
            return 1;
        }
    }
    if (connect(fd, (const struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("connect");
        close(fd);
        return 1;
    }
    printf("client: connected to %s:%d; expecting %u bytes\n",
           argv[1], TCP_PORT, RESPONSE_BYTES);
    if (hold) {
        printf("client: not reading yet; press ENTER to start receiving\n");
        fflush(stdout);
        if (getchar() == EOF) {
            fprintf(stderr, "client: stdin ended; starting receive\n");
        }
    }

    size_t received = 0;
    int matches = 1;
    char buffer[4096];
    for (;;) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n > 0) {
            if ((size_t)n > RESPONSE_BYTES - received) {
                fprintf(stderr, "client: response exceeds expected size\n");
                close(fd);
                return 2;
            }
            for (size_t i = 0; i < (size_t)n; ++i) {
                if (buffer[i] != (char)('A' + (received + i) % 26)) {
                    matches = 0;
                }
            }
            received += (size_t)n;
        } else if (n == 0) {
            break;
        } else if (errno != EINTR) {
            perror("read response");
            close(fd);
            return 1;
        }
    }
    matches = matches && received == RESPONSE_BYTES;
    printf("client: EOF; received=%zu; pattern matches=%s\n",
           received, matches ? "yes" : "no");
    if (close(fd) == -1) {
        perror("close connection");
        return 1;
    }
    return matches ? 0 : 2;
}
