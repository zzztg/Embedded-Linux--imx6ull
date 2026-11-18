#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include "stream_io.h"

int main(int argc, char *argv[])
{
    int hold = 0;
    if (argc == 3 && strcmp(argv[2], "hold") == 0) {
        hold = 1;
    } else if (argc != 2) {
        fprintf(stderr, "Usage: %s <server-ipv4> [hold]\n", argv[0]);
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
            fprintf(stderr, "client: invalid IPv4 address\n");
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

    const char message[] = "Hello from WSL!\n";
    const size_t expected = sizeof(message) - 1;
    if (send_all(fd, message, expected) == -1) {
        perror("send request");
        close(fd);
        return 1;
    }
    printf("client: sent %zu bytes\n", expected);

    if (hold) {
        printf("client: write side still open; press ENTER to shutdown SHUT_WR\n");
        fflush(stdout);
        if (getchar() == EOF) {
            fprintf(stderr, "client: input ended before ENTER\n");
            close(fd);
            return 2;
        }
    }

    if (shutdown(fd, SHUT_WR) == -1) {
        perror("shutdown SHUT_WR");
        close(fd);
        return 1;
    }
    printf("client: write side shut down; fd remains open for reading\n");

    char reply[MAX_REQUEST + 1] = {0};
    size_t length = 0;
    if (receive_until_eof(fd, reply, MAX_REQUEST, &length, "client") == -1) {
        perror("receive reply");
        close(fd);
        return 1;
    }
    int matches = length == expected && memcmp(reply, message, expected) == 0;
    printf("client: reply matches=%s; text follows:\n%s", matches ? "yes" : "no", reply);

    if (close(fd) == -1) {
        perror("close connection");
        return 1;
    }
    printf("client: fd closed\n");
    return matches ? 0 : 2;
}
