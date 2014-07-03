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
#include "vlog.h"

VLOG_DEFINE_THIS_MODULE(pe_commands);

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
 * \return { void }
 *
 **/

void pe_command_list_processor(command_node_t *list) {
    command_node_t *current;

    if(list == NULL)
        ovs_abort(-1,"NULL passed into %s",__func__);

    current = list;
    do {

        VLOG_INFO("Current pointer at %i command %s\n",
                   current->id, current->command);
        current = current->next;

    } while(current != NULL);

}
