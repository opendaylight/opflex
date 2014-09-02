#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ovs-thread.h"
#include "peovs.h"
#include "peovs_commands.h"
#include "vlog.h"
#include "vlog-opflex.h"

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

static void rt_simple_policy(void **state) {
    (void) state;
    char *brname = "test_br1";
    char *option = "-O OpenFlow13,OpenFlow12,OpenFlow11";
    char *arg2 = "\"group_id=1, type=all\"";
    char *arg3 = "\"priority=10, table=1, reg2=1,dl_dst=ff:ff:ff:ff:ff:ff, actions=group:1\"";
    char *arg4 = "set bridge";
    char *arg5 = "protocols=OpenFlow13,OpenFlow12,OpenFlow11";
    int32_t result_sum = 0;
    void *arg1_array[2];
    void *arg2_array[2];
    void *arg3_array[2];
    void *arg4_array[3];
    void *opt_array[1];
    //char *bd     = "1";
    pe_command_results_t *these_results;
    pe_command_results_t *result = NULL;
    pe_command_results_t *next_result = NULL;
    pe_command_node_t *c_node, *c_head_node;

    VLOG_ENTER(__func__);

    memset(arg1_array,0,sizeof(arg1_array));
    memset(arg2_array,0,sizeof(arg2_array));
    memset(arg3_array,0,sizeof(arg3_array));
    memset(arg4_array,0,sizeof(arg4_array));
    memset(opt_array,0,sizeof(opt_array));

    arg1_array[0] = (void *) brname;
    arg1_array[1] = NULL;
    arg2_array[0] = (void *) brname;
    arg2_array[1] = (void *) arg2;
    arg3_array[0] = (void *) brname;
    arg3_array[1] = (void *) arg3;
    arg4_array[0] = (void *) arg4;
    arg4_array[1] = (void *) brname;
    arg4_array[2] = (void *) arg5;
    opt_array[0] = (void *) option;

    /* add a bridge */
    c_node = xmalloc(sizeof(pe_command_node_t));
    c_node->group_id = 0;
    c_node->cmd_id = 0;
    c_node->next = xmalloc(sizeof(pe_command_node_t));
    c_node->previous = NULL;
    c_node->cmd = VS_CREATE_BRIDGE;
    c_node->nr_args = 1;
    c_node->v_args = arg1_array;
    c_node->nr_opts = 0;
    c_node->v_opts = NULL;
    c_head_node = c_node;

    /* set protocols on bridge */
    c_node = c_node->next;
    c_node->previous = c_head_node;
    c_node->group_id = 0;
    c_node->cmd_id = 2;
    c_node->next = xmalloc(sizeof(pe_command_node_t));
    c_node->cmd = VS_GP_CMD;
    c_node->nr_args = 3;
    c_node->v_args = arg4_array;
    c_node->nr_opts = 0;
    c_node->v_opts = NULL;

    /* add a group */
    c_node = c_node->next;
    c_node->previous = c_head_node;
    c_node->group_id = 0;
    c_node->cmd_id = 3;
    c_node->next = xmalloc(sizeof(pe_command_node_t));
    c_node->cmd = OF_ADD_GROUP;
    c_node->nr_args = 2;
    c_node->v_args = arg2_array;
    c_node->nr_opts = 1;
    c_node->v_opts = opt_array;

    /*  add flow */
    c_node->next->previous = c_node;
    c_node = c_node->next;
    c_node->group_id = 0;
    c_node->cmd_id = 4;
    c_node->cmd = OF_ADD_FLOW;
    c_node->next = NULL;
    c_node->nr_args = 2;
    c_node->v_args = arg3_array;
    c_node->nr_opts = 1;
    c_node->v_opts = opt_array;

    these_results = pe_command_list_processor(c_head_node, 3);
    result = these_results;

    while(result != NULL)
    {
        VLOG_DBG("Command id/Group id %d/%d returned %d with errno = %d\n",
                  result->cmd_ptr->cmd_id,
                  result->cmd_ptr->group_id,
                  result->retcode,
                  result->err_no);
        result_sum += result_sum + result->retcode;
        result = result->next;
    }

    assert_int_equal(result_sum, 0);

    result = these_results;
    while(result != NULL)
    {
        next_result = result->next;
        free(result->cmd_ptr);
        free(result);
        result = next_result;
    }

    VLOG_LEAVE(__func__);
}
static void rt_cleanup_simple_policy(void **state) {
    (void) state;
    char *brname = "test_br1";
    char *option = "-O OpenFlow13,OpenFlow12,OpenFlow11";
    int32_t result_sum = 0;
    void *arg_array[1];
    void *opt_array[1];
    //char *bd     = "1";
    pe_command_results_t *these_results;
    pe_command_results_t *result = NULL;
    pe_command_results_t *next_result = NULL;
    pe_command_node_t *c_node, *c_head_node;

    VLOG_ENTER(__func__);

    memset(arg_array,0,sizeof(arg_array));
    memset(opt_array,0,sizeof(opt_array));

    arg_array[0] = (void *) brname;
    opt_array[0] = (void *) option;

    /* this test cleans up what rt_simple_policy did */
    /* remove the flows first */
    c_node = xmalloc(sizeof(pe_command_node_t));
    c_node->group_id = 1;
    c_node->cmd_id = 0;
    c_node->next = xmalloc(sizeof(pe_command_node_t));
    c_node->previous = NULL;
    c_node->cmd = OF_DEL_FLOWS;
    c_node->nr_args = 1;
    c_node->v_args = arg_array;
    c_node->nr_opts = 1;
    c_node->v_opts = opt_array;
    c_head_node = c_node;

    /* now remove the group group */
    c_node = c_node->next;
    c_node->previous = c_head_node;
    c_node->group_id = 1;
    c_node->cmd_id = 1;
    c_node->next = xmalloc(sizeof(pe_command_node_t));
    c_node->cmd = OF_DEL_GROUPS;
    c_node->nr_args = 1;
    c_node->v_args = arg_array;
    c_node->nr_opts = 1;
    c_node->v_opts = opt_array;

    /* now the bridge */
    c_node->next->previous = c_node;
    c_node = c_node->next;
    c_node->group_id = 1;
    c_node->cmd_id = 2;
    c_node->cmd = VS_DELETE_BRIDGE;
    c_node->next = NULL;
    c_node->nr_args = 1;
    c_node->v_args = arg_array;
    c_node->nr_opts = 0;
    c_node->v_opts = NULL;

    these_results = pe_command_list_processor(c_head_node, 3);
    result = these_results;

    while(result != NULL)
    {
        VLOG_DBG("Command id/Group id %d/%d returned %d with errno = %d\n",
                  result->cmd_ptr->cmd_id,
                  result->cmd_ptr->group_id,
                  result->retcode,
                  result->err_no);
        result_sum += result_sum + result->retcode;
        result = result->next;
    }

    assert_int_equal(result_sum, 0);

    result = these_results;
    while(result != NULL)
    {
        next_result = result->next;
        free(result->cmd_ptr);
        free(result);
        result = next_result;
    }

    VLOG_LEAVE(__func__);
}

static void rt_bad_command(void **state) {
    (void) state;
    char brname[9] = "test_br1";
    void *arg_array[2];
    pe_command_results_t *these_results;
    pe_command_results_t *result=NULL;
    pe_command_results_t *next_result=NULL;
    pe_command_node_t *c_node = xmalloc(sizeof(pe_command_node_t));
    pe_command_node_t *c_node_head;

    VLOG_ENTER(__func__);

    arg_array[0] = (void *) brname;
    arg_array[1] = NULL;

    // pass a bad bridge creation command (2nd arg is NULL)
    c_node->group_id = 2;
    c_node->cmd_id = 0;
    c_node->next = xmalloc(sizeof(pe_command_node_t));
    c_node->cmd = VS_CREATE_BRIDGE;
    c_node->nr_args = 2;
    c_node->v_args = arg_array;
    c_node->nr_opts = 0;
    c_node->v_opts = NULL;

    c_node_head = c_node;
    c_node = c_node->next;

    c_node->group_id = 2;
    c_node->cmd_id = 1;
    c_node->next = NULL;
    c_node->cmd = PE_OVS_COMMAND_TOTAL + 1;
    c_node->nr_args = 0;
    c_node->v_args = NULL;
    c_node->nr_opts = 0;
    c_node->v_opts = NULL;

    these_results = pe_command_list_processor(c_node_head, 2);

    VLOG_INFO("Creation of test_br1 returned: %d with errno = %d\n",
               these_results->retcode, these_results->err_no);
    result = these_results;
    while(result != NULL)
    {
        VLOG_DBG("Command id/Group id %d/%d returned %d with errno = %d\n",
                  result->cmd_ptr->cmd_id,
                  result->cmd_ptr->group_id,
                  result->retcode,
                  result->err_no);
        assert_int_not_equal(result->retcode,0);
        result = result->next;
    }

    result = these_results;
    while(result != NULL)
    {
        next_result = result->next;
        free(result->cmd_ptr);
        free(result);
        result = next_result;
    }

    VLOG_LEAVE(__func__);
}
/*
static void rt_test_policy(void **state) {
    (void) state;

    VLOG_ENTER(__func__);

    //Read in params to build list to execute

    VLOG_LEAVE(__func__);
}
*/
int main(void) {
    int retval;

//  Uncomment next line to turn on debugging
      vlog_set_levels(vlog_module_from_name("test_pe_commands"), -1, VLL_DBG);

    const UnitTest tests[] = {
        unit_test(rt_create_bridge),
        unit_test(rt_delete_bridge),
        unit_test(rt_simple_policy),
        unit_test(rt_cleanup_simple_policy),
        unit_test(rt_bad_command),
//        unit_test(rt_test_policy),
    };
    
    return(run_tests(tests));
}
