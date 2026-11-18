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

static void show_events(const char *name, short events)
{
    printf("parent: %s revents=0x%x%s%s%s%s\n", name,
           (unsigned int)(unsigned short)events,
           (events & POLLIN) ? " POLLIN" : "",
           (events & POLLHUP) ? " POLLHUP" : "",
           (events & POLLERR) ? " POLLERR" : "",
           (events & POLLNVAL) ? " POLLNVAL" : "");
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <program-path> <stdout-file> <stderr-file>\n", argv[0]);
        return 2;
    }
    if (setvbuf(stdout, NULL, _IONBF, 0) != 0) {
        fprintf(stderr, "setvbuf failed\n");
        return 1;
    }

    const char *names[2] = {"stdout", "stderr"};
    int pipes[2][2] = {{-1, -1}, {-1, -1}};
    int outputs[2] = {-1, -1};
    struct pollfd watches[2] = {{.fd = -1}, {.fd = -1}};
    size_t totals[2] = {0, 0};
    pid_t child_pid = -1;
    int result = 1;

    for (int i = 0; i < 2; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            goto cleanup;
        }
        int flags = fcntl(pipes[i][0], F_GETFL);
        if (flags == -1 || fcntl(pipes[i][0], F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("set read end nonblocking");
            goto cleanup;
        }
        outputs[i] = open(argv[i + 2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (outputs[i] == -1) {
            perror("open capture file");
            goto cleanup;
        }
        printf("setup: %s read_fd=%d, write_fd=%d\n",
               names[i], pipes[i][0], pipes[i][1]);
    }

    child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        goto cleanup;
    }
    if (child_pid == 0) {
        fprintf(stderr, "child setup: pid=%ld; redirecting stdout and stderr\n",
                (long)getpid());
        /* In this lesson stdin/stdout/stderr are open, so all new fds are > 2. */
        if (dup2(pipes[0][1], STDOUT_FILENO) == -1 ||
            dup2(pipes[1][1], STDERR_FILENO) == -1) {
            perror("child dup2");
            _exit(126);
        }
        for (int i = 0; i < 2; i++) {
            if (close(pipes[i][0]) == -1 || close(pipes[i][1]) == -1 ||
                close(outputs[i]) == -1) {
                perror("child close unused fd");
                _exit(126);
            }
        }
        execl(argv[1], "stream_writer", (char *)NULL);
        perror("child execl");
        _exit(127);
    }

    printf("parent: pid=%ld, child_pid=%ld\n", (long)getpid(), (long)child_pid);
    for (int i = 0; i < 2; i++) {
        int closed = close(pipes[i][1]);
        pipes[i][1] = -1;
        if (closed == -1) {
            perror("parent close write end");
            goto cleanup;
        }
        watches[i].fd = pipes[i][0];
        watches[i].events = POLLIN;
    }

    result = 0;
    int active = 2;
    while (active > 0 && result == 0) {
        int ready = poll(watches, 2, 1000);
        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            result = 1;
            break;
        }
        if (ready == 0) {
            printf("parent: poll timeout, active channels=%d\n", active);
            continue;
        }
        printf("parent: poll returned %d, active channels=%d\n", ready, active);
        for (int i = 0; i < 2; i++) {
            if (watches[i].fd < 0 || watches[i].revents == 0) {
                continue;
            }
            short events = watches[i].revents;
            show_events(names[i], events);
            if (events & POLLNVAL) {
                fprintf(stderr, "parent: invalid %s descriptor\n", names[i]);
                result = 1;
                break;
            }
            if (events & (POLLIN | POLLHUP | POLLERR)) {
                char buffer[256];
                ssize_t count;
                do {
                    count = read(watches[i].fd, buffer, sizeof(buffer));
                } while (count == -1 && errno == EINTR);
                if (count == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("read capture pipe");
                        result = 1;
                        break;
                    }
                } else if (count == 0) {
                    int closed = close(watches[i].fd);
                    watches[i].fd = -1;
                    pipes[i][0] = -1;
                    active--;
                    printf("parent: %s EOF, total=%zu, active channels=%d\n",
                           names[i], totals[i], active);
                    if (closed == -1) {
                        perror("close completed read end");
                        result = 1;
                        break;
                    }
                } else {
                    if (write_all(outputs[i], buffer, (size_t)count) == -1) {
                        perror("write capture file");
                        result = 1;
                        break;
                    }
                    totals[i] += (size_t)count;
                    printf("parent: %s read=%zd, total=%zu\n", names[i], count, totals[i]);
                }
            }
            if (events & POLLERR) {
                fprintf(stderr, "parent: %s poll error\n", names[i]);
                result = 1;
                break;
            }
        }
    }
    if (result == 0) {
        printf("parent: both channels reached EOF\n");
    }

cleanup:
    for (int i = 0; i < 2; i++) {
        for (int end = 0; end < 2; end++) {
            if (pipes[i][end] >= 0 && close(pipes[i][end]) == -1) {
                perror("cleanup pipe");
                result = 1;
            }
        }
        if (outputs[i] >= 0 && close(outputs[i]) == -1) {
            perror("close capture file");
            result = 1;
        }
    }
    if (child_pid > 0) {
        if (result != 0 && kill(child_pid, SIGTERM) == -1 && errno != ESRCH) {
            perror("terminate child after capture error");
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
    }
    return result;
}
