/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#define USE_VLOG 1

#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>

#include "misc-util.h"
#include "compiler.h"
#include "daemon.h"
#include "dirs.h"
#include "dynamic-string.h"
#include "fatal-signal.h"
#include "reconnect.h"
#include "json.h"
#include "jsonrpc.h"
#include "list.h"
#include "poll-loop.h"
#include "sort.h"
#include "svec.h"
#include "ovs-thread.h"
#include "unixctl.h"
#include "pol-mgmt.h"
#include "ofpbuf.h"
#include "sess.h"
#include "opflex.h"
#include "eventq.h"
#include "skt-util.h"
#include "tv-util.h"
#include "util.h"
#include "config-file.h"
#include "vlog.h"


VLOG_DEFINE_THIS_MODULE(opflex);

/*
 * opflex - opflex protocol utilities.
 *
 * History:
 *    30-JUL-2014 dkehn@noironetworks.com - created.
 *
 */


/*
 * Local dcls
 */
static pthread_t opflex_server_thread_id;
static bool opflex_server_running = false;

/*
 * Protos dcls
 */
static char *opflex_dcmd_tomethod(enum_opflex_dcmd dcmd);
static char *opflex_dcmd_tostring(enum_opflex_dcmd dcmd);
static enum_opflex_dcmd opflex_method_to_dcmd(char *method);
static void *opflex_server_thread(void *arg);


/* ===========================================================================
 * PUBLIC ROUTINES
 * ======================================================================== */

/* 
 * opflex_server_create - create the server to listen on the designated 
 * (config file port).
 *
 */
bool opflex_server_create(char *port)
{
    static char *mod = "opflex_server_create";
    bool retb = false;
    int retc;

    ENTER(mod);

    /* create the server thread */
    
    retc = pthread_create(&opflex_server_thread_id, NULL,
                          opflex_server_thread, port);
    if (retc) {
        VLOG_ERR("%s: can't create the OpFlex Server thread: %s",
                 mod, strerror(errno));
        retb = true;
    }

    LEAVE(mod);
    return(retb);
}

/* 
 * opflex_server_destroy
 *
 */
void opflex_server_destroy(void)
{
    static char *mod = "opflex_server_create";
    bool retb = false;
    int retc;

    ENTER(mod);

    opflex_server_running = false;

    LEAVE(mod);
    return(retb);
}

/*
 * opflex_send - send the opflex specified command to the target.
 *
 * where:
 * @param0 <opfcmd>                       - I
 *         opflex command (enum_opflex_cmd).
 */
char *opflex_recv(char *target, long timeout_secs)
{
    static char *mod = "opflex_recv";
    char *opflex_string = NULL;

    ENTER(mod);
    VLOG_DBG("%s: name:%s tmout: %ld", mod, target, timeout_secs);
    opflex_string = sess_recv(target, timeout_secs, true);
    VLOG_INFO("%s: recv msg: %s", mod, opflex_string);
    LEAVE(mod);
    return(opflex_string);
}

/*
 * opflex_send - send the opflex specified command to the target.
 *
 * where:
 * @param0 <opfcmd>                       - I
 *         opflex command (enum_opflex_cmd).
 */
bool opflex_send(enum_opflex_dcmd opfcmd, char *target)
{
    static char *mod = "opflex_send";
    bool retb = false;
    struct jsonrpc_msg *request;
    char *pe_name = NULL;
    char *domain = NULL;
    char *role = NULL;
    struct json *params, *transaction;
    char *method_str = opflex_dcmd_tomethod(opfcmd);
    session_p sessp;

    ENTER(mod);

    if ((sessp = sess_get(target)) == NULL) {
        VLOG_ERR("%s: no active session: %s", mod, target);
        retb = true;
        goto rtn_return;
    }

    switch (opfcmd) {
    case OPFLEX_DCMD_IDENTIFY:
        transaction = json_array_create_empty();
        params = json_object_create();
        pe_name = conf_get_value(PM_SECTION, "pe_name");
        domain = conf_get_value(PM_SECTION, "pe_domain");
        role = conf_get_value(PM_SECTION, "pe_role");

        json_object_put_string(params, "name", pe_name);
        json_object_put_string(params, "domain", domain);
        json_object_put_string(params, "my_role", role);
        json_array_add(transaction, params);

        request = jsonrpc_create_request(method_str, transaction, NULL);

        jsonrpc_session_send(sessp->jsessp, request);

        break;
    case OPFLEX_DCMD_ECHO:
        params = json_object_create();
        json_object_put_string(params, "echo_msg",
                               "dkehn: AUG-01-2014");
        request = jsonrpc_create_request(method_str, params, NULL);
        break;
    case OPFLEX_DCMD_POL_RES:
    case OPFLEX_DCMD_POL_UPD:
    case OPFLEX_DCMD_POL_TRIG:
    case OPFLEX_DCMD_EP_DCL:
    case OPFLEX_DCMD_EP_RQST:
    case OPFLEX_DCMD_EP_POL_UPD:
    case OPFLEX_DCMD_CLOSE:
        VLOG_WARN("%s: OpFlex command: %s, not supported yet.",
                  mod, opflex_dcmd_tostring(opfcmd));
        break;
    default:
        VLOG_ERR("%s: unknow opflex command: %d", mod, opfcmd);
        break;
    }

 rtn_return:
    LEAVE(mod);
    return(retb);
}

/*
 * oplex_dispatcher - the sess_epoll_monitor_thread calles this routine to read
 * the message from the pipe once epoll has detect that there is data from a
 * session. This will read the data and dispatch on the mothod in the message.
 *
 * where:
 * @param0 <sessp>                   - I
 *         sessp is the session structure from the epoll and the session list
 *         from which the message is from.
 * @returns: false (0) is all is good, else true (1).
 *
 */
bool opflex_dispatcher(session_p sessp)
{
    static char *mod = "opflex_dispatcher";
    bool retb = false;
    char *name;
    struct jsonrpc_msg *msg;
    long timeout_secs = 10;
    pthread_t my_pid = pthread_self();
    enum_opflex_dcmd dcmd;

    /* make sure there is something in the pipe */
    name = (char *)jsonrpc_session_get_name(sessp->jsessp);
    if (skt_is_readable(name, sessp->fd) == 0) {
        VLOG_ERR("%s: dispatch on session: %s, but nothing in the pipe.", mod,
                 name);
        retb = true;
        goto rtn_return;
    }
    msg = (struct jsonrpc_msg *)sess_recv(name, timeout_secs, false);
    VLOG_INFO("%s:%ld recv method:%s", mod, my_pid, msg->method);
    if (msg->type == JSONRPC_REQUEST) {
        if ((dcmd = opflex_method_to_dcmd(msg->method)) == -1) {
            VLOG_ERR("%s: unknown OpFlex method:%s ", mod, msg->method);
            retb = true;
            goto rtn_return;
        }

        switch (dcmd) {
        case OPFLEX_DCMD_IDENTIFY:
        case OPFLEX_DCMD_ECHO:
        case OPFLEX_DCMD_POL_RES:
        case OPFLEX_DCMD_POL_UPD:
        case OPFLEX_DCMD_POL_TRIG:
        case OPFLEX_DCMD_EP_DCL:
        case OPFLEX_DCMD_EP_RQST:
        case OPFLEX_DCMD_EP_POL_UPD:
        case OPFLEX_DCMD_CLOSE:
            VLOG_WARN("%s: OpFlex command: %s, not supported yet.",
                      mod, msg->method);
            break;
        default:
            VLOG_ERR("%s: I do not understand this opflex command: %s",
                     mod, msg->method);
            break;
        }
    }
    else if (msg->type == JSONRPC_REPLY) {
        VLOG_INFO("%s: recved JSONRPC_REPLY: %s", mod,
                 json_to_string(msg->result, 0));
    }
    else if (msg->type == JSONRPC_NOTIFY) {
        VLOG_INFO("%s: recved JSONRPC_NOTIFY: %s", mod,
                 json_to_string(msg->result, 0));
    }
    else if (msg->type == JSONRPC_ERROR) {
        VLOG_ERR("%s: recved JSONRPC_ERROR: %s", mod,
                 json_to_string(msg->result, 0));
    }
    else {
        VLOG_ERR("%s: recved ?????????: %s", mod,
                 json_to_string(msg->result, 0));
    }
    jsonrpc_msg_destroy(msg);
    
 rtn_return:
    LEAVE(mod);
    return(retb);
}

/*
 * opflex_list_delete - delete a opflex list.
 *
 */
void opflex_list_delete(struct list *list)
{
    static char *mod = "opflex_list_delete";
  
    ENTER(mod);
    struct ofpbuf *b, *next;

    LIST_FOR_EACH_SAFE (b, next, list_node, list) {
        list_remove(&b->list_node);
        free(b);
        ofpbuf_delete(b);
    }

    LEAVE(mod);
}

/* ============================================================================
 * LOCAL 
 * ========================================================================== */

/* 
 * opflex_server_thread - this is the routine that performs the listen and
 * accpet then hands it off to the epoll thread for anything else.
 *
 */
static void *opflex_server_thread(void *arg)
{
    static char *mod = "opflex_server_thread";
    char *port = arg;
    session_p svr_sessp = NULL;
    int error;
    

    ENTER(mod);

    svr_sessp = sess_open(port, SESS_TYPE_SERVER, SESS_COMM_ASYNC);
    if (!svr_sessp) {
        VLOG_FATAL("%s: Can't create session for server on: %s", mod, port);
        svr_sessp = NULL;
        goto rtn_return;
    }
    for (opflex_server_running = true; opflex_server_running; ) {
        VLOG_DBG("%s: opflex_server running on: %s", mod, port);
        error = sess_server_wait(svr_sessp, true);
        if (error)
            VLOG_ERR("%s; opflex server:%s error:%s", mod, port, 
                     strerror(error));

    }

 rtn_return:
    LEAVE(mod);
    return(NULL);
}

/* opflex_dcmd_tostring.
 *
 */
static char *opflex_dcmd_tostring(enum_opflex_dcmd dcmd)
{
    static char *dcmd_tostring_lookup[] = {
        "Identity",
        "Policy Resolution",
        "Policy Update",
        "Echo",
        "Policy Trigger",
        "Endpoint Declaration",
        "Endpoint Request",
        "Endpoint Policy Update",
        "Close Seesion",
        "Kill All",
    };
    return(dcmd_tostring_lookup[dcmd]);
}

/*
 * opflex_method_to_dcmd - use for when a msg comes in to match to the enum
 *
 */
static enum_opflex_dcmd opflex_method_to_dcmd(char *method)
{
    static char *dcmd_tomethod_lookup[] = {
        "send_identity",
        "resolve_policy",
        "update_policy",
        "echo",
        "trigger_policy",
        "endpoint_declaration",
        "endpoint_request",
        "endpoint_update_policy",
        "close",
        "killall",
        NULL
    };
    int i;

    for (i=0; dcmd_tomethod_lookup[i]; i++) {
        if (strcasecmp(method, dcmd_tomethod_lookup[i]) == 0)
            break;
    }
    if (dcmd_tomethod_lookup[i] == NULL)
        i = -1;
    return(i);
}

/* opflex_dcmd_tomethod - see RFC draft-smith-opflex-00.
 *
 */
static char *opflex_dcmd_tomethod(enum_opflex_dcmd dcmd)
{
    static char *dcmd_tomethod_lookup[] = {
        "send_identity",
        "resolve_policy",
        "update_policy",
        "echo",
        "trigger_policy",
        "endpoint_declaration",
        "endpoint_request",
        "endpoint_update_policy",
    };
    return(dcmd_tomethod_lookup[dcmd]);
}
