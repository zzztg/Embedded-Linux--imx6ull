#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include "transfer_common.h"

#define CLIENT_LIMIT 2
#define SEND_ATTEMPTS_PER_ROUND 4

/* Initialized once before accepting connections; unchanged during transfers. */
static char response[RESPONSE_BYTES];

struct send_state {
    size_t length;
    size_t sent;
    unsigned int stalls;
    int resume_reported;
};

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* 1: all queued to the kernel; 0: pending; -1: connection error. */
static int try_send(int fd, int number, struct send_state *state)
{
    for (int attempt = 0; attempt < SEND_ATTEMPTS_PER_ROUND; ++attempt) {
        size_t remaining = state->length - state->sent;
        ssize_t n = send(fd, response + state->sent, remaining, MSG_NOSIGNAL);
        if (n > 0) {
            if (state->sent == 0) {
                printf("server: client %d first send requested=%zu, accepted=%zd\n",
                       number, remaining, n);
            }
            if (state->stalls > 0 && !state->resume_reported) {
                printf("server: client %d send resumed after EAGAIN\n", number);
                state->resume_reported = 1;
            }
            state->sent += (size_t)n;
            if (state->sent == state->length) {
                return 1;
            }
        } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            ++state->stalls;
            if (state->stalls == 1) {
                printf("server: client %d send would block; progress=%zu/%zu; back to poll\n",
                       number, state->sent, state->length);
            }
            return 0;
        } else if (n == -1 && errno == EINTR) {
            return 0;
        } else {
            if (n == 0) {
                errno = EIO;
            }
            perror("send response");
            return -1;
        }
    }
    /* Yield after a bounded amount of work, even if writes keep succeeding. */
    return 0;
}

int main(void)
{
    for (size_t i = 0; i < sizeof(response); ++i) {
        response[i] = (char)('A' + i % 26);
    }
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        return 1;
    }
    /* This server actively closes transfers; allow restart after TIME_WAIT. */
    int reuse_address = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse_address, sizeof(reuse_address)) == -1) {
        perror("set address reuse");
        close(listen_fd);
        return 1;
    }
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(TCP_PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listen_fd, (const struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        close(listen_fd);
        return 1;
    }
    if (listen(listen_fd, 4) == -1 || set_nonblocking(listen_fd) == -1) {
        perror("listen or nonblocking setup");
        close(listen_fd);
        return 1;
    }

    struct pollfd watches[CLIENT_LIMIT + 1];
    struct send_state states[CLIENT_LIMIT + 1] = {0};
    for (int i = 0; i <= CLIENT_LIMIT; ++i) {
        watches[i] = (struct pollfd){.fd = -1};
    }
    watches[0] = (struct pollfd){.fd = listen_fd, .events = POLLIN};
    int accepted = 0;
    int active = 0;
    int result = 0;
    printf("server: listening on 0.0.0.0:%d; response=%u bytes per client\n",
           TCP_PORT, RESPONSE_BYTES);
    fflush(stdout);

    while (accepted < CLIENT_LIMIT || active > 0) {
        int ready = poll(watches, CLIENT_LIMIT + 1, -1);
        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            result = 1;
            break;
        }
        if (watches[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "server: listener error\n");
            result = 1;
            break;
        }
        if (watches[0].revents & POLLIN) {
            int fd = accept(watches[0].fd, NULL, NULL);
            if (fd >= 0) {
                /* Small buffers make backpressure easy to observe. */
                int send_buffer_bytes = 4096;
                if (set_nonblocking(fd) == -1 ||
                    setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                               &send_buffer_bytes, sizeof(send_buffer_bytes)) == -1) {
                    perror("configure connection");
                    close(fd);
                    result = 1;
                    break;
                }
                ++accepted;
                ++active;
                states[accepted].length = sizeof(response);
                watches[accepted] = (struct pollfd){.fd = fd, .events = POLLOUT};
                printf("server: accepted client %d; fd=%d; pending=%zu; active=%d\n",
                       accepted, fd, states[accepted].length, active);
                if (accepted == CLIENT_LIMIT) {
                    if (close(watches[0].fd) == -1) {
                        perror("close listener");
                        result = 1;
                    }
                    watches[0].fd = -1;
                    watches[0].revents = 0;
                    printf("server: listener closed after 2 clients\n");
                }
            } else if (errno != EINTR && errno != EAGAIN &&
                       errno != EWOULDBLOCK && errno != ECONNABORTED) {
                perror("accept");
                result = 1;
                break;
            }
        }

        for (int i = 1; i <= CLIENT_LIMIT; ++i) {
            if (watches[i].fd < 0 || watches[i].revents == 0) {
                continue;
            }
            int state = 0;
            if (watches[i].revents & POLLNVAL) {
                fprintf(stderr, "server: client %d invalid fd\n", i);
                state = -1;
            } else if (watches[i].revents & (POLLOUT | POLLERR | POLLHUP)) {
                state = try_send(watches[i].fd, i, &states[i]);
            }
            if (state != 0) {
                if (state == -1) {
                    result = 1;
                }
                if (close(watches[i].fd) == -1) {
                    perror("close connection");
                    result = 1;
                }
                watches[i].fd = -1;
                --active;
                printf("server: client %d closed; queued=%zu/%zu; EAGAIN_count=%u; active=%d\n",
                       i, states[i].sent, states[i].length, states[i].stalls, active);
            }
        }
        fflush(stdout);
    }

    for (int i = 0; i <= CLIENT_LIMIT; ++i) {
        if (watches[i].fd >= 0 && close(watches[i].fd) == -1) {
            perror("close during cleanup");
            result = 1;
        }
    }
    printf("server: accepted=%d; queued_total=%zu bytes; exiting\n",
           accepted, states[1].sent + states[2].sent);
    return result;
}
