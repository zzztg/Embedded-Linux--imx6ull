#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORKER_COUNT 2

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
static int slot = 0;
static int full = 0;
static int closed = 0;

struct worker_info {
    int id;
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

static int take_item(int id, int *out)
{
    check_pthread(pthread_mutex_lock(&mutex), "consumer mutex_lock");
    while (!full && !closed) {
        printf("worker %d: empty and open; waiting\n", id);
        fflush(stdout);
        check_pthread(pthread_cond_wait(&not_empty, &mutex), "wait not_empty");
    }
    if (!full) {
        printf("worker %d: closed and empty; exiting\n", id);
        check_pthread(pthread_mutex_unlock(&mutex), "consumer mutex_unlock");
        return 0;
    }
    *out = slot;
    full = 0;
    printf("worker %d: take value=%d; full=0\n", id, *out);
    check_pthread(pthread_cond_signal(&not_full), "signal not_full");
    check_pthread(pthread_mutex_unlock(&mutex), "consumer mutex_unlock");
    return 1;
}

static void close_slot(void)
{
    check_pthread(pthread_mutex_lock(&mutex), "close mutex_lock");
    closed = 1;
    printf("queue: closed=1; full=%d; broadcasting\n", full);
    check_pthread(pthread_cond_broadcast(&not_empty), "broadcast not_empty");
    check_pthread(pthread_cond_broadcast(&not_full), "broadcast not_full");
    check_pthread(pthread_mutex_unlock(&mutex), "close mutex_unlock");
}

static void *worker(void *arg)
{
    struct worker_info *info = arg;
    int item;
    while (take_item(info->id, &item)) {
        info->count++;
        info->sum += item;
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int items[] = {10, 0, 30};
    int expected_count = 3;
    int expected_sum = 0;
    int total_count = 0;
    int total_sum = 0;
    pthread_t threads[WORKER_COUNT];
    struct worker_info infos[WORKER_COUNT] = {0};

    if (argc == 2 && strcmp(argv[1], "empty") == 0) {
        expected_count = 0;
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [empty]\n", argv[0]);
        return 2;
    }

    for (int i = 0; i < WORKER_COUNT; ++i) {
        infos[i].id = i + 1;
        check_pthread(pthread_create(&threads[i], NULL, worker, &infos[i]),
                      "pthread_create");
    }

    printf("main: wait for both workers to print waiting, then press ENTER\n");
    fflush(stdout);
    if (getchar() == EOF) {
        fprintf(stderr, "main: input ended before ENTER\n");
        exit(2);
    }

    for (int i = 0; i < expected_count; ++i) {
        if (!put_item(items[i])) {
            fprintf(stderr, "main: unexpected rejection before closing\n");
            exit(1);
        }
        expected_sum += items[i];
    }
    close_slot();

    for (int i = 0; i < WORKER_COUNT; ++i) {
        check_pthread(pthread_join(threads[i], NULL), "pthread_join");
        printf("main: joined worker %d; count=%d, sum=%d\n",
               infos[i].id, infos[i].count, infos[i].sum);
        total_count += infos[i].count;
        total_sum += infos[i].sum;
    }
    printf("main: total count=%d, sum=%d, expected_count=%d, expected_sum=%d\n",
           total_count, total_sum, expected_count, expected_sum);

    check_pthread(pthread_cond_destroy(&not_empty), "destroy not_empty");
    check_pthread(pthread_cond_destroy(&not_full), "destroy not_full");
    check_pthread(pthread_mutex_destroy(&mutex), "destroy mutex");
    printf("main: synchronization objects destroyed\n");
    return total_count == expected_count && total_sum == expected_sum ? 0 : 2;
}
