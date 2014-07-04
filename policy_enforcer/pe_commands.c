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
 */

#include <stdint.h>

#include "policy_enforcer.h"
#include "pe_commands.h"
#include "vlog.h"

VLOG_DEFINE_THIS_MODULE(pe_commands);


/* static protos */
static uint32_t
pe_command_jump(pe_ovs_command_t, uint32_t, void **);
static uint32_t
pe_ovs_create_bridge(uint32_t, void **);
static uint32_t
pe_ovs_create_broadcast_domain(uint32_t, void **);
static uint32_t
pe_ovs_create_host_port(uint32_t, void **);
static uint32_t
pe_ovs_create_router(uint32_t, void **);
static uint32_t
pe_ovs_add_bd_member(uint32_t, void **);
static uint32_t
pe_ovs_add_flow_arp(uint32_t, void **);
static uint32_t
pe_ovs_add_flow_select_sepg(uint32_t, void **);
static uint32_t
pe_ovs_add_flow_select_depg_l2(uint32_t, void **);
static uint32_t
pe_ovs_add_flow_select_depg_l3(uint32_t, void **);
static uint32_t
pe_ovs_add_flow_policy_permit(uint32_t, void **);
static uint32_t
pe_ovs_add_flow_policy_permit_bi(uint32_t, void **);
static uint32_t
pe_ovs_add_flow_policy_deny(uint32_t, void **);
static uint32_t
pe_ovs_add_flow_policy_drop(uint32_t, void **);
static uint32_t
pe_ovs_commit_flows(uint32_t, void **);

/* jump table - NB: indexing into this table uses pe_ovs_command_t */
static uint32_t (*pe_ovs_commands[])(uint32_t,void **) = {
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


/* ============================================================
 *
 * \brief pe_command_list_processor()
 *        Process the list of commands by executing them in the
 *        correct order and honoring any dependencies
 *
 * @param[in]
 *          list - head of linked list of commands in the order to be
 *                 executed
 *
 * \return { a list of results for the commands executed }
 *
 **/

uint32_t **pe_command_list_processor(pe_command_node_t *list, uint32_t count) {
    uint32_t results[count];
    pe_command_node_t *current;
    uint32_t total_count = 0;
    uint32_t good_count = 0;
    uint32_t fail_count = 0;

    VLOG_ENTER(__func__);

    if(list == NULL)
        ovs_abort(-1,"NULL passed into %s",__func__);

    current = list;
    do { /* there's always at least one */

        VLOG_DBG("Current pointer at %i in group %i command %i\n",
                   current->cmd_id, current->group_id, current->cmd);
        
        results[total_count] = pe_command_jump(current->cmd,
                                               current->nr_args,
                                               current->v_args);
        
        if (results[total_count] == 0) {
            good_count++;
        } else {
            fail_count++;
            //check dependencies and fail them
        }
        current = current->next;
        total_count++;
    } while(current != NULL);

    VLOG_LEAVE(__func__);
    return(&results);
}

static uint32_t
pe_command_jump(pe_ovs_command_t cmd, uint32_t c_args, void **c_argv) {
    uint32_t result = 0;


    VLOG_ENTER(__func__);

    result = pe_ovs_commands[cmd] (c_args,c_argv);
    
    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_create_bridge(uint32_t c_args, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_create_broadcast_domain(uint32_t c_args, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_create_host_port(uint32_t c_args, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_create_router(uint32_t c_args, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_bd_member(uint32_t c_args, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_arp(uint32_t c_args, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_select_sepg(uint32_t c_args, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_select_depg_l2(uint32_t c_args, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_select_depg_l3(uint32_t c_args, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_policy_permit(uint32_t c_args, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_policy_permit_bi(uint32_t c_args, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_policy_deny(uint32_t c_args, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_add_flow_policy_drop(uint32_t c_args, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
static uint32_t
pe_ovs_commit_flows(uint32_t c_args, void **c_argv) {
    uint32_t result = 0;

    VLOG_ENTER(__func__);

    VLOG_LEAVE(__func__);
    return(result);
}
