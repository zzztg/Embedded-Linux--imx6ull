#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include "udp_common.h"

static int echo_datagrams(int fd)
{
    int echoed = 0;
    size_t total = 0;
    while (echoed < DATAGRAM_COUNT) {
        unsigned char buffer[PAYLOAD_CAPACITY];
        struct sockaddr_in peer = {0};
        socklen_t peer_length = sizeof(peer);
        ssize_t n = recvfrom(fd, buffer, sizeof(buffer), MSG_TRUNC,
                             (struct sockaddr *)&peer, &peer_length);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("recvfrom");
            return 1;
        }
        if ((size_t)n > sizeof(buffer)) {
            printf("server: dropped oversized datagram; length=%zd; capacity=%zu\n",
                   n, sizeof(buffer));
            fflush(stdout);
            continue;
        }

        char peer_ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &peer.sin_addr, peer_ip, sizeof(peer_ip)) == NULL) {
            perror("inet_ntop");
            return 1;
        }
        if (send_datagram(fd, buffer, (size_t)n, &peer) == -1) {
            perror("sendto echo");
            return 1;
        }
        ++echoed;
        total += (size_t)n;
        printf("server: datagram %d from %s:%u; length=%zd%s; echoed\n",
               echoed, peer_ip, (unsigned)ntohs(peer.sin_port), n,
               n == 0 ? " (empty)" : "");
        fflush(stdout);
    }
    printf("server: echoed_datagrams=%d; payload_total=%zu; exiting\n", echoed, total);
    return 0;
}

int main(void)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("socket");
        return 1;
    }
    struct sockaddr_in local = {0};
    local.sin_family = AF_INET;
    local.sin_port = htons(UDP_PORT);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (const struct sockaddr *)&local, sizeof(local)) == -1) {
        perror("bind");
        close(fd);
        return 1;
    }
    printf("server: UDP bound to 0.0.0.0:%d; fd=%d\n", UDP_PORT, fd);
    fflush(stdout);

    int result = echo_datagrams(fd);
    if (close(fd) == -1) {
        perror("close");
        return 1;
    }
    return result;
}
