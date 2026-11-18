#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <string.h>

struct job {
    int left;
    int right;
    int sum;
};

static void *worker(void *arg)
{
    struct job *job = arg;

    printf("worker: left=%d, right=%d\n", job->left, job->right);
    job->sum = job->left + job->right;
    printf("worker: sum=%d; returning its address\n", job->sum);

    /* The pointed-to object belongs to main and outlives this worker. */
    return &job->sum;
}

int main(void)
{
    struct job job = { .left = 12, .right = 30, .sum = 0 };
    pthread_t worker_thread;
    void *result = NULL;

    printf("main: before create sum=%d\n", job.sum);

    int rc = pthread_create(&worker_thread, NULL, worker, &job);
    if (rc != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(rc));
        return 1;
    }

    rc = pthread_join(worker_thread, &result);
    if (rc != 0) {
        fprintf(stderr, "pthread_join: %s\n", strerror(rc));
        return 1;
    }

    int *sum = result;
    printf("main: join rc=%d, job.sum=%d, *sum=%d\n", rc, job.sum, *sum);
    printf("main: result points to job.sum: %s\n",
           sum == &job.sum ? "yes" : "no");

    return 0;
}
