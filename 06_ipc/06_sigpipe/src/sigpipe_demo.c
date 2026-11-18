#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t caught_signal = 0;

static void handle_sigpipe(int signum)
{
    caught_signal = signum;
}

int main(int argc, char *argv[])
{
    if (argc != 2 || (strcmp(argv[1], "default") != 0 &&
                      strcmp(argv[1], "ignore") != 0 &&
                      strcmp(argv[1], "catch") != 0)) {
        fprintf(stderr, "Usage: %s default|ignore|catch\n", argv[0]);
        return 2;
    }

    struct sigaction action = {0};
    if (sigemptyset(&action.sa_mask) == -1) {
        perror("sigemptyset");
        return 1;
    }
    if (strcmp(argv[1], "ignore") == 0) {
        action.sa_handler = SIG_IGN;
    } else if (strcmp(argv[1], "catch") == 0) {
        action.sa_handler = handle_sigpipe;
    } else {
        action.sa_handler = SIG_DFL;
    }
    if (sigaction(SIGPIPE, &action, NULL) == -1) {
        perror("sigaction SIGPIPE");
        return 1;
    }

    /* Make the demonstration independent of an inherited blocked SIGPIPE. */
    sigset_t signals;
    if (sigemptyset(&signals) == -1 || sigaddset(&signals, SIGPIPE) == -1 ||
        sigprocmask(SIG_UNBLOCK, &signals, NULL) == -1) {
        perror("unblock SIGPIPE");
        return 1;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }
    if (close(pipefd[0]) == -1) {
        perror("close read end");
        close(pipefd[1]);
        return 1;
    }

    printf("mode=%s: all read ends closed; writing 1 byte (SIGPIPE=%d)\n",
           argv[1], SIGPIPE);
    if (fflush(stdout) == EOF) {
        perror("fflush before write");
        close(pipefd[1]);
        return 1;
    }

    const char byte = 'X';
    ssize_t count;
    do {
        count = write(pipefd[1], &byte, 1);
    } while (count == -1 && errno == EINTR);
    int error = count == -1 ? errno : 0;

    /* With an unblocked default SIGPIPE, execution never reaches this line. */
    printf("after write: returned %zd\n", count);
    if (count == -1) {
        printf("saved errno=%d (%s)\n", error, strerror(error));
    }
    printf("caught_signal=%d\n", (int)caught_signal);

    if (close(pipefd[1]) == -1) {
        perror("close write end");
        return 1;
    }
    if (count == -1 && error == EPIPE) {
        printf("EPIPE: no readers; stop sending on this pipe\n");
        return 1;
    }
    fprintf(stderr, "unexpected write result in this experiment\n");
    return 2;
}
