#define main original_demo_main
#include "../src/close_demo.c"
#undef main
#include <assert.h>

int main(void)
{
    int item = -7;
    assert(put_item(0) == 1);
    close_slot();
    assert(closed == 1 && full == 1);
    assert(take_item(&item) == 1 && item == 0);
    assert(closed == 1 && full == 0);
    item = -7;
    assert(take_item(&item) == 0 && item == -7);
    assert(put_item(99) == 0);
    assert(full == 0 && closed == 1);
    check_pthread(pthread_cond_destroy(&not_empty), "destroy not_empty");
    check_pthread(pthread_cond_destroy(&not_full), "destroy not_full");
    check_pthread(pthread_mutex_destroy(&mutex), "destroy mutex");
    printf("Passed: closed-full drains zero; EOF preserves output; late put rejected\n");
    return 0;
}
