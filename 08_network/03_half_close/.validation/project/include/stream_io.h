#ifndef STREAM_IO_H
#define STREAM_IO_H

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define TCP_PORT 8890
#define MAX_REQUEST 64

static int send_all(int fd, const char *data, size_t length)
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

/* 0: received through EOF; -1: error or capacity exceeded.
   This helper waits for the peer's send-side EOF, not for a fixed length. */
static int receive_until_eof(int fd, char *data, size_t capacity,
                             size_t *length, const char *role)
{
    size_t total = 0;
    char chunk[32];
    for (;;) {
        ssize_t n = read(fd, chunk, sizeof(chunk));
        if (n > 0) {
            if ((size_t)n > capacity - total) {
                fprintf(stderr, "%s: data exceeds %zu-byte limit\n", role, capacity);
                errno = EMSGSIZE;
                return -1;
            }
            memcpy(data + total, chunk, (size_t)n);
            total += (size_t)n;
            printf("%s: read=%zd, total=%zu\n", role, n, total);
            fflush(stdout);
        } else if (n == 0) {
            *length = total;
            printf("%s: EOF; total=%zu\n", role, total);
            return 0;
        } else if (errno != EINTR) {
            return -1;
        }
    }
}

#endif
