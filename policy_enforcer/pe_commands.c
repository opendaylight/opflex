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

#define PE_OVS_BAD_COMMAND 256 /* Greater than 255, largest shell exit code */
#define PE_OVS_MAX_COMMAND_SIZE 1024
#define PE_OVS_MAX_RESPONSE_SIZE 1024
static const char *PE_FILENAME_SEPARATOR="/";
static const char *PE_SINGLE_SPACE=" ";

/* static protos */
static char *
pe_ovs_build_command(pe_ovs_command_t,
                     uint32_t,
                     void **,
                     uint32_t,
                     void **);
static void
pe_ovs_command_exec(char *, pe_command_results_t *);

/* command enum to string array - add a command to
 * pe_command_string_defs.mh
 */
/* TODO: set up appropriate sizes for strings of commands and subcommands*/
static const char
pe_ovs_command_array[PE_OVS_COMMAND_TOTAL+1][PE_OVS_MAX_COMMAND_SIZE] = {
#define COMMAND_DEFN(a,b,c) b,
#include "pe_command_string_defs.mh"
#undef COMMAND_DEFN
    "end_of_array"
};

static const char
pe_ovs_subcommand_array[PE_OVS_COMMAND_TOTAL+1][PE_OVS_MAX_COMMAND_SIZE] = {
#define COMMAND_DEFN(a,b,c) c,
#include "pe_command_string_defs.mh"
#undef COMMAND_DEFN
0
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
 *          count - number of nodes in the list
 *
 * \return { a list of results for the commands executed, if NULL then
 *            an error occured }
 *
 **/

/* calling function needs to release memory for results struct */
pe_command_results_t *
pe_command_list_processor(pe_command_node_t *list, uint32_t count) {
    char *command[count];
    pe_command_node_t *current;
    pe_command_results_t *result;
    pe_command_results_t *result_head;
    uint32_t total_count = 0;
    uint32_t good_count = 0;
    uint32_t fail_count = 0;

    VLOG_ENTER(__func__);

    result_head = xmalloc(sizeof(pe_command_results_t)); /* need at least one */
    result_head->cmd_ptr = list;
    result_head->next = NULL;
    result_head->previous = NULL;
    result_head->retcode = 0;
    result_head->err_no = 0;

    if(list == NULL) {
        VLOG_ERR("NULL passed into %s",__func__);
        result_head->retcode = PE_OVS_BAD_COMMAND;
    } else {
        current = list;
        do { /* there's always at least one */
            if (total_count > 0) {
                result->next = xmalloc(sizeof(pe_command_results_t)); 
                result->next->previous = result;
                result = result->next;
                result->retcode = 0;
                result->err_no = 0;
                result->next = NULL;
                result->cmd_ptr = current;
            } else {
                result = result_head;
            }

            VLOG_DBG("Current pointer at %i in group %i command %i\n",
                       current->cmd_id, current->group_id, current->cmd);

            if ((command[total_count] = pe_ovs_build_command(current->cmd,
                                 current->nr_args, current->v_args,
                                 current->nr_opts, current->v_opts))
                 == NULL)
            {
                // TODO: fail the command and fail dependencies without
                //       executing them (optimize), but calling layer
                //       (pe_translate) will determine what to do.
                // TODO: set current to proper pointer
                result->retcode = PE_OVS_BAD_COMMAND;
                current = current->next;
                continue;
            }

            VLOG_INFO("Executing %s\n",command[total_count]);
            pe_ovs_command_exec(command[total_count], result);

            // TODO: at this point, if the result is an error, need to
            // check dependencies (and fail those, too)
        
            VLOG_DBG("In %s; result is %i\n",__func__,
                      result->retcode);

            if (result->retcode == 0) {
                good_count++;
            } else {
                fail_count++;
                //check dependencies and fail them
            }
            current = current->next;
            total_count++;
        } while(current != NULL);
        VLOG_DBG("good count %i, fail count %i\n",good_count,fail_count);
    }
    VLOG_LEAVE(__func__);
    return(result_head);
}

/* space for dest is allocated here and freed in pe_ovs_command_exec */
static char *
pe_ovs_build_command(pe_ovs_command_t cmd,
                     uint32_t nr_args,
                     void **v_args,
                     uint32_t nr_opts,
                     void **v_opts)
{
    uint32_t count;
    char *dest;
    char *path;
    int rval = 0;

//  Uncomment next line to turn on debugging
//    vlog_set_levels(vlog_module_from_name("pe_commands"), -1, VLL_DBG);

    VLOG_ENTER(__func__);

    /* set up the proper command path */
    if(strncmp(pe_ovs_command_array[cmd],"ovs",3) == 0)
    {
        path = xmalloc(strlen(PE_OVSDB_CMD_PATH) + 1);
        rval = snprintf(path, strlen(PE_OVSDB_CMD_PATH)+1, PE_OVSDB_CMD_PATH);
    } else {
        path = xmalloc(strlen(PE_SYS_CMD_PATH) + 1);
        rval = snprintf(path, strlen(PE_SYS_CMD_PATH)+1, PE_SYS_CMD_PATH);
    }
    if (rval < 0)
    {
        VLOG_ERR("snprintf failed for cmd %s and returned %d at %i\n",
                 pe_ovs_command_array[cmd], rval,__LINE__);
    } else {
        /* allocate space and copy base path into command string */
        dest = xmalloc(strlen(path));
        if ((rval = snprintf(dest,strlen(path)+1,path))
            < 0) {
            VLOG_ERR("snprintf failed and returned %d at %i\n",rval,__LINE__);
            dest = NULL;
        } else {
            /* at this point the string has the base path in it
             * increase size by 1 for final \0
             */
            size_t str_size = strlen(dest) + 1;

            VLOG_DBG("dest/size \"%s\"/%d str_size %d\n",
                       dest, strlen(dest), str_size);

            /* now add the "/" */
            str_size += strlen(PE_FILENAME_SEPARATOR);
            dest = realloc(dest,str_size);
            strncat(dest,PE_FILENAME_SEPARATOR,strlen(PE_FILENAME_SEPARATOR));

            VLOG_DBG("dest/size \"%s\"/%d str_size %d\n",
                       dest, strlen(dest), str_size);

            /* add command */
            str_size += strlen(pe_ovs_command_array[cmd]);
            dest = xrealloc(dest,str_size);
            strncat(dest, pe_ovs_command_array[cmd],
                    strlen(pe_ovs_command_array[cmd]));

            VLOG_DBG("dest/size \"%s\"/%d str_size %d\n",
                       dest, strlen(dest), str_size);
        
            /* add options to command */
            for (count = 0; count < nr_opts; count++) {
                str_size += (strlen(PE_SINGLE_SPACE) +
                             strlen(v_opts[count]));
                dest = xrealloc(dest,str_size);
                strncat(dest, PE_SINGLE_SPACE, strlen(PE_SINGLE_SPACE));
                strncat(dest, v_opts[count], strlen(v_opts[count]));
            }

            VLOG_DBG("dest/size \"%s\"/%d str_size %d\n",
                       dest, strlen(dest), str_size);
         
            /* add space then sub-command */
            str_size += strlen(PE_SINGLE_SPACE);
            dest = xrealloc(dest,str_size);
            strncat(dest,PE_SINGLE_SPACE, strlen(PE_SINGLE_SPACE));
            str_size += strlen(pe_ovs_subcommand_array[cmd]);
            dest = xrealloc(dest,str_size);
            strncat(dest, pe_ovs_subcommand_array[cmd],
                          sizeof(pe_ovs_subcommand_array[cmd]));

            VLOG_DBG("dest/size \"%s\"/%d str_size %d\n",
                       dest, strlen(dest), str_size);

            /* add arguments to subcommand */
            for (count = 0; count < nr_args; count++) {
                str_size += (strlen(PE_SINGLE_SPACE) +
                             strlen(v_args[count]));
                dest = xrealloc(dest,str_size);
                strncat(dest, PE_SINGLE_SPACE, strlen(PE_SINGLE_SPACE));
                strncat(dest, v_args[count], strlen(v_args[count]));
            }

            VLOG_DBG("dest/size \"%s\"/%d str_size %d\n",
                       dest, strlen(dest), str_size);

            /* Make sure the string is terminiated */
            dest[str_size] = '\0';

            VLOG_DBG("dest/size \"%s\"/%d str_size %d\n",
                       dest, strlen(dest), str_size);
        }
    }
    VLOG_LEAVE(__func__);
    return(dest);
}
static void
pe_ovs_command_exec(char *command, pe_command_results_t *result) {
    char response[PE_OVS_MAX_RESPONSE_SIZE];
    FILE *pipe_fs;

    VLOG_ENTER(__func__);

    pipe_fs = pipe_read(command);
    
    if (VLOG_IS_DBG_ENABLED() == true) {
        while(fgets(response, sizeof(response), pipe_fs)) {
            VLOG_DBG("Response of %s to command %s\n", response, command);
        }
    }

    //result = WEXITSTATUS(pipe_close(pipe_fs));
    result->retcode = pipe_close_na(pipe_fs, &result->err_no);

    VLOG_DBG("Execution of %s returned exit status %i\n",
               command, result->retcode);

    free(command);

    VLOG_LEAVE(__func__);
}
