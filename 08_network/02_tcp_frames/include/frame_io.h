#ifndef FRAME_IO_H
#define FRAME_IO_H

#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#define TCP_PORT 8889
#define MAX_PAYLOAD 64
#define READ_STEP 3

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

/* 1: complete field; 0: EOF before any byte; -1: error or truncated field.
   Limit each read to three bytes only to make accumulation observable. */
static int read_exact(int fd, void *buffer, size_t length)
{
    char *data = buffer;
    size_t received = 0;
    while (received < length) {
        size_t request = length - received;
        if (request > READ_STEP) {
            request = READ_STEP;
        }
        ssize_t n = read(fd, data + received, request);
        if (n > 0) {
            received += (size_t)n;
            fprintf(stderr, "read_exact: got=%zd, progress=%zu/%zu\n",
                    n, received, length);
        } else if (n == 0) {
            if (received == 0) {
                return 0;
            }
            fprintf(stderr, "read_exact: EOF inside field (%zu/%zu bytes)\n",
                    received, length);
            errno = EPROTO;
            return -1;
        } else if (errno != EINTR) {
            return -1;
        }
    }
    return 1;
}

static int send_frame(int fd, const char *payload, size_t length)
{
    if (length > MAX_PAYLOAD) {
        errno = EMSGSIZE;
        return -1;
    }
    uint32_t network_length = htonl((uint32_t)length);
    if (send_all(fd, (const char *)&network_length, sizeof(network_length)) == -1) {
        return -1;
    }
    return send_all(fd, payload, length);
}

/* 1: one complete frame; 0: clean EOF at a frame boundary; -1: failure.
   payload needs room for the body and one local printing terminator. */
static int recv_frame(int fd, char *payload, size_t capacity, uint32_t *length)
{
    uint32_t network_length;
    int rc = read_exact(fd, &network_length, sizeof(network_length));
    if (rc != 1) {
        return rc;
    }

    uint32_t body_length = ntohl(network_length);
    if (body_length > MAX_PAYLOAD || (size_t)body_length >= capacity) {
        fprintf(stderr, "recv_frame: payload length %zu exceeds limit\n",
                (size_t)body_length);
        errno = EMSGSIZE;
        return -1;
    }
    rc = read_exact(fd, payload, body_length);
    if (rc != 1) {
        if (rc == 0) {
            fprintf(stderr, "recv_frame: EOF before the declared payload\n");
            errno = EPROTO;
        }
        return -1;
    }
    payload[body_length] = '\0';
    *length = body_length;
    return 1;
}

#endif
