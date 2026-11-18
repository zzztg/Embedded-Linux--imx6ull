#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int ready = 0;
static int value = 0;

static void check_pthread(int rc, const char *operation)
{
    if (rc != 0) {
        fprintf(stderr, "%s: %s\n", operation, strerror(rc));
        exit(1);
    }
}

static void press_enter(const char *prompt)
{
    printf("%s\n", prompt);
    fflush(stdout);
    if (getchar() == EOF) {
        fprintf(stderr, "main: input ended before ENTER\n");
        exit(2);
    }
}

static void *worker(void *arg)
{
    (void)arg;

    check_pthread(pthread_mutex_lock(&mutex), "worker mutex_lock");
    while (!ready) {
        printf("worker: ready=0; waiting (wait releases mutex)\n");
        fflush(stdout);

        check_pthread(pthread_cond_wait(&changed, &mutex), "cond_wait");

        printf("worker: wait returned; mutex reacquired; ready=%d\n", ready);
    }

    int received = value;
    check_pthread(pthread_mutex_unlock(&mutex), "worker mutex_unlock");

    printf("worker: received value=%d\n", received);
    return NULL;
}

int main(void)
{
    pthread_t worker_thread;
    check_pthread(pthread_create(&worker_thread, NULL, worker, NULL),
                  "pthread_create");

    press_enter("main: press ENTER to notify without setting ready");

    check_pthread(pthread_mutex_lock(&mutex), "main mutex_lock");
    printf("main: notify only; ready=%d\n", ready);
    check_pthread(pthread_cond_signal(&changed), "first cond_signal");
    check_pthread(pthread_mutex_unlock(&mutex), "main mutex_unlock");

    press_enter("main: press ENTER again to publish value=42");

    check_pthread(pthread_mutex_lock(&mutex), "main mutex_lock");
    value = 42;
    ready = 1;
    printf("main: published value=42, ready=1; notifying worker\n");
    check_pthread(pthread_cond_signal(&changed), "second cond_signal");
    check_pthread(pthread_mutex_unlock(&mutex), "main mutex_unlock");

    check_pthread(pthread_join(worker_thread, NULL), "pthread_join");
    check_pthread(pthread_cond_destroy(&changed), "cond_destroy");
    check_pthread(pthread_mutex_destroy(&mutex), "mutex_destroy");
    printf("main: worker joined; condition variable and mutex destroyed\n");
    return 0;
}
