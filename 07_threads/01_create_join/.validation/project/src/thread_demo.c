#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void *worker(void *arg)
{
    int *number = arg;
    printf("worker: pid=%ld, initial number=%d\n", (long)getpid(), *number);
    *number = 99;
    printf("worker: changed number=%d; returning\n", *number);
    return NULL;
}

int main(void)
{
    int number = 10;
    pthread_t worker_thread;

    printf("main: pid=%ld, before create number=%d\n", (long)getpid(), number);

    int rc = pthread_create(&worker_thread, NULL, worker, &number);
    if (rc != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(rc));
        return 1;
    }

    /* Do not access number again until the worker has terminated. */
    rc = pthread_join(worker_thread, NULL);
    if (rc != 0) {
        fprintf(stderr, "pthread_join: %s\n", strerror(rc));
        return 1;
    }

    printf("main: after join number=%d\n", number);
    return 0;
}
