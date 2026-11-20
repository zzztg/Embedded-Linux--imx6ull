#ifndef UDP_COMMON_H
#define UDP_COMMON_H

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define UDP_PORT 8895
#define PAYLOAD_CAPACITY 64
#define DATAGRAM_COUNT 3

/* A successful call sends one entire datagram, even when length is zero. */
static inline int send_datagram(int fd, const void *data, size_t length,
                                const struct sockaddr_in *destination)
{
    ssize_t n;
    do {
        n = sendto(fd, data, length, 0, (const struct sockaddr *)destination,
                   sizeof(*destination));
    } while (n == -1 && errno == EINTR);

    if (n == -1) {
        return -1;
    }
    if ((size_t)n != length) {
        errno = EIO;
        return -1;
    }
    return 0;
}

#endif
