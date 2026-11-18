#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
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

static int child_write(int pipefd[2])
{
    if (close(pipefd[0]) == -1) {
        perror("child close read end");
        close(pipefd[1]);
        return 1;
    }
    printf("child: press ENTER to send data\n");
    if (getchar() == EOF) {
        fprintf(stderr, "child: stdin ended; sending now\n");
    }
    const char message[] = "ABCDEFGHIJKLMNOP";
    if (write_all(pipefd[1], message, sizeof(message) - 1) == -1) {
        perror("child write");
        close(pipefd[1]);
        return 1;
    }
    printf("child: sent 16 bytes; press ENTER again to close write end\n");
    if (getchar() == EOF) {
        fprintf(stderr, "child: stdin ended; closing write end now\n");
    }
    if (close(pipefd[1]) == -1) {
        perror("child close write end");
        return 1;
    }
    printf("child: write end closed\n");
    return 0;
}

static void show_events(int ready, short events)
{
    printf("parent: poll returned %d, revents=0x%x%s%s%s%s\n", ready,
           (unsigned int)(unsigned short)events,
           (events & POLLIN) ? " POLLIN" : "",
           (events & POLLHUP) ? " POLLHUP" : "",
           (events & POLLERR) ? " POLLERR" : "",
           (events & POLLNVAL) ? " POLLNVAL" : "");
}

int main(void)
{
    /* Make the observation messages visible before the program exits. */
    if (setvbuf(stdout, NULL, _IONBF, 0) != 0) {
        fprintf(stderr, "setvbuf failed\n");
        return 1;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }
    int flags = fcntl(pipefd[0], F_GETFL);
    if (flags == -1 || fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("set read end nonblocking");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }
    printf("before fork: pid=%ld, read_fd=%d, write_fd=%d\n",
           (long)getpid(), pipefd[0], pipefd[1]);

    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }
    if (child_pid == 0) {
        return child_write(pipefd);
    }

    int result = 0;
    int finished = 0;
    size_t total = 0;
    if (close(pipefd[1]) == -1) {
        perror("parent close write end");
        result = 1;
        finished = 1;
    }
    struct pollfd watch = {
        .fd = pipefd[0],
        .events = POLLIN,
        .revents = 0
    };

    while (!finished) {
        watch.revents = 0;
        int ready = poll(&watch, 1, 1000);
        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            result = 1;
            break;
        }
        if (ready == 0) {
            printf("parent: poll timeout; no event within 1000 ms\n");
            continue;
        }
        show_events(ready, watch.revents);
        if (watch.revents & POLLNVAL) {
            fprintf(stderr, "parent: invalid watched descriptor\n");
            result = 1;
            break;
        }

        if (watch.revents & (POLLIN | POLLHUP | POLLERR)) {
            for (;;) {
                char buffer[4];
                ssize_t count = read(pipefd[0], buffer, sizeof(buffer));
                if (count == -1) {
                    if (errno == EINTR) {
                        continue;
                    }
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf("parent: read would block; back to poll\n");
                        break;
                    }
                    perror("read");
                    result = 1;
                    finished = 1;
                    break;
                }
                if (count == 0) {
                    printf("parent: read EOF, total=%zu\n", total);
                    finished = 1;
                    break;
                }
                total += (size_t)count;
                printf("parent: read=%zd, total=%zu, data=\"%.*s\"\n",
                       count, total, (int)count, buffer);
            }
        }
        if (watch.revents & POLLERR) {
            fprintf(stderr, "parent: poll reported an error condition\n");
            result = 1;
            finished = 1;
        }
    }

    if (close(pipefd[0]) == -1) {
        perror("parent close read end");
        result = 1;
    }
    if (result != 0 && kill(child_pid, SIGTERM) == -1 && errno != ESRCH) {
        perror("terminate child after parent error");
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
