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
    char brname[9] = "test_br1";
    void *arg_array[1];
    pe_command_results_t *these_results;
    pe_command_node_t c_node;

    VLOG_ENTER(__func__);

    //set up container and start ovs + ovsdb
    //start with bridge creation as first command into pe_command
    arg_array[0] = (void *) brname;
    
    c_node.group_id = 0;
    c_node.cmd_id = 0;
    c_node.next = NULL;
    c_node.cmd = VS_DELETE_BRIDGE;
    c_node.nr_args = 1;
    c_node.v_args = arg_array;
    c_node.nr_opts = 0;
    c_node.v_opts = NULL;

    these_results = pe_command_list_processor(&c_node, 1);

    VLOG_INFO("Deletion of test_br1 returned: %d with errno = %d\n",
               these_results->retcode, these_results->err_no);

    assert_int_equal(these_results->retcode,0);

    free(these_results);

    VLOG_LEAVE(__func__);

}

static void rt_create_bridge(void **state) {
    (void) state;
    char brname[9] = "test_br1";
    void *arg_array[1];
    pe_command_results_t *these_results;
    pe_command_node_t c_node;

    VLOG_ENTER(__func__);

//    c_node = xzalloc(sizeof(pe_command_node_t));

    //set up container and start ovs + ovsdb
    //start with bridge creation as first command into pe_command
    arg_array[0] = (void *) brname;

    // pass a bridge creation command
    c_node.group_id = 0;
    c_node.cmd_id = 0;
    c_node.next = NULL;
    c_node.cmd = VS_CREATE_BRIDGE;
    c_node.nr_args = 1;
    c_node.v_args = arg_array;
    c_node.nr_opts = 0;
    c_node.v_opts = NULL;

    these_results = pe_command_list_processor(&c_node, 1);

    VLOG_INFO("Creation of test_br1 returned: %d with errno = %d\n",
               these_results->retcode, these_results->err_no);

    assert_int_equal(these_results->retcode,0);

    free(these_results);

    VLOG_LEAVE(__func__);

}

int main(void) {
    int retval;

    const UnitTest tests[] = {
        unit_test(rt_create_bridge),
        unit_test(rt_delete_bridge),
    };
    
    return(run_tests(tests));
}
