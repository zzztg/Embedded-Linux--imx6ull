#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include "echo_common.h"

#define CLIENT_LIMIT 2

struct connection {
    unsigned char output[OUTPUT_CAPACITY];
    size_t length;
    size_t sent;
    int input_eof;
    size_t received;
    size_t queued;
    unsigned int full_count;
};

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* The only input to this decision is the connection's current state. */
static short wanted_events(const struct connection *client)
{
    short events = 0;
    if (!client->input_eof && client->length < sizeof(client->output)) {
        events |= POLLIN;
    }
    if (client->sent < client->length) {
        events |= POLLOUT;
    }
    return events;
}

/* 0: keep watching; 1: input EOF and output drained; -1: connection error. */
static int handle_client(int fd, short events, int number, struct connection *client)
{
    if (events & POLLNVAL) {
        fprintf(stderr, "server: client %d invalid fd\n", number);
        return -1;
    }
    if (!client->input_eof && client->length < sizeof(client->output) &&
        (events & (POLLIN | POLLHUP | POLLERR))) {
        ssize_t n = recv(fd, client->output + client->length,
                         sizeof(client->output) - client->length, 0);
        if (n > 0) {
            client->length += (size_t)n;
            client->received += (size_t)n;
            if (client->length == sizeof(client->output)) {
                ++client->full_count;
                if (client->full_count == 1) {
                    printf("server: client %d output buffer full; pause receiving\n", number);
                }
            }
        } else if (n == 0) {
            client->input_eof = 1;
            printf("server: client %d input EOF; pending=%zu\n",
                   number, client->length - client->sent);
        } else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recv connection");
            return -1;
        }
    }

    if (client->sent < client->length &&
        (events & (POLLOUT | POLLHUP | POLLERR))) {
        ssize_t n = send(fd, client->output + client->sent,
                         client->length - client->sent, MSG_NOSIGNAL);
        if (n > 0) {
            client->sent += (size_t)n;
            client->queued += (size_t)n;
            if (client->sent == client->length) {
                if (client->input_eof) {
                    printf("server: client %d output drained after EOF\n", number);
                }
                client->sent = 0;
                client->length = 0;
            }
        } else if (n == 0) {
            errno = EIO;
            perror("send made no progress");
            return -1;
        } else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("send echo");
            return -1;
        }
    }
    return client->input_eof && client->sent == client->length ? 1 : 0;
}

int main(void)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        return 1;
    }
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
    struct connection clients[CLIENT_LIMIT + 1] = {0};
    for (int i = 0; i <= CLIENT_LIMIT; ++i) {
        watches[i] = (struct pollfd){.fd = -1};
    }
    watches[0] = (struct pollfd){.fd = listen_fd, .events = POLLIN};
    int accepted = 0;
    int active = 0;
    int result = 0;
    printf("server: listening on 0.0.0.0:%d; output capacity=%u per client\n",
           TCP_PORT, OUTPUT_CAPACITY);
    fflush(stdout);

    while (accepted < CLIENT_LIMIT || active > 0) {
        for (int i = 1; i <= CLIENT_LIMIT; ++i) {
            if (watches[i].fd >= 0) {
                watches[i].events = wanted_events(&clients[i]);
            }
        }
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
                watches[accepted] = (struct pollfd){.fd = fd, .events = POLLIN};
                printf("server: accepted client %d; fd=%d; active=%d\n", accepted, fd, active);
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
            int state = handle_client(watches[i].fd, watches[i].revents, i, &clients[i]);
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
                printf("server: client %d closed; received=%zu queued=%zu full_count=%u active=%d\n",
                       i, clients[i].received, clients[i].queued, clients[i].full_count, active);
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
    printf("server: accepted=%d; received_total=%zu queued_total=%zu; exiting\n",
           accepted, clients[1].received + clients[2].received,
           clients[1].queued + clients[2].queued);
    return result;
}
