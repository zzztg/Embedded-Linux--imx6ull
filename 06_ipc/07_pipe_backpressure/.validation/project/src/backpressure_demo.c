#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("set nonblocking");
        return -1;
    }
    return 0;
}

static int probe_writable(int fd, const char *label)
{
    struct pollfd watch = {.fd = fd, .events = POLLOUT, .revents = 0};
    int ready;
    do {
        ready = poll(&watch, 1, 0);
    } while (ready == -1 && errno == EINTR);
    if (ready == -1) {
        perror("poll write end");
        return -1;
    }
    printf("%s: poll=%d, POLLOUT=%s\n", label, ready,
           (watch.revents & POLLOUT) ? "yes" : "no");
    if (watch.revents & (POLLERR | POLLNVAL | POLLHUP)) {
        fprintf(stderr, "unexpected pipe event\n");
        return -1;
    }
    return (watch.revents & POLLOUT) ? 1 : 0;
}

int main(void)
{
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }
    int result = 1;
    if (set_nonblocking(pipefd[0]) == -1 || set_nonblocking(pipefd[1]) == -1) {
        goto cleanup;
    }
    if (probe_writable(pipefd[1], "1 initially empty") != 1) {
        goto cleanup;
    }

    char chunk[256];
    for (size_t i = 0; i < sizeof(chunk); i++) {
        chunk[i] = 'A';
    }
    size_t written = 0;
    for (;;) {
        ssize_t count = write(pipefd[1], chunk, sizeof(chunk));
        if (count == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("2 write would block: accepted_total=%zu bytes\n", written);
                break;
            }
            perror("fill pipe");
            goto cleanup;
        }
        if (count == 0) {
            fprintf(stderr, "unexpected zero-byte write\n");
            goto cleanup;
        }
        written += (size_t)count;
    }
    if (probe_writable(pipefd[1], "3 after write EAGAIN") != 0) {
        goto cleanup;
    }

    size_t drained = 0;
    for (;;) {
        char buffer[1024];
        ssize_t count = read(pipefd[0], buffer, sizeof(buffer));
        if (count == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("4 read would block: drained_total=%zu bytes\n", drained);
                break;
            }
            perror("drain pipe");
            goto cleanup;
        }
        if (count == 0) {
            fprintf(stderr, "unexpected EOF while write end is open\n");
            goto cleanup;
        }
        for (ssize_t i = 0; i < count; i++) {
            if (buffer[i] != 'A') {
                fprintf(stderr, "unexpected payload byte\n");
                goto cleanup;
            }
        }
        drained += (size_t)count;
    }
    if (written != drained) {
        fprintf(stderr, "written/drained byte counts differ\n");
        goto cleanup;
    }
    if (probe_writable(pipefd[1], "5 after draining") != 1) {
        goto cleanup;
    }

    ssize_t count;
    do {
        count = write(pipefd[1], "OK", 2);
    } while (count == -1 && errno == EINTR);
    if (count != 2) {
        fprintf(stderr, "unexpected retry write result: %zd\n", count);
        goto cleanup;
    }
    printf("6 retry write: returned %zd\n", count);
    char reply[2];
    do {
        count = read(pipefd[0], reply, sizeof(reply));
    } while (count == -1 && errno == EINTR);
    if (count != 2 || reply[0] != 'O' || reply[1] != 'K') {
        fprintf(stderr, "unexpected verification read\n");
        goto cleanup;
    }
    printf("7 verification read: returned %zd, data=\"%.*s\"\n", count, (int)count, reply);
    result = 0;

cleanup:
    if (close(pipefd[0]) == -1) {
        perror("close read end");
        result = 1;
    }
    if (close(pipefd[1]) == -1) {
        perror("close write end");
        result = 1;
    }
    return result;
}
