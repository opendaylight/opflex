#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "ovs-thread.h"
#include "misc-util.h"
#include "pe_monitor.h"
#include "vlog.h"

#define USE_VLOG 1

void *test_monitor_init(void *input);


VLOG_DEFINE_THIS_MODULE(test_pe_monitor);

static void rt_pe_monitor(void **state) {
    (void) state;
    pthread_t monitor;
    bool pe_monitor_quit;
    struct stat *sbuf;
    int stat_retval = 0;
    int err_no = 0;

    VLOG_ENTER(__func__);

    sbuf = xzalloc(sizeof(struct stat));

    stat_retval = stat(PE_OVSDB_SOCK_PATH, sbuf);
    err_no = errno;

    if (stat_retval == 0) {
         /* sets up the monitor */
         pag_pthread_create(&monitor,NULL,test_monitor_init,NULL);
         sleep(3);
         pe_set_monitor_quit(true);
         sleep(2);
         xpthread_join(monitor,NULL);
     } else {
         //need to find a good way to start ovs in this case
         VLOG_WARN("OVSDB not running, test cannot proceed\n");
         VLOG_INFO("      stat returned: %s",strerror(err_no));
     }

//    assert_int_equal(push_counter, PE_TEST_PRODUCER_MAX_PUSH);
//    assert_int_equal(push_thread_counter, PE_TEST_PRODUCER_THREADS);
    VLOG_LEAVE(__func__);
}

void *test_monitor_init(void *input) {
    (void) input;

    VLOG_ENTER(__func__);

    VLOG_DBG("Calling pe_monitor_init()");

    pe_monitor_init();

    VLOG_LEAVE(__func__);

    return(NULL);
}

int main(void) {
    const UnitTest tests[] = {
        unit_test(rt_pe_monitor),
    };
    return run_tests(tests);
}
