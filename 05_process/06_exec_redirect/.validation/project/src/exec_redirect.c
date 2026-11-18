#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <program-path> <output-file>\n", argv[0]);
        return 2;
    }

    printf("parent before fork: pid=%ld\n", (long)getpid());
    if (fflush(stdout) == EOF) {
        perror("fflush before fork");
        return 1;
    }

    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        return 1;
    }

    if (child_pid == 0) {
        fprintf(stderr, "child setup: pid=%ld, redirecting stdout to %s\n",
                (long)getpid(), argv[2]);

        int file_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file_fd == -1) {
            perror("child open output");
            _exit(126);
        }
        if (dup2(file_fd, STDOUT_FILENO) == -1) {
            perror("child dup2");
            close(file_fd);
            _exit(126);
        }
        if (file_fd != STDOUT_FILENO && close(file_fd) == -1) {
            perror("child close file_fd");
            _exit(126);
        }

        execl(argv[1], "args_demo", "hello", "i.MX6ULL", (char *)NULL);
        perror("child execl");
        _exit(127);
    }

    printf("parent: child_pid=%ld; stdout still at original destination\n",
           (long)child_pid);

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
        printf("parent: child exit code=%d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("parent: child killed by signal=%d\n", WTERMSIG(status));
    }
    /* Report the child's result; do not propagate it in this teaching launcher. */
    return 0;
}
