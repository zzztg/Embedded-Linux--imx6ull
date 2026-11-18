#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int write_all(int fd, const char *data, size_t length)
{
    while (length > 0) {
        ssize_t count = write(fd, data, length);

        if (count < 0) {
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

int main(int argc, char *argv[])
{
    const char message[] = "Hello from Linux file I/O!\n";
    int fd;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output-file>\n", argv[0]);
        return 2;
    }

    /* O_TRUNC 会清空已有文件；本实验使用学习目录里的 note.txt。 */
    fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    printf("open succeeded: fd=%d\n", fd);
    if (write_all(fd, message, sizeof(message) - 1) == -1) {
        perror("write");
        close(fd);
        return 1;
    }

    if (close(fd) == -1) {
        perror("close");
        return 1;
    }

    printf("Wrote %zu bytes to %s\n", sizeof(message) - 1, argv[1]);
    return 0;
}
