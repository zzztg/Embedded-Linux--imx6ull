#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "udp_common.h"

enum { REPLY_TIMEOUT_MS = 3000 };

struct sample {
    const char *data;
    size_t length;
};

static const struct sample samples[DATAGRAM_COUNT] = {
    {"HELLO", sizeof("HELLO") - 1},
    {"i.MX6ULL", sizeof("i.MX6ULL") - 1},
    {"", 0},
};

static int exchange_datagrams(int fd, const struct sockaddr_in *server)
{
    for (int i = 0; i < DATAGRAM_COUNT; ++i) {
        if (send_datagram(fd, samples[i].data, samples[i].length, server) == -1) {
            perror("sendto request");
            return 1;
        }
        printf("client: sent datagram %d; length=%zu\n", i + 1, samples[i].length);
    }
    fflush(stdout);

    int seen[DATAGRAM_COUNT] = {0};
    for (int received = 0; received < DATAGRAM_COUNT; ++received) {
        struct pollfd watch = {.fd = fd, .events = POLLIN};
        int ready;
        do {
            ready = poll(&watch, 1, REPLY_TIMEOUT_MS);
        } while (ready == -1 && errno == EINTR);
        if (ready == 0) {
            fprintf(stderr, "client: timed out waiting for echo %d/%d\n",
                    received + 1, DATAGRAM_COUNT);
            return 1;
        }
        if (ready == -1) {
            perror("poll");
            return 1;
        }

        unsigned char buffer[PAYLOAD_CAPACITY];
        struct sockaddr_in peer = {0};
        socklen_t peer_length = sizeof(peer);
        ssize_t n = recvfrom(fd, buffer, sizeof(buffer), MSG_TRUNC | MSG_DONTWAIT,
                             (struct sockaddr *)&peer, &peer_length);
        if (n == -1) {
            perror("recvfrom echo");
            return 1;
        }
        if (peer.sin_addr.s_addr != server->sin_addr.s_addr ||
            peer.sin_port != server->sin_port || (size_t)n > sizeof(buffer)) {
            fprintf(stderr, "client: unexpected source or oversized echo\n");
            return 1;
        }

        int matched = 0;
        for (int i = 0; i < DATAGRAM_COUNT; ++i) {
            if (!seen[i] && (size_t)n == samples[i].length &&
                memcmp(buffer, samples[i].data, samples[i].length) == 0) {
                seen[i] = 1;
                matched = i + 1;
                break;
            }
        }
        if (!matched) {
            fprintf(stderr, "client: unexpected or duplicate echo; length=%zd\n", n);
            return 1;
        }
        printf("client: received echo for sample %d; length=%zd%s; matches=yes\n",
               matched, n, n == 0 ? " (empty datagram)" : "");
    }
    printf("client: all %d datagrams verified\n", DATAGRAM_COUNT);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server-ipv4>\n", argv[0]);
        return 2;
    }
    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(UDP_PORT);
    int parsed = inet_pton(AF_INET, argv[1], &server.sin_addr);
    if (parsed != 1) {
        if (parsed == -1) {
            perror("inet_pton");
        } else {
            fprintf(stderr, "client: invalid IPv4 address: %s\n", argv[1]);
        }
        return 2;
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("socket");
        return 1;
    }
    int result = exchange_datagrams(fd, &server);
    if (close(fd) == -1) {
        perror("close");
        return 1;
    }
    return result;
}
