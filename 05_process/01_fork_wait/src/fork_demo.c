#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    int number = 10;

    printf("before fork: pid=%ld, number=%d\n", (long)getpid(), number);
    /* Avoid copying pending stdout output into the child. */
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
        printf("child: pid=%ld, ppid=%ld, fork_return=%ld, number=%d\n",
               (long)getpid(), (long)getppid(), (long)child_pid, number);
        number = 99;
        printf("child: changed number=%d; exiting with 7\n", number);
        return 7;
    }

    printf("parent: pid=%ld, child_pid=%ld, number=%d\n",
           (long)getpid(), (long)child_pid, number);

    int status;
    pid_t waited;
    do {
        waited = waitpid(child_pid, &status, 0);
    } while (waited == -1 && errno == EINTR);

    if (waited == -1) {
        perror("waitpid");
        return 1;
    }

    printf("parent: waitpid returned %ld, number=%d\n", (long)waited, number);
    if (WIFEXITED(status)) {
        printf("parent: child exit code=%d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("parent: child killed by signal=%d\n", WTERMSIG(status));
    }

    return 0;
}
