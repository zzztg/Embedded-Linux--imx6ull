#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include "frame_io.h"

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
    if (listen(listen_fd, 1) == -1) {
        perror("listen");
        close(listen_fd);
        return 1;
    }
    printf("server: listening on 0.0.0.0:%d; listen_fd=%d\n", TCP_PORT, listen_fd);
    fflush(stdout);

    int fd;
    do {
        fd = accept(listen_fd, NULL, NULL);
    } while (fd == -1 && errno == EINTR);
    if (fd == -1) {
        perror("accept");
        close(listen_fd);
        return 1;
    }
    if (close(listen_fd) == -1) {
        perror("close listener");
        close(fd);
        return 1;
    }
    printf("server: listener closed; connection_fd=%d remains open\n", fd);

    int frames = 0;
    size_t total = 0;
    int status = 0;
    char payload[MAX_PAYLOAD + 1];
    for (;;) {
        uint32_t length;
        int rc = recv_frame(fd, payload, sizeof(payload), &length);
        if (rc == 0) {
            printf("server: clean EOF; frames=%d, payload_bytes=%zu\n", frames, total);
            break;
        }
        if (rc == -1) {
            perror("recv_frame");
            status = 1;
            break;
        }
        frames++;
        total += length;
        printf("server: frame=%d, length=%zu; echoing complete frame\n",
               frames, (size_t)length);
        if (send_frame(fd, payload, length) == -1) {
            perror("send_frame");
            status = 1;
            break;
        }
    }
    if (close(fd) == -1) {
        perror("close connection");
        status = 1;
    }
    printf("server: connection closed; exiting\n");
    return status;
}
