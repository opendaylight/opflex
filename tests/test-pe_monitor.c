#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#include "ovs-thread.h"
#include "misc-util.h"
#include "pe_monitor.h"

static void rt_pe_monitor(void **state) {
    (void) state;
    pthread_t monitor;
    bool pe_monitor_quit;

    /* sets up the monitor */
    pthread_create(&monitor,NULL,pe_monitor_init,NULL);
    sleep(3);
    pe_monitor_quit = true;
    sleep(2);
    xpthread_join(monitor,NULL);

//    assert_int_equal(push_counter, PE_TEST_PRODUCER_MAX_PUSH);
//    assert_int_equal(push_thread_counter, PE_TEST_PRODUCER_THREADS);
}

int main(void) {
    const UnitTest tests[] = {
        unit_test(rt_pe_monitor),
    };
    return run_tests(tests);
}
