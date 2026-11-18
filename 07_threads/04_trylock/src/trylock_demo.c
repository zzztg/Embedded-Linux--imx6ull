#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void check_pthread(int rc, const char *operation)
{
    if (rc != 0) {
        fprintf(stderr, "%s: %s\n", operation, strerror(rc));
        exit(1);
    }
}

static void *worker(void *arg)
{
    int *try_result = arg;
    *try_result = pthread_mutex_trylock(&mutex);

    if (*try_result == EBUSY) {
        printf("worker: trylock returned EBUSY; lock not acquired\n");
        return NULL;
    }

    check_pthread(*try_result, "worker pthread_mutex_trylock");
    printf("worker: unexpectedly acquired mutex\n");
    check_pthread(pthread_mutex_unlock(&mutex), "worker pthread_mutex_unlock");
    return NULL;
}

int main(void)
{
    pthread_t worker_thread;
    int first_result = 0;

    check_pthread(pthread_mutex_lock(&mutex), "main pthread_mutex_lock");
    printf("main: mutex locked before create\n");

    check_pthread(pthread_create(&worker_thread, NULL, worker, &first_result),
                  "pthread_create");

    /* This is safe here because worker only tries once and never waits for mutex.
       Replacing its trylock with lock would deadlock this experiment. */
    check_pthread(pthread_join(worker_thread, NULL), "pthread_join");

    printf("main: worker joined; releasing mutex\n");
    check_pthread(pthread_mutex_unlock(&mutex), "main pthread_mutex_unlock");

    int rc = pthread_mutex_trylock(&mutex);
    check_pthread(rc, "main pthread_mutex_trylock");
    printf("main: trylock after release returned 0; lock acquired\n");

    check_pthread(pthread_mutex_unlock(&mutex), "main pthread_mutex_unlock");
    check_pthread(pthread_mutex_destroy(&mutex), "pthread_mutex_destroy");
    printf("main: mutex unlocked and destroyed\n");

    return first_result == EBUSY ? 0 : 2;
}
