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
            perror("write");
            return -1;
        }
        if (count == 0) {
            errno = EIO;
            perror("write");
            return -1;
        }
        data += (size_t)count;
        length -= (size_t)count;
    }
    return 0;
}

static int seek_report(int fd, off_t offset, int whence, const char *label)
{
    off_t position = lseek(fd, offset, whence);
    if (position == (off_t)-1) {
        perror("lseek");
        return -1;
    }
    printf("%s: offset=%lld\n", label, (long long)position);
    return 0;
}

static int read_preview(int fd, const char *label)
{
    /* 本例只展示实验文件中的文本：读最多 4 字节，预留 1 字节补零。 */
    char buffer[5];
    ssize_t count;

    do {
        count = read(fd, buffer, sizeof(buffer) - 1);
    } while (count == -1 && errno == EINTR);

    if (count == -1) {
        perror("read");
        return -1;
    }
    buffer[(size_t)count] = '\0';
    printf("%s: %s (%zd bytes)\n", label, buffer, count);
    return 0;
}

static int run_demo(int fd)
{
    /* 按顺序执行；任何一步失败就停止后续操作，交给 main 关闭文件。 */
    if (seek_report(fd, 0, SEEK_CUR, "initial") == -1 ||
        read_preview(fd, "first read") == -1 ||
        seek_report(fd, 0, SEEK_CUR, "after read") == -1 ||
        seek_report(fd, 2, SEEK_SET, "move to byte 2") == -1 ||
        write_all(fd, "xy", 2) == -1 ||
        seek_report(fd, 0, SEEK_CUR, "after overwrite") == -1 ||
        seek_report(fd, 0, SEEK_END, "file end") == -1 ||
        seek_report(fd, 0, SEEK_SET, "rewind") == -1 ||
        read_preview(fd, "second read") == -1) {
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <study-file-to-modify>\n", argv[0]);
        return 2;
    }

    /* 不创建、不清空：本实验先用 Shell 准备 seek_demo.txt。 */
    int fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    int result = run_demo(fd);
    if (close(fd) == -1) {
        perror("close");
        result = 1;
    }
    return result;
}
