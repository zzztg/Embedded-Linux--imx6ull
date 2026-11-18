#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

static int child_read(int pipefd[2])
{
    if (close(pipefd[1]) == -1) {
        perror("child close unused write end");
        close(pipefd[0]);
        return 1;
    }

    char data[6];
    size_t total = 0;
    int result = 0;
    for (;;) {
        ssize_t count = read(pipefd[0], data, sizeof(data));
        if (count == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("child read");
            result = 1;
            break;
        }
        if (count == 0) {
            printf("child: read returned 0 (EOF), total=%zu\n", total);
            break;
        }
        total += (size_t)count;
        printf("child: read=%zd, total=%zu, data=\"%.*s\"\n",
               count, total, (int)count, data);
        if (fflush(stdout) == EOF) {
            perror("child fflush");
            result = 1;
            break;
        }
    }
    if (close(pipefd[0]) == -1) {
        perror("child close read end");
        result = 1;
    }
    return result;
}

int main(int argc, char *argv[])
{
    if (argc != 2 || (strcmp(argv[1], "normal") != 0 &&
                      strcmp(argv[1], "hold") != 0)) {
        fprintf(stderr, "Usage: %s normal|hold\n", argv[0]);
        return 2;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }
    printf("before fork: pid=%ld, read_fd=%d, write_fd=%d\n",
           (long)getpid(), pipefd[0], pipefd[1]);
    if (fflush(stdout) == EOF) {
        perror("fflush before fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }
    if (child_pid == 0) {
        return child_read(pipefd);
    }

    int result = 0;
    if (close(pipefd[0]) == -1) {
        perror("parent close unused read end");
        result = 1;
    }

    const char message[] = "ABCDEFGHIJKLMNOP";
    if (result == 0) {
        if (write_all(pipefd[1], message, sizeof(message) - 1) == -1) {
            perror("parent write");
            result = 1;
        } else {
            printf("parent: wrote %zu bytes to child pid=%ld\n",
                   sizeof(message) - 1, (long)child_pid);
        }
    }

    if (result == 0 && strcmp(argv[1], "hold") == 0) {
        printf("parent: write end still open; press ENTER to close it\n");
        if (fflush(stdout) == EOF) {
            perror("parent fflush");
            result = 1;
        } else if (getchar() == EOF) {
            fprintf(stderr, "parent: input ended; closing write end now\n");
        }
    }

    if (close(pipefd[1]) == -1) {
        perror("parent close write end");
        result = 1;
    } else {
        printf("parent: write end closed\n");
    }

    int status;
    pid_t waited;
    do {
        waited = waitpid(child_pid, &status, 0);
    } while (waited == -1 && errno == EINTR);
    if (waited == -1) {
        perror("waitpid");
        return 1;
    }
    printf("parent: reaped pid=%ld\n", (long)waited);
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        printf("parent: child exit code=%d\n", exit_code);
        if (exit_code != 0) {
            result = 1;
        }
    } else {
        if (WIFSIGNALED(status)) {
            printf("parent: child killed by signal=%d\n", WTERMSIG(status));
        }
        result = 1;
    }
    return result;
}
