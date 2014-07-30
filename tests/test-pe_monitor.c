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
#include "peovs_monitor.h"
#include "vlog.h"

#define USE_VLOG 1

void *test_monitor_init(void *input);


VLOG_DEFINE_THIS_MODULE(test_pe_monitor);

static void rt_pe_monitor(void **state) {
    (void) state;
    const char *logfile = "/var/log/openvswitch/pe_monitor.log";
    uid_t myuid;
    pthread_t monitor;
    struct ovs_mutex lock;
    pthread_cond_t quit_notice;
    pe_monitor_thread_mgmt_t arg;
    struct stat *stbuf;
    int stat_retval = 0;
    int err_no = 0;

    if (vlog_set_log_file(logfile) != 0)
        VLOG_ERR("Cannot open %s\n",logfile);

    VLOG_ENTER(__func__);

    stbuf = xzalloc(sizeof(struct stat));

    ovs_mutex_init(&lock);
    xpthread_cond_init(&quit_notice, NULL);
    arg.lock = &lock;
    arg.quit_notice = &quit_notice;
 
    stat_retval = stat(PE_OVSDB_SOCK_PATH, stbuf);
    err_no = errno;

    if (stat_retval == 0) {
        /* sets up the monitor */
        if(stbuf->st_uid == (myuid = geteuid())) {
            pag_pthread_create(&monitor,NULL,pe_monitor_init,(void *) &arg);
            VLOG_INFO("Monitor started.");
            sleep(3);
            xpthread_cond_signal(&quit_notice);
            sleep(1);
            pag_pthread_join(monitor,NULL);
        } else {
            VLOG_WARN("Test must run with same uid as OVS, but"
                      " OVS is running as uid=%i and this uid=%i",
                      stbuf->st_uid,myuid);
        }
    } else {
         //need to find a good way to start ovs in this case
         VLOG_WARN("OVSDB not running, test cannot proceed\n");
         VLOG_INFO("      stat returned: %s",strerror(err_no));
    }

//    assert_int_equal(push_counter, PE_TEST_PRODUCER_MAX_PUSH);
//    assert_int_equal(push_thread_counter, PE_TEST_PRODUCER_THREADS);
    VLOG_LEAVE(__func__);
}
/*
void *test_monitor_init(void *input) {
    (void) input;

    VLOG_ENTER(__func__);

    VLOG_DBG("Calling pe_monitor_init()");

    pe_monitor_init();

    VLOG_LEAVE(__func__);

    pthread_exit(NULL);
}
*/

int main(void) {
    const UnitTest tests[] = {
        unit_test(rt_pe_monitor),
    };
    return run_tests(tests);
}
