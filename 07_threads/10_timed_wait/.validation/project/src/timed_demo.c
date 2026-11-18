#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int ready = 0;
static int value = 0;

struct wait_result {
    int got_data;
    int value;
};

static void check_pthread(int rc, const char *operation)
{
    if (rc != 0) {
        fprintf(stderr, "%s: %s\n", operation, strerror(rc));
        exit(1);
    }
}

static void *worker(void *arg)
{
    struct wait_result *result = arg;
    struct timespec deadline;

    check_pthread(pthread_mutex_lock(&mutex), "worker mutex_lock");
    if (clock_gettime(CLOCK_REALTIME, &deadline) == -1) {
        perror("clock_gettime");
        exit(1);
    }
    deadline.tv_sec += 3;

    while (!ready) {
        printf("worker: ready=0; timed wait with the original 3-second deadline\n");
        fflush(stdout);
        int rc = pthread_cond_timedwait(&changed, &mutex, &deadline);
        if (rc == ETIMEDOUT) {
            printf("worker: ETIMEDOUT; mutex reacquired; ready=%d\n", ready);
            /* Even on timeout, inspect the predicate while holding mutex. */
            break;
        }
        check_pthread(rc, "pthread_cond_timedwait");
        printf("worker: wait returned 0; ready=%d; rechecking\n", ready);
    }

    if (ready) {
        result->value = value;
        result->got_data = 1;
        printf("worker: received value=%d\n", result->value);
    } else {
        printf("worker: deadline reached without data; ending this wait\n");
    }

    check_pthread(pthread_mutex_unlock(&mutex), "worker mutex_unlock");
    return NULL;
}

int main(int argc, char *argv[])
{
    int prepared = 0;
    pthread_t worker_thread;
    struct wait_result result = { .got_data = 0, .value = 0 };

    if (argc == 2 && strcmp(argv[1], "ready") == 0) {
        prepared = 1;
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [ready]\n", argv[0]);
        return 2;
    }

    if (prepared) {
        /* No worker exists yet, so these initial writes do not need mutex. */
        value = 42;
        ready = 1;
        printf("main: data prepared before creating worker\n");
    } else {
        printf("main: no data will be published; observing a timeout\n");
    }

    check_pthread(pthread_create(&worker_thread, NULL, worker, &result),
                  "pthread_create");
    check_pthread(pthread_join(worker_thread, NULL), "pthread_join");

    if (result.got_data) {
        printf("main: joined; outcome=data, value=%d\n", result.value);
    } else {
        printf("main: joined; outcome=timeout\n");
    }

    check_pthread(pthread_cond_destroy(&changed), "cond_destroy");
    check_pthread(pthread_mutex_destroy(&mutex), "mutex_destroy");
    printf("main: synchronization objects destroyed\n");

    if (result.got_data != prepared) {
        return 2;
    }
    return result.got_data && result.value != 42 ? 2 : 0;
}
