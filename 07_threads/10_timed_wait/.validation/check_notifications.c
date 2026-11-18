#define main original_demo_main
#include "../src/timed_demo.c"
#undef main
#include <assert.h>

int main(void)
{
    pthread_t thread;
    struct wait_result result = {0};
    struct timespec start, finish;
    assert(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
    check_pthread(pthread_create(&thread, NULL, worker, &result), "create");
    for (int i = 0; i < 4; ++i) {
        struct timespec pause = { .tv_sec = 0, .tv_nsec = 200000000L };
        while (nanosleep(&pause, &pause) == -1) {
            assert(errno == EINTR);
        }
        check_pthread(pthread_mutex_lock(&mutex), "lock");
        check_pthread(pthread_cond_signal(&changed), "extra signal");
        check_pthread(pthread_mutex_unlock(&mutex), "unlock");
    }
    check_pthread(pthread_join(thread, NULL), "join");
    assert(clock_gettime(CLOCK_MONOTONIC, &finish) == 0);
    double elapsed = (double)(finish.tv_sec - start.tv_sec)
                   + (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    assert(!result.got_data && elapsed >= 2.5 && elapsed < 6.0);
    check_pthread(pthread_mutex_trylock(&mutex), "verify unlocked");
    check_pthread(pthread_mutex_unlock(&mutex), "unlock");
    check_pthread(pthread_cond_destroy(&changed), "destroy cond");
    check_pthread(pthread_mutex_destroy(&mutex), "destroy mutex");
    printf("Passed: extra notifications do not reset deadline; elapsed=%.3f seconds\n",
           elapsed);
    return 0;
}
