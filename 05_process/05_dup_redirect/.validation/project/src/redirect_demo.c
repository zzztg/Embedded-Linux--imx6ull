#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output-file>\n", argv[0]);
        return 2;
    }
    printf("stdout: before redirection\n");
    if (fflush(stdout) == EOF) {
        perror("flush before redirection");
        return 1;
    }

    int file_fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1) {
        perror("open");
        return 1;
    }
    int saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout == -1) {
        perror("save stdout");
        close(file_fd);
        return 1;
    }
    fprintf(stderr, "setup: file_fd=%d, saved_stdout=%d\n", file_fd, saved_stdout);

    if (dup2(file_fd, STDOUT_FILENO) == -1) {
        perror("redirect stdout");
        close(file_fd);
        close(saved_stdout);
        return 1;
    }
    if (close(file_fd) == -1) {
        perror("close file_fd");
        close(saved_stdout);
        return 1;
    }

    if (printf("stdout: this line goes to the file.\n") < 0 ||
        fflush(stdout) == EOF) {
        perror("write redirected stdout");
        close(saved_stdout);
        return 1;
    }
    fprintf(stderr, "stderr: still at original destination\n");

    if (dup2(saved_stdout, STDOUT_FILENO) == -1) {
        perror("restore stdout");
        close(saved_stdout);
        return 1;
    }
    if (close(saved_stdout) == -1) {
        perror("close saved_stdout");
        return 1;
    }
    printf("stdout: restored to original destination\n");
    return 0;
}
