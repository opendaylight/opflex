#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ovs-thread.h"
#include "policy_enforcer.h"
#include "vlog.h"

VLOG_DEFINE_THIS_MODULE(test_pe_commands);

/* protos */

/* private */

static void rt_pe_commands(void **state) {
    (void) state;

    VLOG_ENTER("rt_pe_commands");

    //set up container and start ovs + ovsdb
    //start with bridge creation as first command into pe_command

    VLOG_LEAVE(NULL);
}

int main(void) {
    const UnitTest tests[] = {
        unit_test(rt_pe_commands),
    };
    return run_tests(tests);
}
