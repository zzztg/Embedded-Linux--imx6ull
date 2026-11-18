#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
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

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <program-path>\n", argv[0]);
        return 2;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }
    fprintf(stderr, "parent before fork: pid=%ld, read_fd=%d, write_fd=%d\n",
            (long)getpid(), pipefd[0], pipefd[1]);

    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    if (child_pid == 0) {
        fprintf(stderr, "child setup: pid=%ld; stdout goes to pipe\n",
                (long)getpid());
        if (close(pipefd[0]) == -1) {
            perror("child close read end");
            _exit(126);
        }
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("child dup2");
            close(pipefd[1]);
            _exit(126);
        }
        if (pipefd[1] != STDOUT_FILENO && close(pipefd[1]) == -1) {
            perror("child close original write end");
            _exit(126);
        }
        execl(argv[1], "args_demo", "hello", "i.MX6ULL", (char *)NULL);
        perror("child execl");
        _exit(127);
    }

    int result = 0;
    if (close(pipefd[1]) == -1) {
        perror("parent close write end");
        result = 1;
    }
    fprintf(stderr, "parent: reading stdout of child pid=%ld\n", (long)child_pid);

    char buffer[32];
    size_t total = 0;
    for (;;) {
        ssize_t count = read(pipefd[0], buffer, sizeof(buffer));
        if (count == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("parent read");
            result = 1;
            break;
        }
        if (count == 0) {
            fprintf(stderr, "parent: EOF, captured %zu bytes\n", total);
            break;
        }
        total += (size_t)count;
        if (write_all(STDOUT_FILENO, buffer, (size_t)count) == -1) {
            perror("parent replay stdout");
            result = 1;
            break;
        }
    }
    if (close(pipefd[0]) == -1) {
        perror("parent close read end");
        result = 1;
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
    fprintf(stderr, "parent: reaped pid=%ld\n", (long)waited);
    if (WIFEXITED(status)) {
        fprintf(stderr, "parent: child exit code=%d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "parent: child killed by signal=%d\n", WTERMSIG(status));
    }
    /* Report the child status, while returning only the capture result here. */
    return result;
}
