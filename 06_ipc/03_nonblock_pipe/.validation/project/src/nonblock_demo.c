#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int observe_read(const char *label, int fd)
{
    char buffer[2];
    ssize_t count;
    do {
        count = read(fd, buffer, sizeof(buffer));
    } while (count == -1 && errno == EINTR);

    if (count == -1) {
        int error = errno;
        if (error == EAGAIN || error == EWOULDBLOCK) {
            printf("%s: read=-1, would block (EAGAIN/EWOULDBLOCK), errno=%d\n",
                   label, error);
            return 0;
        }
        errno = error;
        perror("read");
        return -1;
    }
    if (count == 0) {
        printf("%s: read=0 (EOF)\n", label);
    } else {
        printf("%s: read=%zd, data=\"%.*s\"\n",
               label, count, (int)count, buffer);
    }
    return 0;
}

static int write_all(int fd, const char *data, size_t length)
{
    while (length > 0) {
        ssize_t count = write(fd, data, length);
        if (count == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (count == 0) {
            errno = EIO;
            return -1;
        }
        data += (size_t)count;
        length -= (size_t)count;
    }
    return 0;
}

int main(void)
{
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }
    int result = 1;
    printf("pipe: read_fd=%d, write_fd=%d\n", pipefd[0], pipefd[1]);

    int flags = fcntl(pipefd[0], F_GETFL);
    if (flags == -1) {
        perror("F_GETFL read end");
        goto cleanup;
    }
    if (fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("F_SETFL O_NONBLOCK");
        goto cleanup;
    }
    int read_flags = fcntl(pipefd[0], F_GETFL);
    int write_flags = fcntl(pipefd[1], F_GETFL);
    if (read_flags == -1 || write_flags == -1) {
        fprintf(stderr, "could not inspect pipe status flags\n");
        goto cleanup;
    }
    printf("read nonblocking=%s, write nonblocking=%s\n",
           (read_flags & O_NONBLOCK) ? "yes" : "no",
           (write_flags & O_NONBLOCK) ? "yes" : "no");

    if (observe_read("1 empty, writer open", pipefd[0]) == -1) {
        goto cleanup;
    }

    const char message[] = "ABCD";
    if (write_all(pipefd[1], message, sizeof(message) - 1) == -1) {
        perror("write ABCD");
        goto cleanup;
    }
    printf("wrote 4 bytes: ABCD\n");

    if (observe_read("2 first data", pipefd[0]) == -1 ||
        observe_read("3 remaining data", pipefd[0]) == -1 ||
        observe_read("4 empty again, writer open", pipefd[0]) == -1) {
        goto cleanup;
    }

    if (close(pipefd[1]) == -1) {
        perror("close write end");
        pipefd[1] = -1;
        goto cleanup;
    }
    pipefd[1] = -1;
    printf("write end closed\n");
    if (observe_read("5 empty, no writers", pipefd[0]) == -1) {
        goto cleanup;
    }
    result = 0;

cleanup:
    if (close(pipefd[0]) == -1) {
        perror("close read end");
        result = 1;
    }
    if (pipefd[1] >= 0 && close(pipefd[1]) == -1) {
        perror("cleanup write end");
        result = 1;
    }
    return result;
}
