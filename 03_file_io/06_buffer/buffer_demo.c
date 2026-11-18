#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 2 ||
        (strcmp(argv[1], "buffered") != 0 &&
         strcmp(argv[1], "flushed") != 0)) {
        fprintf(stderr, "Usage: %s buffered|flushed\n", argv[0]);
        return 2;
    }

    /* stdout 重定向到普通文件时，通常使用全缓冲。 */
    if (printf("HELLO\n") < 0) {
        perror("printf");
        return 1;
    }

    if (strcmp(argv[1], "flushed") == 0) {
        if (fflush(stdout) == EOF) {
            perror("fflush");
            return 1;
        }
    }

    /* 日志通过 stderr 显示，确认上面的 printf 已执行。 */
    fprintf(stderr, "%s: printf completed; sleeping 30 seconds\n", argv[1]);
    sleep(30);

    /* 从 main 正常返回时，C 运行库会刷新尚未输出的流数据。 */
    return 0;
}
