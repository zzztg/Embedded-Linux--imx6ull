#define main original_demo_main
#include "../src/ring_demo.c"
#undef main
#include <assert.h>

int main(void)
{
    int item = -7;
    assert(put_item(10) == 1);
    assert(put_item(0) == 1);
    assert(put_item(30) == 1);
    assert(head == 0 && tail == 0 && count == CAPACITY);
    assert(take_item(1, &item) == 1 && item == 10);
    assert(put_item(40) == 1);
    assert(head == 1 && tail == 1 && count == CAPACITY);
    close_queue();
    assert(take_item(1, &item) == 1 && item == 0);
    assert(take_item(1, &item) == 1 && item == 30);
    assert(take_item(1, &item) == 1 && item == 40);
    assert(count == 0 && head == tail && closed == 1);
    item = -7;
    assert(take_item(1, &item) == 0 && item == -7);
    assert(put_item(99) == 0);
    check_pthread(pthread_cond_destroy(&not_empty), "destroy not_empty");
    check_pthread(pthread_cond_destroy(&not_full), "destroy not_full");
    check_pthread(pthread_mutex_destroy(&mutex), "destroy mutex");
    printf("Passed: full uses all slots, wrap and refill preserve FIFO, zero data,");
    printf(" close drains backlog, EOF preserves output, late put rejected\n");
    return 0;
}
