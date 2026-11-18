#define _POSIX_C_SOURCE 200809L

#include "io_utils.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    const char message[] = "Hello from Linux file I/O!\n";

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output-file>\n", argv[0]);
        return 2;
    }

    /* 本节使用独立实验文件 app_note.txt，已有内容会被清空。 */
    int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    if (write_all(fd, message, sizeof(message) - 1) == -1) {
        perror("write");
        close(fd);
        return 1;
    }

    if (close(fd) == -1) {
        perror("close");
        return 1;
    }

    printf("Wrote %zu bytes using io_utils.c\n", sizeof(message) - 1);
    return 0;
}
