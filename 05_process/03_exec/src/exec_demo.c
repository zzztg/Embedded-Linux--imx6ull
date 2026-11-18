#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <program-path>\n", argv[0]);
        return 2;
    }

    printf("launcher: pid=%ld\n", (long)getpid());
    if (fflush(stdout) == EOF) {
        perror("fflush");
        return 1;
    }

    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        return 1;
    }

    if (child_pid == 0) {
        printf("child before exec: pid=%ld, ppid=%ld\n",
               (long)getpid(), (long)getppid());
        if (fflush(stdout) == EOF) {
            perror("fflush");
            _exit(126);
        }

        execl(argv[1], "args_demo", "hello", "i.MX6ULL", (char *)NULL);

        /* A successful exec never returns to this program. */
        perror("execl");
        _exit(127);
    }

    printf("parent: pid=%ld, child_pid=%ld\n", (long)getpid(), (long)child_pid);

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

    /* This teaching launcher reports the result without propagating it. */
    return 0;
}
