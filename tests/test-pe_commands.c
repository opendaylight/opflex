#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ovs-thread.h"
#include "policy_enforcer.h"
#include "pe_commands.h"
#include "vlog.h"

VLOG_DEFINE_THIS_MODULE(test_pe_commands);

/* protos */

/* private */
static void rt_delete_bridge(void **state) {
    (void) state;
    char *this_arg = "test_br1";
    uint32_t **these_results;
    pe_command_node_t c_node;

    VLOG_ENTER(__func__);

    //set up container and start ovs + ovsdb
    //start with bridge creation as first command into pe_command

    // pass a bridge creation command
    c_node.group_id = 0;
    c_node.cmd_id = 0;
    c_node.next = NULL;
    c_node.previous = NULL;
    c_node.cmd = DELETE_BRIDGE;
    c_node.nr_args = 1;
    c_node.v_args[0] = (void *) this_arg;

    these_results = pe_command_list_processor(&c_node, 1);

    VLOG_INFO("Deletion of test_br1 returned: %i\n",*these_results[0]);

    assert_int_equal(*these_results[0],0);

    VLOG_LEAVE(__func__);

    return(0);
}

static void rt_create_bridge(void **state) {
    (void) state;
    char *this_arg = "test_br1";
    uint32_t **these_results;
    pe_command_node_t c_node;

    VLOG_ENTER(__func__);

    //set up container and start ovs + ovsdb
    //start with bridge creation as first command into pe_command

    // pass a bridge creation command
    c_node.group_id = 0;
    c_node.cmd_id = 0;
    c_node.next = NULL;
    c_node.cmd = CREATE_BRIDGE;
    c_node.nr_args = 1;
    c_node.v_args[0] = (void *) this_arg;

    these_results = pe_command_list_processor(&c_node, 1);

    VLOG_INFO("Creation of test_br1 returned: %i\n",*these_results[0]);

    assert_int_equal(*these_results[0],0);

    VLOG_LEAVE(__func__);

    return(0);
}

int main(void) {
    int retval;

    const UnitTest tests[] = {
        unit_test(rt_create_bridge),
        unit_test(rt_delete_bridge),
    };
    
    retval = run_tests(tests);
    return(retval);
}
