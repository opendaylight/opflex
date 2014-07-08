/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

/*
 * Takes a list of commands from pe_translate and executes them
 * TODO: determine how to use threads here...may not be possible.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "policy_enforcer.h"
#include "pe_commands.h"
#include "misc-util.h"
#include "vlog.h"

VLOG_DEFINE_THIS_MODULE(pe_commands);


/* static protos */
static uint32_t
pe_command_jump(pe_ovs_command_t, uint32_t, void **);
static uint32_t
pe_ovs_create_bridge(pe_ovs_command_t, void **);
static uint32_t
pe_ovs_delete_bridge(pe_ovs_command_t, void **);
static uint32_t
pe_ovs_create_broadcast_domain(pe_ovs_command_t, void **);
static uint32_t
pe_ovs_create_host_port(pe_ovs_command_t, void **);
static uint32_t
pe_ovs_create_router(pe_ovs_command_t, void **);
static uint32_t
pe_ovs_add_bd_member(pe_ovs_command_t, void **);
static uint32_t
pe_ovs_add_flow_arp(pe_ovs_command_t, void **);
static uint32_t
pe_ovs_add_flow_select_sepg(pe_ovs_command_t, void **);
static uint32_t
pe_ovs_add_flow_select_depg_l2(pe_ovs_command_t, void **);
static uint32_t
pe_ovs_add_flow_select_depg_l3(pe_ovs_command_t, void **);
static uint32_t
pe_ovs_add_flow_policy_permit(pe_ovs_command_t, void **);
static uint32_t
pe_ovs_add_flow_policy_permit_bi(pe_ovs_command_t, void **);
static uint32_t
pe_ovs_add_flow_policy_deny(pe_ovs_command_t, void **);
static uint32_t
pe_ovs_add_flow_policy_drop(pe_ovs_command_t, void **);
static uint32_t
pe_ovs_commit_flows(pe_ovs_command_t, void **);

/* jump table - NB: indexing into this table uses pe_ovs_command_t */
static uint32_t (*pe_ovs_commands[])(pe_ovs_command_t,void **) = {
    pe_ovs_create_bridge,
    pe_ovs_create_broadcast_domain,
    pe_ovs_create_host_port,
    pe_ovs_create_router,
    pe_ovs_add_bd_member,
    pe_ovs_add_flow_arp,
    pe_ovs_add_flow_select_sepg,
    pe_ovs_add_flow_select_depg_l2,
    pe_ovs_add_flow_select_depg_l3,
    pe_ovs_add_flow_policy_deny,
    pe_ovs_add_flow_policy_drop,
    pe_ovs_add_flow_policy_permit,
    pe_ovs_add_flow_policy_permit_bi,
    pe_ovs_commit_flows
};

/* command enum to string array - add a command to
 * pe_command_string_defs.h
 */
static const char *pe_ovs_command_array[PE_OVS_COMMAND_TOTAL]= {
#define COMMAND_DEFN(a,b,c) b,
#include "pe_command_string_defs.h"
#undef COMMAND_DEFN
    "end_of_array"
};

static uint32_t pe_nr_of_required_args[PE_OVS_COMMAND_TOTAL]= {
#define COMMAND_DEFN(a,b,c) c,
#include "pe_command_string_defs.h"
#undef COMMAND_DEFN
0
};

#define PE_OVS_BAD_COMMAND 256 /* Greater than 255, largest shell exit code */

/* ============================================================
 *
 * \brief pe_command_list_processor()
 *        Process the list of commands by executing them in the
 *        correct order and honoring any dependencies
 *
 * @param[in]
 *          list - head of linked list of commands in the order to be
 *                 executed
 *          count - number of nodes in the list
 *
 * \return { a list of results for the commands executed, if NULL then
 *            an error occured }
 *
 **/

uint32_t **pe_command_list_processor(pe_command_node_t *list, uint32_t count) {
    uint32_t results[count];
    uint32_t **retval;
    pe_command_node_t *current;
    uint32_t total_count = 0;
    uint32_t good_count = 0;
    uint32_t fail_count = 0;

    VLOG_ENTER(__func__);

    if(list == NULL) {
        VLOG_ERR("NULL passed into %s",__func__);
        retval = NULL;
    } else {
        current = list;
        do { /* there's always at least one */

            VLOG_DBG("Current pointer at %i in group %i command %i\n",
                       current->cmd_id, current->group_id, current->cmd);
        
            results[total_count] = pe_command_jump(current->cmd,
                                                   current->nr_args,
                                                   current->v_args);
        
            VLOG_DBG("In %s; result is %i\n",__func__, results[total_count]);

            if (results[total_count] == 0) {
                good_count++;
            } else {
                fail_count++;
                //check dependencies and fail them
            }
            current = current->next;
            total_count++;
        } while(current != NULL);
        VLOG_DBG("good count %i, fail count %i\n",good_count,fail_count);
        retval = &results;
    }
    VLOG_LEAVE(__func__);
    return(retval);
}

static uint32_t
pe_command_jump(pe_ovs_command_t cmd, uint32_t c_args, void **c_argv) {
    uint32_t result = 0;


    VLOG_ENTER(__func__);

    if (c_args == pe_nr_of_required_args[cmd]) {

        result = pe_ovs_commands[cmd] (cmd, c_argv);

        VLOG_DBG("In %s; result is %i\n",__func__, result);

    } else {

        VLOG_ERR("%s requires %i argument(s), but %i was supplied\n",
                  pe_ovs_command_array[cmd],
                  pe_nr_of_required_args[cmd],
                  c_args);
        result = PE_OVS_BAD_COMMAND;
    }
    
    VLOG_LEAVE(__func__);
    return(result);
}

static uint32_t
pe_ovs_create_bridge(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;
    uint32_t count;
    int exit_status;
    FILE *pipe_fs;
    char command[1024]; //TODO:  need #define for 1024
    char response[1024];//TODO:  need #define for 1024

    VLOG_ENTER(__func__);

    /*
     * Build the command - TODO: need a util for this
     */
    snprintf(command,sizeof(PE_OVSDB_CMD_PATH),PE_OVSDB_CMD_PATH);
    (void) strncat(command,"/",1);
    (void) strncat(command, pe_ovs_command_array[cmd], (sizeof(command)-1));
    (void) strncat(command," ", 1);
    for (count = 0; count < pe_nr_of_required_args[cmd]; count++) {
        (void) strncat(command, c_argv[count], (sizeof(command)-1));
    }

    pipe_fs = pipe_read(command);
    
    if (VLOG_IS_DBG_ENABLED() == true) {
        while(fgets(response, sizeof(response), pipe_fs)) {
            VLOG_DBG("Response of %s to command %s\n", response, command);
        }
    }

    exit_status = pipe_close(pipe_fs);
    result = WEXITSTATUS(exit_status);
    VLOG_INFO("Execution of %s returned exit status %i\n",
               command, result);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_delete_bridge(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;
    uint32_t count;
    int exit_status;
    FILE *pipe_fs;
    char command[1024]; //TODO:  need #define for 1024
    char response[1024];//TODO:  need #define for 1024

    VLOG_ENTER(__func__);

    /*
     * Build the command - TODO: need a util for this
     */
    snprintf(command,sizeof(PE_OVSDB_CMD_PATH),PE_OVSDB_CMD_PATH);
    (void) strncat(command,"/",1);
    (void) strncat(command, pe_ovs_command_array[cmd], (sizeof(command)-1));
    (void) strncat(command," ", 1);
    for (count = 0; count < pe_nr_of_required_args[cmd]; count++) {
        (void) strncat(command, c_argv[count], (sizeof(command)-1));
    }

    pipe_fs = pipe_read(command);
    
    if (VLOG_IS_DBG_ENABLED() == true) {
        while(fgets(response, sizeof(response), pipe_fs)) {
            VLOG_DBG("Response of %s to command %s\n", response, command);
        }
    }

    exit_status = pipe_close(pipe_fs);
    result = WEXITSTATUS(exit_status);
    VLOG_INFO("Execution of %s returned exit status %i\n",
               command, result);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_create_broadcast_domain(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_create_host_port(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_create_router(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_bd_member(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_arp(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_select_sepg(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_select_depg_l2(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_select_depg_l3(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_policy_permit(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_policy_permit_bi(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_policy_deny(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_policy_drop(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_commit_flows(pe_ovs_command_t cmd, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
