#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include "net_common.h"

#define CLIENT_LIMIT 2

/* Return 1 to keep watching, 0 for EOF, -1 for a connection error. */
static int echo_once(int fd, int number, size_t *total)
{
    char buffer[64];
    /* Only this receive call is nonblocking; sends still use send_all. */
    ssize_t n = recv(fd, buffer, sizeof(buffer), MSG_DONTWAIT);
    if (n > 0) {
        if (send_all(fd, buffer, (size_t)n) == -1) {
            perror("send echo");
            return -1;
        }
        *total += (size_t)n;
        printf("server: client %d read=%zd; echoed_total=%zu\n", number, n, *total);
        return 1;
    }
    if (n == 0) {
        printf("server: client %d EOF\n", number);
        return 0;
    }
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        return 1;
    }
    perror("recv connection");
    return -1;
}

int main(void)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
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
    if (listen(listen_fd, 4) == -1) {
        perror("listen");
        close(listen_fd);
        return 1;
    }
    int flags = fcntl(listen_fd, F_GETFL);
    if (flags == -1 || fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("set listener nonblocking");
        close(listen_fd);
        return 1;
    }

    /* Index 0: listener. Indices 1 and 2: first and second accepted clients. */
    struct pollfd watches[CLIENT_LIMIT + 1];
    for (int i = 0; i <= CLIENT_LIMIT; ++i) {
        watches[i] = (struct pollfd){.fd = -1, .events = POLLIN, .revents = 0};
    }
    watches[0].fd = listen_fd;
    size_t totals[CLIENT_LIMIT + 1] = {0};
    int accepted = 0;
    int active = 0;
    int result = 0;
    printf("server: listening on 0.0.0.0:%d; listen_fd=%d; limit=%d clients\n",
           TCP_PORT, listen_fd, CLIENT_LIMIT);
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
            fprintf(stderr, "server: listener error; revents=0x%x\n",
                    (unsigned int)(unsigned short)watches[0].revents);
            result = 1;
            break;
        }
        if (watches[0].revents & POLLIN) {
            int fd = accept(watches[0].fd, NULL, NULL);
            if (fd >= 0) {
                ++accepted;
                ++active;
                watches[accepted] = (struct pollfd){
                    .fd = fd, .events = POLLIN, .revents = 0
                };
                printf("server: accepted client %d; connection_fd=%d; active=%d\n",
                       accepted, fd, active);
                if (accepted == CLIENT_LIMIT) {
                    if (close(watches[0].fd) == -1) {
                        perror("close listener");
                        result = 1;
                    }
                    watches[0].fd = -1;
                    watches[0].revents = 0;
                    printf("server: listener closed after accepting %d clients\n", accepted);
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
            int state = 1;
            if (watches[i].revents & POLLNVAL) {
                fprintf(stderr, "server: client %d has an invalid fd\n", i);
                state = -1;
            } else if (watches[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                /* One receive attempt per ready client in this poll round. */
                state = echo_once(watches[i].fd, i, &totals[i]);
            }
            if (state <= 0) {
                if (state == -1) {
                    result = 1;
                }
                if (close(watches[i].fd) == -1) {
                    perror("close connection");
                    result = 1;
                }
                watches[i].fd = -1;
                --active;
                printf("server: client %d closed; active=%d\n", i, active);
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
    printf("server: accepted=%d; echoed_total=%zu bytes; exiting\n",
           accepted, totals[1] + totals[2]);
    return result;
}
