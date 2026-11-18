#ifndef NET_COMMON_H
#define NET_COMMON_H

#include <errno.h>
#include <stddef.h>
#include <sys/socket.h>

#define TCP_PORT 8888

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

#endif
