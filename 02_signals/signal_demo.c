#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static volatile sig_atomic_t stop_signal = 0;

/* 信号处理函数只记录信号；打印和退出由主流程完成。 */
static void handle_stop(int signo)
{
    stop_signal = signo;
}

int main(void)
{
    struct sigaction action = {0};

    action.sa_handler = handle_stop;
    if (sigemptyset(&action.sa_mask) == -1 ||
        sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1) {
        perror("install signal handler");
        return 1;
    }

    printf("PID=%ld; waiting for SIGINT or SIGTERM...\n", (long)getpid());
    fflush(stdout);

    while (stop_signal == 0) {
        sleep(1);
    }

    printf("Received signal %d; exiting normally.\n", (int)stop_signal);
    return 0;
}
