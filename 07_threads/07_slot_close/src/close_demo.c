#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
static int slot = 0;
static int full = 0;
static int closed = 0;

struct result {
    int count;
    int sum;
};

static void check_pthread(int rc, const char *operation)
{
    if (rc != 0) {
        fprintf(stderr, "%s: %s\n", operation, strerror(rc));
        exit(1);
    }
}

/* Return 1 if accepted, 0 if the slot has been closed. */
static int put_item(int item)
{
    check_pthread(pthread_mutex_lock(&mutex), "producer mutex_lock");
    while (full && !closed) {
        printf("main: full=1; waiting for an empty slot\n");
        check_pthread(pthread_cond_wait(&not_full, &mutex), "wait not_full");
    }
    if (closed) {
        check_pthread(pthread_mutex_unlock(&mutex), "producer mutex_unlock");
        return 0;
    }

    slot = item;
    full = 1;
    printf("main: put value=%d; full=1\n", item);
    check_pthread(pthread_cond_signal(&not_empty), "signal not_empty");
    check_pthread(pthread_mutex_unlock(&mutex), "producer mutex_unlock");
    return 1;
}

/* Return 1 with data in *out, or 0 when closed and empty. */
static int take_item(int *out)
{
    check_pthread(pthread_mutex_lock(&mutex), "consumer mutex_lock");
    while (!full && !closed) {
        printf("worker: empty and open; waiting for an item\n");
        check_pthread(pthread_cond_wait(&not_empty, &mutex), "wait not_empty");
    }
    if (!full) {
        printf("worker: closed and empty; end of stream\n");
        check_pthread(pthread_mutex_unlock(&mutex), "consumer mutex_unlock");
        return 0;
    }

    *out = slot;
    full = 0;
    printf("worker: take value=%d; full=0\n", *out);
    check_pthread(pthread_cond_signal(&not_full), "signal not_full");
    check_pthread(pthread_mutex_unlock(&mutex), "consumer mutex_unlock");
    return 1;
}

static void close_slot(void)
{
    check_pthread(pthread_mutex_lock(&mutex), "close mutex_lock");
    closed = 1;
    printf("queue: closed=1; full=%d\n", full);
    /* This demo has at most one producer and one consumer. */
    check_pthread(pthread_cond_signal(&not_empty), "close signal not_empty");
    check_pthread(pthread_cond_signal(&not_full), "close signal not_full");
    check_pthread(pthread_mutex_unlock(&mutex), "close mutex_unlock");
}

static void *worker(void *arg)
{
    struct result *result = arg;
    int item;

    while (take_item(&item)) {
        result->count++;
        result->sum += item;
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int items[] = {10, 0, 30};
    int expected_count = 3;
    int expected_sum = 0;
    struct result result = { .count = 0, .sum = 0 };
    pthread_t worker_thread;

    if (argc == 2 && strcmp(argv[1], "empty") == 0) {
        expected_count = 0;
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [empty]\n", argv[0]);
        return 2;
    }

    printf("main: producing %d tasks; consumer does not know the count\n",
           expected_count);
    check_pthread(pthread_create(&worker_thread, NULL, worker, &result),
                  "pthread_create");

    for (int i = 0; i < expected_count; ++i) {
        if (!put_item(items[i])) {
            fprintf(stderr, "main: unexpected rejection before closing\n");
            exit(1);
        }
        expected_sum += items[i];
    }

    close_slot();
    check_pthread(pthread_join(worker_thread, NULL), "pthread_join");
    printf("main: joined; count=%d, sum=%d, expected_count=%d, expected_sum=%d\n",
           result.count, result.sum, expected_count, expected_sum);

    check_pthread(pthread_cond_destroy(&not_empty), "destroy not_empty");
    check_pthread(pthread_cond_destroy(&not_full), "destroy not_full");
    check_pthread(pthread_mutex_destroy(&mutex), "destroy mutex");
    printf("main: synchronization objects destroyed\n");
    return result.count == expected_count && result.sum == expected_sum ? 0 : 2;
}
