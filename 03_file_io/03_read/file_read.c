#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

/* 沿用写入实验：只发送指定长度的字节，并处理短写。 */
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
    char buffer[16];
    int result = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
        return 2;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    for (;;) {
        ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            result = 1;
            break;
        }

        /* 观察信息走标准错误；文件的原始字节走标准输出。 */
        fprintf(stderr, "read returned %zd\n", count);
        if (count == 0) {
            break;
        }

        /* 本次只有 count 字节有效，不把 buffer 当 C 字符串。 */
        if (write_all(STDOUT_FILENO, buffer, (size_t)count) == -1) {
            perror("write stdout");
            result = 1;
            break;
        }
    }

    if (close(fd) == -1) {
        perror("close");
        result = 1;
    }
    return result;
}
