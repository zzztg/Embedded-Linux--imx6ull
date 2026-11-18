#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int read_piece(const char *label, int fd)
{
    char data[4];
    off_t before = lseek(fd, 0, SEEK_CUR);
    if (before == (off_t)-1) {
        perror("lseek before");
        return 1;
    }
    ssize_t count;
    do {
        count = read(fd, data, sizeof(data));
    } while (count == -1 && errno == EINTR);
    if (count == -1) {
        perror("read");
        return 1;
    }
    off_t after = lseek(fd, 0, SEEK_CUR);
    if (after == (off_t)-1) {
        perror("lseek after");
        return 1;
    }
    printf("%s: fd=%d, offset=%lld -> %lld, read=%zd, data=\"%.*s\"\n",
           label, fd, (long long)before, (long long)after,
           count, (int)count, data);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
        return 2;
    }
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    int copy_fd = dup(fd);
    if (copy_fd == -1) {
        perror("dup");
        close(fd);
        return 1;
    }
    printf("original fd=%d, duplicate fd=%d\n", fd, copy_fd);
    if (read_piece("original", fd) != 0 ||
        read_piece("duplicate", copy_fd) != 0) {
        close(fd);
        close(copy_fd);
        return 1;
    }
    if (close(fd) == -1) {
        perror("close original");
        close(copy_fd);
        return 1;
    }
    fd = -1;
    printf("original closed; local fd=%d\n", fd);
    int result = read_piece("duplicate after close", copy_fd);
    if (close(copy_fd) == -1) {
        perror("close duplicate");
        result = 1;
    }
    return result;
}
