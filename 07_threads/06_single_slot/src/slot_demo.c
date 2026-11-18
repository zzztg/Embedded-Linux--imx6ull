#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TASK_COUNT 3

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
static int slot = 0;
static int full = 0;

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

static void put_item(int item)
{
    check_pthread(pthread_mutex_lock(&mutex), "producer mutex_lock");

    while (full) {
        printf("main: full=1; waiting for an empty slot\n");
        check_pthread(pthread_cond_wait(&not_full, &mutex), "wait not_full");
    }

    slot = item;
    full = 1;
    printf("main: put value=%d; full=1\n", item);

    check_pthread(pthread_cond_signal(&not_empty), "signal not_empty");
    check_pthread(pthread_mutex_unlock(&mutex), "producer mutex_unlock");
}

static int take_item(void)
{
    check_pthread(pthread_mutex_lock(&mutex), "consumer mutex_lock");

    while (!full) {
        printf("worker: full=0; waiting for an item\n");
        check_pthread(pthread_cond_wait(&not_empty, &mutex), "wait not_empty");
    }

    int item = slot;
    full = 0;
    printf("worker: take value=%d; full=0\n", item);

    check_pthread(pthread_cond_signal(&not_full), "signal not_full");
    check_pthread(pthread_mutex_unlock(&mutex), "consumer mutex_unlock");
    return item;
}

static void *worker(void *arg)
{
    struct result *result = arg;

    for (int i = 0; i < TASK_COUNT; ++i) {
        int item = take_item();
        result->count++;
        result->sum += item;
    }

    return NULL;
}

int main(void)
{
    pthread_t worker_thread;
    struct result result = { .count = 0, .sum = 0 };
    int expected_sum = 0;

    printf("main: one slot; producing %d tasks\n", TASK_COUNT);
    check_pthread(pthread_create(&worker_thread, NULL, worker, &result),
                  "pthread_create");

    for (int i = 1; i <= TASK_COUNT; ++i) {
        int item = i * 10;
        put_item(item);
        expected_sum += item;
    }

    check_pthread(pthread_join(worker_thread, NULL), "pthread_join");
    printf("main: joined; count=%d, sum=%d, expected_sum=%d\n",
           result.count, result.sum, expected_sum);

    check_pthread(pthread_cond_destroy(&not_empty), "destroy not_empty");
    check_pthread(pthread_cond_destroy(&not_full), "destroy not_full");
    check_pthread(pthread_mutex_destroy(&mutex), "destroy mutex");
    printf("main: synchronization objects destroyed\n");

    return result.count == TASK_COUNT && result.sum == expected_sum ? 0 : 2;
}
