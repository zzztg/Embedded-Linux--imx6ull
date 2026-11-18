#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include "net_common.h"

/* A teaching limit, unrelated to listen's backlog argument. */
#define CLIENT_LIMIT 2

static int serve_connection(int fd, int client_number, size_t *echoed)
{
    char buffer[64];
    *echoed = 0;
    for (;;) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n > 0) {
            printf("server: client %d read=%zd; echoing\n", client_number, n);
            fflush(stdout);
            if (send_all(fd, buffer, (size_t)n) == -1) {
                perror("send echo");
                return -1;
            }
            *echoed += (size_t)n;
        } else if (n == 0) {
            printf("server: client %d EOF; echoed=%zu bytes\n",
                   client_number, *echoed);
            return 0;
        } else if (errno != EINTR) {
            perror("read connection");
            return -1;
        }
    }
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

    printf("server: listening on 0.0.0.0:%d; listen_fd=%d; limit=%d clients\n",
           TCP_PORT, listen_fd, CLIENT_LIMIT);
    int result = 0;
    size_t total = 0;
    for (int client_number = 1; client_number <= CLIENT_LIMIT; ++client_number) {
        printf("server: waiting for client %d/%d; listen_fd=%d\n",
               client_number, CLIENT_LIMIT, listen_fd);
        fflush(stdout);

        int connection_fd;
        do {
            connection_fd = accept(listen_fd, NULL, NULL);
        } while (connection_fd == -1 && errno == EINTR);
        if (connection_fd == -1) {
            perror("accept");
            close(listen_fd);
            return 1;
        }
        printf("server: accepted client %d; connection_fd=%d\n",
               client_number, connection_fd);
        fflush(stdout);

        size_t echoed;
        if (serve_connection(connection_fd, client_number, &echoed) == -1) {
            result = 1;
        }
        total += echoed;
        if (close(connection_fd) == -1) {
            perror("close connection");
            result = 1;
        }
        printf("server: client %d finished; listener remains open\n", client_number);
    }

    if (close(listen_fd) == -1) {
        perror("close listener");
        result = 1;
    }
    printf("server: handled %d connections; echoed_total=%zu bytes; exiting\n",
           CLIENT_LIMIT, total);
    return result;
}
