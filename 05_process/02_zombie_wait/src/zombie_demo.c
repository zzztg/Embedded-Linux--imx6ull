#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        return 1;
    }

    if (child_pid == 0) {
        printf("child: pid=%ld; exiting with 7\n", (long)getpid());
        return 7;
    }

    printf("parent: pid=%ld, child_pid=%ld\n", (long)getpid(), (long)child_pid);
    printf("inspect in another terminal: cat /proc/%ld/status\n", (long)child_pid);
    printf("parent: press ENTER here to call waitpid...\n");
    fflush(stdout);

    /* Deliberately delay reaping while the user inspects /proc. */
    if (getchar() == EOF) {
        fprintf(stderr, "parent: input ended; collecting child now\n");
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
        printf("parent: child exit code=%d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("parent: child killed by signal=%d\n", WTERMSIG(status));
    }
    return 0;
}
