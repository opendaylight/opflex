#include <stdarg.h>
#include <stdlib.h>
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
#include "util.h"
#include "peovs_monitor.h"
#include "vlog.h"

#define USE_VLOG 1

//void *test_monitor_init(void *input);
static void rt_peovs_monitor(void **state);

VLOG_DEFINE_THIS_MODULE(test_pe_monitor);

static void rt_peovs_monitor(void **state) {
    (void) state;
    const char *logfile = "/var/log/openvswitch/pe_monitor.log";
    uid_t myuid;
    pe_monitor_thread_mgmt_t arg;
    pthread_t monitor;
    struct ovs_mutex lock;
    struct stat *stbuf;
    int stat_retval = 0;
    int err_no = 0;
    int save_errno = 0;
    int result = 0;
    FILE *pfd = 0;

    if (vlog_set_log_file(logfile) != 0)
        VLOG_ERR("Cannot open %s\n",logfile);

    VLOG_ENTER(__func__);

//TODO: add check to make sure that libvirt is available and running

    stbuf = xzalloc(sizeof(struct stat));

    ovs_mutex_init(&lock);
    xpthread_cond_init(&arg.quit_notice, NULL);
    ovs_mutex_init(&arg.lock);
    //arg.lvirt = "qemu:///system";
    arg.lvirt = NULL;
 
    stat_retval = stat(PE_OVSDB_SOCK_PATH, stbuf);
    err_no = errno;

    if (stat_retval == 0) {
        /* sets up the monitor */
        if(stbuf->st_uid == (myuid = geteuid())) {
            pag_pthread_create(&monitor,NULL,pe_monitor_init,(void *) &arg);
            VLOG_INFO("Monitor started.");

            pfd = pipe_read("/bin/virsh reboot virtc1");
            result = pipe_close_na(pfd,&save_errno);
            if(result != 0) {
                VLOG_ERR("pipe_close_na returned %s\n",strerror(save_errno));
            } else {
                VLOG_INFO("virtc1 rebooted.\n");
            }

            sleep(1);
            xpthread_cond_signal(&arg.quit_notice);
            sleep(1);
            //pthread_cancel(monitor);
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

    ovs_mutex_destroy(&arg.lock);
    xpthread_cond_destroy(&arg.quit_notice);

    VLOG_LEAVE(__func__);
}

int main(void) {
    const UnitTest tests[] = {
        unit_test(rt_peovs_monitor),
    };
    return run_tests(tests);
}
