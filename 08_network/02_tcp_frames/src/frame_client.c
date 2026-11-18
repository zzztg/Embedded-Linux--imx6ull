#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include "frame_io.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server-ipv4>\n", argv[0]);
        return 2;
    }
    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(TCP_PORT);
    int parsed = inet_pton(AF_INET, argv[1], &server.sin_addr);
    if (parsed != 1) {
        if (parsed == -1) {
            perror("inet_pton");
        } else {
            fprintf(stderr, "client: invalid IPv4 address\n");
        }
        return 2;
    }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return 1;
    }
    if (connect(fd, (const struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("connect");
        close(fd);
        return 1;
    }
    printf("client: connected to %s:%d\n", argv[1], TCP_PORT);

    const char *messages[] = {"HELLO", "i.MX6ULL"};
    int status = 0;
    for (int i = 0; i < 2; ++i) {
        size_t expected = strlen(messages[i]);
        if (send_frame(fd, messages[i], expected) == -1) {
            perror("send_frame");
            status = 1;
            break;
        }
        char reply[MAX_PAYLOAD + 1];
        uint32_t length;
        int rc = recv_frame(fd, reply, sizeof(reply), &length);
        if (rc != 1) {
            if (rc == -1) {
                perror("recv_frame");
            } else {
                fprintf(stderr, "client: EOF before the expected reply frame\n");
            }
            status = 1;
            break;
        }
        int matches = length == expected && memcmp(reply, messages[i], expected) == 0;
        printf("client: frame=%d, length=%zu, matches=%s, text=\"%s\"\n",
               i + 1, (size_t)length, matches ? "yes" : "no", reply);
        if (!matches) {
            status = 2;
            break;
        }
    }
    if (close(fd) == -1) {
        perror("close connection");
        status = 1;
    }
    printf("client: connection closed\n");
    return status;
}
