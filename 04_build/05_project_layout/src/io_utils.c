#define _POSIX_C_SOURCE 200809L

#include "io_utils.h"

#include <errno.h>
#include <unistd.h>

/* 函数定义放在一个 .c 文件中，供其他模块调用。 */
int write_all(int fd, const char *data, size_t length)
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
