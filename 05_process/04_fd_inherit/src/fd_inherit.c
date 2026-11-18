#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int read_piece(const char *who, int fd)
{
    char data[4];
    off_t before = lseek(fd, 0, SEEK_CUR);
    if (before == (off_t)-1) {
        perror("lseek before read");
        return 1;
    }

    ssize_t count;
    do {
        count = read(fd, data, sizeof(data));
    } while (count == -1 && errno == EINTR);
    if (count == -1) {
        perror("read");
        return 1;
    }

    off_t after = lseek(fd, 0, SEEK_CUR);
    if (after == (off_t)-1) {
        perror("lseek after read");
        return 1;
    }
    printf("%s: fd=%d, offset=%lld -> %lld, read=%zd, data=\"%.*s\"\n",
           who, fd, (long long)before, (long long)after,
           count, (int)count, data);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3 || (strcmp(argv[1], "shared") != 0 &&
                      strcmp(argv[1], "separate") != 0)) {
        fprintf(stderr, "Usage: %s shared|separate <input-file>\n", argv[0]);
        return 2;
    }

    int shared = strcmp(argv[1], "shared") == 0;
    int fd = -1;
    if (shared) {
        fd = open(argv[2], O_RDONLY);
        if (fd == -1) {
            perror("open before fork");
            return 1;
        }
    }

    printf("mode=%s, before fork: pid=%ld, fd=%d\n",
           argv[1], (long)getpid(), fd);
    if (fflush(stdout) == EOF) {
        perror("fflush");
        if (fd >= 0) {
            close(fd);
        }
        return 1;
    }

    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        if (fd >= 0) {
            close(fd);
        }
        return 1;
    }

    if (child_pid == 0) {
        if (!shared) {
            fd = open(argv[2], O_RDONLY);
            if (fd == -1) {
                perror("child open");
                return 1;
            }
        }

        int result = read_piece("child", fd);
        if (close(fd) == -1) {
            perror("child close");
            result = 1;
        }
        fd = -1;
        printf("child: closed its fd; local fd=%d\n", fd);
        return result;
    }

    int status;
    pid_t waited;
    do {
        waited = waitpid(child_pid, &status, 0);
    } while (waited == -1 && errno == EINTR);
    if (waited == -1) {
        perror("waitpid");
        if (fd >= 0) {
            close(fd);
        }
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "parent: child did not finish successfully\n");
        if (fd >= 0) {
            close(fd);
        }
        return 1;
    }

    printf("parent: child reaped; local fd=%d\n", fd);
    if (!shared) {
        fd = open(argv[2], O_RDONLY);
        if (fd == -1) {
            perror("parent open");
            return 1;
        }
        printf("parent: opened input separately; fd=%d\n", fd);
    }

    int result = read_piece("parent", fd);
    if (close(fd) == -1) {
        perror("parent close");
        result = 1;
    }
    return result;
}
