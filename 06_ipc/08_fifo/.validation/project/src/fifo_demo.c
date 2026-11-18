#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int write_all(int fd, const char *data, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        ssize_t n = write(fd, data + sent, length - sent);
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

int main(int argc, char *argv[])
{
    if (argc != 3 || (strcmp(argv[1], "create") != 0
                     && strcmp(argv[1], "read") != 0
                     && strcmp(argv[1], "write") != 0)) {
        fprintf(stderr, "Usage: %s create|read|write <fifo-path>\n", argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "create") == 0) {
        if (mkfifo(argv[2], 0600) == -1) {
            perror("mkfifo");
            return 1;
        }
        printf("created FIFO: %s\n", argv[2]);
        return 0;
    }

    int writing = strcmp(argv[1], "write") == 0;
    const char *role = writing ? "writer" : "reader";
    fprintf(stderr, "%s: opening FIFO (may block until peer opens)\n", role);
    int fd = open(argv[2], writing ? O_WRONLY : O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    struct stat info;
    if (fstat(fd, &info) == -1) {
        perror("fstat");
        close(fd);
        return 1;
    }
    if (!S_ISFIFO(info.st_mode)) {
        fprintf(stderr, "%s: requested path is not a FIFO\n", role);
        close(fd);
        return 1;
    }
    fprintf(stderr, "%s: open succeeded; fd=%d\n", role, fd);

    int status = 0;
    if (writing) {
        const char message[] = "Hello through FIFO!\n";
        if (write_all(fd, message, sizeof(message) - 1) == -1) {
            perror("write FIFO");
            status = 1;
        } else {
            fprintf(stderr, "writer: sent %zu bytes; closing write end\n",
                    sizeof(message) - 1);
        }
    } else {
        char buffer[64];
        size_t total = 0;
        for (;;) {
            ssize_t n = read(fd, buffer, sizeof(buffer));
            if (n > 0) {
                if (write_all(STDOUT_FILENO, buffer, (size_t)n) == -1) {
                    perror("write stdout");
                    status = 1;
                    break;
                }
                total += (size_t)n;
            } else if (n == 0) {
                fprintf(stderr, "reader: EOF; received %zu bytes\n", total);
                break;
            } else if (errno != EINTR) {
                perror("read FIFO");
                status = 1;
                break;
            }
        }
    }

    if (close(fd) == -1) {
        perror("close");
        status = 1;
    }
    return status;
}
