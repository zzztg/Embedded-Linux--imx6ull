#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/* Storage only: initialize_condition() must run before any use. */
static pthread_cond_t changed;
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

static void initialize_condition(void)
{
    pthread_condattr_t attr;

    check_pthread(pthread_condattr_init(&attr), "condattr_init");
    check_pthread(pthread_condattr_setclock(&attr, CLOCK_MONOTONIC),
                  "condattr_setclock");
    check_pthread(pthread_cond_init(&changed, &attr), "cond_init");
    check_pthread(pthread_condattr_destroy(&attr), "condattr_destroy");

    printf("main: condition initialized with CLOCK_MONOTONIC; attributes destroyed\n");
}

static void *worker(void *arg)
{
    struct wait_result *result = arg;
    struct timespec deadline;

    check_pthread(pthread_mutex_lock(&mutex), "worker mutex_lock");
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) == -1) {
        perror("clock_gettime");
        exit(1);
    }
    deadline.tv_sec += 3;

    while (!ready) {
        printf("worker: ready=0; waiting with one monotonic 3-second deadline\n");
        fflush(stdout);
        int rc = pthread_cond_timedwait(&changed, &mutex, &deadline);
        if (rc == ETIMEDOUT) {
            printf("worker: ETIMEDOUT; mutex reacquired; ready=%d\n", ready);
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
        printf("worker: monotonic deadline reached without data\n");
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

    initialize_condition();

    if (prepared) {
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
    printf("main: condition variable and mutex destroyed\n");

    if (result.got_data != prepared) {
        return 2;
    }
    return result.got_data && result.value != 42 ? 2 : 0;
}
