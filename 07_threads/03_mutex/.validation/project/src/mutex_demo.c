#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int counter = 0;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

/* On an unexpected pthread error, terminate the entire demo process. */
static void check_pthread(int rc, const char *operation)
{
    if (rc != 0) {
        fprintf(stderr, "%s: %s\n", operation, strerror(rc));
        exit(1);
    }
}

static void add_many(int iterations)
{
    for (int i = 0; i < iterations; ++i) {
        check_pthread(pthread_mutex_lock(&counter_mutex),
                      "pthread_mutex_lock");

        /* Both threads use this same mutex for the whole update. */
        counter++;

        check_pthread(pthread_mutex_unlock(&counter_mutex),
                      "pthread_mutex_unlock");
    }
}

static void *worker(void *arg)
{
    int iterations = *(int *)arg;
    add_many(iterations);
    return NULL;
}

int main(void)
{
    int iterations = 100000;
    pthread_t worker_thread;

    printf("main: before create counter=%d\n", counter);

    check_pthread(pthread_create(&worker_thread, NULL, worker, &iterations),
                  "pthread_create");

    /* The main thread also performs updates before joining the worker. */
    add_many(iterations);

    check_pthread(pthread_join(worker_thread, NULL), "pthread_join");

    printf("main: after join counter=%d, expected=%d\n",
           counter, 2 * iterations);

    check_pthread(pthread_mutex_destroy(&counter_mutex),
                  "pthread_mutex_destroy");
    printf("main: mutex destroyed\n");

    return counter == 2 * iterations ? 0 : 2;
}
