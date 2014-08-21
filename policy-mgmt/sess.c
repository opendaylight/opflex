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
#include "poll-loop.h"
#include "ofpbuf.h"
#include "sort.h"
#include "svec.h"
#include "shash.h"
#include "socket-util.h"
#include "ovs-thread.h"
#include "unixctl.h"
#include "pol-mgmt.h"
#include "sess.h"
#include "eventq.h"
#include "tv-util.h"
#include "util.h"
#include "config-file.h"
#include "opflex.h"
#include "skt-util.h"
#include "fnm-util.h"
#include "meo-util.h"
#include "vlog.h"


VLOG_DEFINE_THIS_MODULE(session);

/*
 * sess - session management and control
 *
 * History:
 *    24-JUN-2014 dkehn@noironetworks.com - created.
 *
 */


/*
 * Local dcls
 */
static bool sess_initialized = false;

static pthread_t sess_mgr_thread;
static bool session_worker_thread_running = false;

static bool sess_epoll_thread_running = false;
static pthread_t sess_epoll_thread;

static sess_manage_p sess_info;
static char *not_init_msg = "%s: session layer has not been intialized.";

/* NOTE: locally defined structs that we hidden within the ovs code base, due
 * to there weird class hiding architecture. To get meaningful use from them
 * about the lower layer they are defined here.
 */
struct stream_fd {
    struct stream stream;
    int fd;
};

struct jsonrpc {
    struct stream *stream;
    char *name;
    int status;

    /* Input. */
    struct byteq input;
    uint8_t input_buffer[512];
    struct json_parser *parser;

    /* Output. */
    struct list output;
    size_t backlog;
};

struct jsonrpc_session {
    struct reconnect *reconnect;
    struct jsonrpc *rpc;
    struct stream *stream;
    struct pstream *pstream;
    int last_error;
    unsigned int seqno;
    uint8_t dscp;
};

/*
 * Protos dcls
 */
static void *session_worker_thread (void *arg);
static struct jsonrpc *sess_open_jsonrpc(const char *server);
static struct jsonrpc_session *sess_open_jsonsession(char *server,
                                                     enum_sess_comm_type sess_type);
static bool sess_epoll_modify(session_p sessp, bool add_flag);
static void *sess_epoll_monitor_thread(void *sip_);
static session_p sess_get_with_fd(int fd);

static int sess_list_mod_count(enum_counter_direction inc_flag,
                               enum_locking lock_flag);

static inline struct stream_fd *stream_fd_cast(struct stream *stream)
{
    return CONTAINER_OF(stream, struct stream_fd, stream);
}

static bool sess_list_add(session_p sessp, char *name);
static void jsonrpc_session_connect(struct jsonrpc_session *s);
static void jsonrpc_session_disconnect(struct jsonrpc_session *s);
static void sess_jsonrpc_session_close(struct jsonrpc_session *s);

static void jsonrpc_error(struct jsonrpc *rpc, int error);
static void jsonrpc_cleanup(struct jsonrpc *rpc);


/* ===========================================================================
 * PUBLIC ROUTINES
 * ======================================================================== */

/*
 * sess_initialize - setup the sess_manager and protocol handlers, etc.
 *
 */
bool sess_initialize(void)
{
    static char *mod = "sess_initialize";
    bool retb = false;
    sess_manage_p sip = NULL;
    char *df_type_string;
    int retc;

    ENTER(mod);

    if (!sess_is_initialized()) {

        /* create the session mamangement info structure
         */
        sip = xzalloc(sizeof(sess_manage_t));
        sess_info = sip;
        ovs_rwlock_init(&sip->rwlock);
        ovs_rwlock_wrlock(&sip->rwlock);

        /* Setup the protocol
         */
        df_type_string = conf_get_value(PM_SECTION, "sess_protocol");
        if (strlen(df_type_string)) {
            if (strcasecmp(df_type_string, "json") == 0) {
                sip->df_type = DF_JSON;
            }
            else if (strcasecmp(df_type_string, "xml") == 0) {
                sip->df_type = DF_XML;
            }
            else {
                VLOG_FATAL("%s: unsupport sess_protocol type: %s",
                           mod, df_type_string);
                free(sip);
                retb = true;
                goto rtn_return;
            }
        }
        else {
            sip->df_type = DF_JSON;
        }

        sip->max_sessions = atoi(conf_get_value(PM_SECTION,
                                                "max_active_sessions"));

        shash_init(&sip->sess_list);
        ovs_rwlock_init(&sip->sess_list_rwlock);
        ovs_rwlock_wrlock(&sip->sess_list_rwlock);

        /*
         * setup the session_work_q & threads
         */
        sip->sess_q_depth = atoi(conf_get_value(PM_SECTION,
                                                "session_queue_depth"));
        if (init_workpool(sip->sess_q_depth, sizeof(session_t), 1,
                          sip->sess_q_depth, &sip->sess_event_q)) {
            VLOG_FATAL("%s: Can't allocate the session event "
                       "queue of depth: %d.",
                       mod, sip->sess_q_depth);
            retb = true;
        }

        /*
         * create the work crew 
         */
        sip->max_sess_threads = atoi(conf_get_value(PM_SECTION,
                                                    "max_session_threads"));
        sip->workq = xzalloc(sizeof(crew_t));
        if (crew_create(sip->workq, sip->max_sess_threads,
                        sip->max_sess_threads, session_worker_thread)) {
            VLOG_FATAL("%s: Can't allocate the session work queue of size: %d.",
                       mod, sip->max_sess_threads);
            retb = true;
        }

        /*
         * start the sess_epoll_thread, this basically start with no
         * FDs in the monitoring pool and as sess_open occurr FDs will
         * be put inot it.
         */
        retc = pthread_create(&sess_epoll_thread, NULL,
                              sess_epoll_monitor_thread, sip);
        if (retc) {
            VLOG_FATAL("%s: error creating sess_epoll_monitor_thread: %d",
                       mod, retc);
            retb = true;
            goto rtn_return;
        }

        ovs_rwlock_unlock(&sip->sess_list_rwlock);
        ovs_rwlock_unlock(&sip->rwlock);
        sess_initialized = true;
    }
    else {
        VLOG_ERR("%s: Session already initialized", mod);
        retb = true;
    }
 rtn_return:
    LEAVE(mod);
    return(retb);
}

/*
 * sess_open - this creates and opens a session to the name
 * <protocol>:server:port> and adds it to the sess_list. Adds it
 * to the epoll_wait and adds it as session to the sess_list. If
 * this session already exists will not create a new one.
 *
 * where:
 *  @param0 <name>                - I
 *          This will either be a server or client side pointer to the session
 *          point.
 *  @param1 <sess_type>            - I
 *          Identifies the json session pointer enum_sess_type.
 *  @param2 <df_type>              - I
 *          Defines the type of serializer that will be used.
 * @returns <jsp>                  - O 
 *          session pointer or NULL if error. In the case of a client
 *          this is the session_p in the session_list, in the case of the
 *          server its is not in the session_list.
 *
 */
session_p
sess_open(char *name, enum_sess_type sess_type, enum_sess_comm_type ctype)
{
    static char *mod = "sess_add";
    struct jsonrpc_session *jsp = NULL;
    sess_manage_p sip = sess_info;
    session_p sessp = NULL;
    struct stream_fd *s;
    int current_sessions;

    ENTER(mod);
    VLOG_INFO("%s: server %s", mod, name);

    if (sess_is_initialized()) {
        current_sessions = sess_get_session_count();
        if (current_sessions >= sip->max_sessions) {
            VLOG_WARN("%s: session max_session reached: max:%d current:%d",
                      mod, sip->max_sessions, current_sessions);
            sessp = NULL;
            goto rtn_return;
        }
    
        if (sess_type == SESS_TYPE_CLIENT) {
            /* see if this is already there */
            if ((sessp = sess_list_search(name))) {
                VLOG_INFO("%s: %s already exists.", mod,
                          jsonrpc_session_get_name(sessp->jsessp));
                sessp = NULL;
                goto rtn_return;
            }
            else {
                /* let try and connect */
                if ((jsp = sess_open_jsonsession(name, ctype)) == NULL) {
                    VLOG_ERR("%s: failed json session to %s.", mod, name);
                    sessp = NULL;
                    goto rtn_return;
                }

                VLOG_INFO("%s: jsonrpc session open to:%s status:%d", mod,
                          name, jsonrpc_session_get_status(jsp));
                s = stream_fd_cast(jsp->stream);
                sessp = xzalloc(sizeof(session_t));
                ovs_rwlock_init(&sessp->rwlock);
                ovs_rwlock_wrlock(&sessp->rwlock);
                sessp->sess_start_ts = tv_tod();
                sessp->fd = s->fd;
                sessp->comm_type = ctype;
                sessp->jsessp = jsp;
                ovs_rwlock_unlock(&sessp->rwlock);

                /* add it to the sessin list */
                if (sess_list_add(sessp, name)) {
                    VLOG_WARN("%s: cant add %s to the sess_list", mod, name);
                    sess_jsonrpc_session_close(jsp);
                    free(sessp);
                    sessp = NULL;
                    goto rtn_return;
                }
                /* if this is an async session need to add it to the
                 * monitoring epoll, if its sync then we do not add it.
                 */
                if (ctype == SESS_COMM_ASYNC) {
                    if (sess_epoll_modify(sessp, true)) {
                        VLOG_WARN("%s: can't add %s to epoll monitor list", mod,
                                  name);
                        sessp = NULL;;
                    }
                }
                if (sessp) {
                    sess_list_mod_count(CNTR_INC, WITH_LOCKING);
                }
            }
        }
        else {
            /* creates and opens up a session for the server, this will not be
             * put into the session list, its connections from the outside
             * will be put into the session_list. Upon returning the session 
             * struct is completed but the call must do the listen/accept.
             */
            struct pstream *listener;
            int error;
            
            if ((jsp = sess_open_jsonsession(name, ctype)) == NULL) {
                VLOG_ERR("%s: failed creating the json session for the server:%s.",
                         mod, name);
                sessp = NULL;
                goto rtn_return;
            }
            
            sessp = xzalloc(sizeof(session_t));
            ovs_rwlock_init(&sessp->rwlock);
            ovs_rwlock_wrlock(&sessp->rwlock);
            sessp->sess_start_ts = tv_tod();
            sessp->fd = 0;
            sessp->comm_type = ctype;
            sessp->jsessp = jsp;

            error = jsonrpc_pstream_open(name, &listener, 0);
            if (error && error != EAFNOSUPPORT) {
                VLOG_ERR("%s: listen failed on: %s error:%s",
                         mod, name, strerror(error));
                sessp = NULL;
                goto rtn_return;
            }
            sessp->jsessp->pstream = listener;
            ovs_rwlock_unlock(&sessp->rwlock);
        }
    }
    else {
        VLOG_ERR(not_init_msg, mod);
        sessp = NULL;
    }

 rtn_return:
    LEAVE(mod);
    return(sessp);
}

/*
 * sess_get - this is going to determine if there is an active session in the sess_list
 * for a possible existing connection, will return the sess_list slot back to the caller if
 * one exists, else NULL.
 *
 * where:
 * @param0 <name>               - I
 *         this will be the name of whom we want to establish a session too.
 * @returns: <sessp>            - O
 *         this will either be loaded with the session, if one exists, or NULL.
 */
session_p sess_get(char *name)
{
    static char *mod = "sess_get";
    session_p sessp = NULL;
    sess_manage_p sip = sess_info;
    struct shash_node *shnp = NULL;

    ENTER(mod);

    if (sess_is_initialized()) {
        ovs_rwlock_rdlock(&sip->sess_list_rwlock);
        shnp = shash_find(&sip->sess_list, name);
        if (shnp) {
            sessp = (session_p)shnp->data;
            VLOG_DBG("%s: found %s in the sess_list", mod, name);
        }
        ovs_rwlock_unlock(&sip->sess_list_rwlock);
    }
    else {
        VLOG_ERR(not_init_msg, mod);
    }

    LEAVE(mod);
    return(sessp);
}

/*
 * sess_close - this will close & delete the session from the
 *  sess_list, epoll monito and close the json channel.
 *
 * where:
 *  @param0 <name>            - I
 *          the name of the session that we are deleteing.
 * returns: false if everything ok, else 1
 *
 */
bool sess_close(char *name)
{
    static char *mod = "sess_close";
    bool retb = false;
    session_p sessp = NULL;
    sess_manage_p sip = sess_info;
    struct shash_node *sndp = NULL;

    ENTER(mod);

    if (sess_is_initialized()) {
        ovs_rwlock_wrlock(&sip->sess_list_rwlock);
        sndp = shash_find(&sip->sess_list, name);

        if (sndp) {
            sessp = (session_p)sndp->data;
            shash_delete(&sip->sess_list, sndp);
        }

        ovs_rwlock_unlock(&sip->sess_list_rwlock);

        if (sessp) {
            sess_jsonrpc_session_close(sessp->jsessp);
            sess_list_mod_count(CNTR_DEC, WOUT_LOCKING);
            sess_epoll_modify(sessp, false);
            free(sessp);
        }
        else {
            VLOG_WARN("%s: can't delete %s from the sess_list.", mod, name);
            retb = true;
            goto rtn_return;
        }
    }
    else {
        VLOG_ERR(not_init_msg, mod);
        retb = true;
    }

 rtn_return:
    LEAVE(mod);
    return(retb);
}

/*
 * sess_is_intiialied - returns the flag
 */
bool sess_is_initialized(void)
{
    return(sess_initialized);
}

/*
 * sess_is_connected - determins if the session is connected.
 *
 * where:
 * @param0 <name>               - I
 *         name of the previously connected session name.
 * @returns true if connected else false.
 *
 */
bool sess_is_alive(char *name)
{
    static char *mod = "sess_is_connected";
    bool retb = false;
    session_p sessp;

    ENTER(mod);

    if ((sessp = sess_get(name))) {
        retb = jsonrpc_session_is_alive(sessp->jsessp);
    }
    else {
        VLOG_WARN("%s: this session is not connected: %s",
                  mod, name);
        retb = false;
    }

    VLOG_DBG("%s: name:%s retb=%d", mod, name, retb);
    LEAVE(mod);
    return(retb);
}

/*
 * sess_is_connected - determins if the session is connected.
 *
 * where:
 * @param0 <name>               - I
 *         name of the previously connected session name.
 * @returns true if connected else false.
 *
 */
bool sess_is_connected(char *name)
{
    static char *mod = "sess_is_connected";
    bool retb = false;
    session_p sessp;
    int retc;
    char *peer;
    
    ENTER(mod);

    if ((sessp = sess_get(name))) {
        retc = skt_is_up(name, sessp->fd);
        if (retc) {
            peer = skt_peer(name, sessp->fd);
            VLOG_INFO("%s: is up connected: %d", mod, retc);
            retb = true;
        }

        else {
            VLOG_INFO("%s: this session is not connected: %s",
                      mod, name);
            retb = false;
        }
    }

    VLOG_DBG("%s: name:%s retb=%d", mod, name, retb);
    LEAVE(mod);
    return(retb);
}

/*
 * sess_recv - recv a message from the session.
 *
 * where:
 *   @param0 <name>             - I
 *           name of the session.
 *   @param1 <timeout_secs>     - I
 *           timeout in seconds.
 *   @returns <ret_string>     - O
 *            string or NULL if error.
 *
 */
void *sess_recv(char *name, long timeout_secs, bool cvt_to_string)
{
    static char *mod = "sess_recv";
    void *ret_dp = NULL;

    struct jsonrpc_msg *msg;
    session_p sessp;
    struct timeval end, ctime;
    int timeout, retc;

    ENTER(mod);

    if ((sessp = sess_get(name)) == NULL) {
        VLOG_ERR("%s: no active session named: %s", mod, name);
        ret_dp = NULL;
        goto rtn_return;
    }

    end = tv_add(tv_tod(), tv_create(timeout_secs, 0));
    for (timeout = 1; timeout; ) {
        retc = jsonrpc_recv(sessp->jsessp->rpc, &msg);
        if (retc == EAGAIN) {
            ctime = tv_subtract(end, tv_tod());
            if (ctime.tv_sec <= 0 && ctime.tv_usec <= 0) {
                timeout = 0;
            }
        }
        else if (retc != 0) {
            VLOG_ERR("%s: bad recv json msg: %d", mod, retc);
            ret_dp = NULL;
            break;
        }
        else {
            if (cvt_to_string) {
                ret_dp = json_to_string(msg->result, 0);
            }
            else {
                ret_dp = (void *)msg;
            }
            break;
        }
    }  /* for */
 rtn_return:
    LEAVE(mod);
    return(ret_dp);
}

/*
 * sess_send - sends a message to the previously opened session,
 *
 * where:
 * @param0 <sess_name>                - I
 *         a previously opened session name.
 * @param1 <msg>                      - I
 *         this is a message structure that will be cast by the session handler
 * @returns flase, all is good else, true.
 *
 */
bool sess_send(char *sess_name, void *msg)
{
    static char *mod = "sess_send";
    bool retb = false;
    session_p sessp = NULL;
    sess_manage_p sip = sess_info;

    ENTER(mod);

    if (!sess_is_initialized()) {
        VLOG_WARN("%s: can't delete %s from the sess_list.",
                  mod, sess_name);
        retb = true;
        goto rtn_return;
    }

    if ((sessp = sess_get(sess_name)) == NULL) {
        VLOG_WARN("%s: No session open with name: %s.",
                  mod, sess_name);
        retb = true;
        goto rtn_return;
    }

    if (sip->df_type == DF_JSON) {
        if (jsonrpc_session_send(sessp->jsessp, (struct jsonrpc_msg *)msg)) {
            VLOG_ERR("%s: can't send message to %s", mod, sess_name);
            retb = true;
            goto rtn_return;
        }
    }
    else {
        VLOG_WARN("%s: unsupported protocol: %d", mod, sip->df_type);
            retb = true;
            goto rtn_return;
    }

 rtn_return:
    LEAVE(mod);
    return(retb);
}

/* 
 * sess_destroy - cleans the sess layer up.
 *
 */
void sess_cleanup(void) 
{
    static char *mod = "sess_cleanup";
    eventq_ele_t *loc_ep = NULL;
    sess_manage_p sip = sess_info;

    ENTER(mod);

    if (sess_is_initialized()) {
        /* TODO: dkehn - go through every active session and terminate it */
        ovs_rwlock_wrlock(&sip->sess_list_rwlock);

        /* remove everything from the event q and release it.
         */
        while ((loc_ep = get_event_from_queue(sip->sess_event_q)) != NULL) {
            if (loc_ep->dp) {
                free(loc_ep->dp);
            }
            free(loc_ep);
        }
        free(sip->sess_event_q);

        /*
         * Terminate all the workers
         */
        crew_destroy(sip->sess_event_q, sip->workq);
        free(sip->workq);

        ovs_rwlock_destroy(&sip->sess_list_rwlock);
        free(sip);
        sess_initialized = false;
    }
    else {
        VLOG_ERR(not_init_msg, mod);
    }

    LEAVE(mod);
    return;
}

/* 
 * sess_list_search - searches the sess_list for name and if foound returns the session
 * pointer.
 *
 * where:
 * @param0 <name>                - I
 *         the name of the session <protocol:server:port>
 * @return: <sessp>              - O
 *          the session pointer from the sess_list.
 *
 */
 session_p sess_list_search(char *name)
 {
     static char *mod = "sess_list_search";
     session_p sessp = NULL;
     sess_manage_p sip = sess_info;
     struct shash_node *shnp = NULL;

     ENTER(mod);
     if (sess_is_initialized()) {
         ovs_rwlock_rdlock(&sip->sess_list_rwlock);
         shnp = shash_find(&sip->sess_list, name);
         if (shnp) {
         /* if (hash_search(sip->sess_list, name, (void *)&sessp)) { */
             sessp = (session_p)shnp->data;
             VLOG_DBG("%s: %s already exists.", mod, name);
         }
         ovs_rwlock_unlock(&sip->sess_list_rwlock);
     }
     else {
        VLOG_ERR(not_init_msg, mod);
    }
     LEAVE(mod);
     return(sessp);
}

/*
 * sess_list_get_session_count - returns the current sess_list_count, this is
 * how many active connected sessions are active.
 *
 * where:
 * @returns <count>           - O
 */
int sess_get_session_count(void)
{
    static char *mod = "sess_list_get_session_count";
    int retc = 0;
    sess_manage_p sip = sess_info;

    ENTER(mod);
    if (sess_is_initialized()) {
        ovs_rwlock_rdlock(&sip->sess_list_rwlock);
        retc = sip->sess_list_count;
        ovs_rwlock_unlock(&sip->sess_list_rwlock);
        VLOG_DBG("%s: retc=%d", mod, retc);
    }
    else {
        VLOG_ERR(not_init_msg, mod);
    }
    LEAVE(mod);
    return(retc);
}

/*
 * sess_server_wait - this waits for a connection and if instructed puts the 
 * session into the session_list and epoll thread list.
 *
 * where:
 * @param0 <srv_sessp>            - I
 *         this is the server session_p definition that has been
 *         previously setup.
 * @param1 <add_to_list>          - I
 *         true, the new connected client is added to the session list and 
 *         epoll rooutine, false its dropped.
 *
 */
int sess_server_wait(session_p srv_sessp, bool add_to_list)
{
    static char *mod = "sess_server_wait";
    sess_manage_p sip = sess_info;
    session_p sessp = NULL;
    struct stream_fd *s;
    struct stream *stream;
    struct jsonrpc_session *jsp;
    char *name;
    int error;

    ENTER(mod);
    name = jsonrpc_session_get_name(srv_sessp);
    error = pstream_accept(srv_sessp->jsessp->pstream, &stream);
    if (!error) {
        if (sess_get_session_count() < sip->max_sessions) {
            VLOG_ERR("%s: max sessions reached.", mod);
            error = EUSERS;
        }
        else {
            jsp = xmalloc(sizeof *jsp);
            jsp->reconnect = reconnect_create(time_msec());
            reconnect_set_name(jsp->reconnect, name);
            reconnect_enable(jsp->reconnect, time_msec());
            jsp->rpc = NULL;
            jsp->stream = stream;
            jsp->pstream = NULL;
            jsp->seqno = 0;
            jsp->dscp = 0;
            jsp->last_error = 0;

            s = stream_fd_cast(stream);
            sessp = xzalloc(sizeof(session_t));
            ovs_rwlock_init(&sessp->rwlock);
            ovs_rwlock_wrlock(&sessp->rwlock);
            sessp->sess_start_ts = tv_tod();
            sessp->fd = s->fd;
            sessp->comm_type = SESS_COMM_ASYNC;
            sessp->jsessp = jsp;
            ovs_rwlock_unlock(&sessp->rwlock);
            if (sess_list_add(sessp, name)) {
                VLOG_WARN("%s: cant add %s to the sess_list", mod, name);
                sess_jsonrpc_session_close(jsp);
                free(sessp);
                error = ENOSR;
                goto rtn_return;
            }
            if (sess_epoll_modify(sessp, true)) {
                VLOG_WARN("%s: can't add %s to epoll monitor list: %s", mod,
                          name, strerror(errno));
                error = errno;
            }
            if (sessp) {
                sess_list_mod_count(CNTR_INC, WITH_LOCKING);
            }
        }
    }
 rtn_return:
    LEAVE(mod);
    return(error);
}

/* ============================================================================
 * Private Routines
 * ========================================================================= */

/*
 * sess_get_with_fd - searches the session list for the matching fd and
 * returns it.
 *
 * where:
 * @param0 <sfd>                  - I
 *         fd to search for.
 * @returns session_p if found or NULL if not found.
 */
static session_p sess_get_with_fd(int fd)
{
    static char *mod = "sess_get_with_fd";
    session_p sessp = NULL;
    struct shash_node *node;

    ENTER(mod);

    SHASH_FOR_EACH(node, &sess_info->sess_list) {
        if (((session_p)node->data)->fd == fd) {
            sessp = (session_p)node->data;
            break;
        }
    }

    VLOG_DBG("%s: sessp: %p", mod, sessp);
    LEAVE(mod);
    return(sessp);
}
/*
 * sess_open_jsonsession - this is what opens a session to a server and
 *  establishesthe session.
 *
 * where:
 * @param0  <server>         - I
 *          the server string <protocol:address:port>
 * @param1 <sess_type>       - I
 *          defines the session type (*client/server)
 * returns: <jsonp>         - O
 *          returns the ptr to jsonrpc structure.
 *
 */
static struct jsonrpc_session *
sess_open_jsonsession(char *server, enum_sess_comm_type sess_type)
{
    static char *mod = "sess_open_jsonrpc";
    struct jsonrpc_session *jsessp;

    ENTER(mod);

    jsessp = jsonrpc_session_open(server, false);
    if (sess_type == SESS_TYPE_CLIENT) {
        jsonrpc_session_connect(jsessp);
        jsessp->rpc = jsonrpc_open(jsessp->stream);
    }

    LEAVE(mod);
    return(jsessp);
}

/*
 * jsonrpc_session_connect
 */
static void jsonrpc_session_connect(struct jsonrpc_session *s)
{
    const char *name = reconnect_get_name(s->reconnect);
    int error;

    jsonrpc_session_disconnect(s);
    if (!reconnect_is_passive(s->reconnect)) {
        error = jsonrpc_stream_open(name, &s->stream, s->dscp);
        if (!error) {
            reconnect_connecting(s->reconnect, time_msec());
        } else {
            s->last_error = error;
        }
    } else {
        error = s->pstream ? 0 : jsonrpc_pstream_open(name, &s->pstream,
                                                      s->dscp);
        if (!error) {
            reconnect_listening(s->reconnect, time_msec());
        }
    }

    if (error) {
        reconnect_connect_failed(s->reconnect, time_msec(), error);
    }
    s->seqno++;
}

static void jsonrpc_session_disconnect(struct jsonrpc_session *s)
{
    if (s->rpc) {
        jsonrpc_error(s->rpc, EOF);
        jsonrpc_close(s->rpc);
        s->rpc = NULL;
        s->seqno++;
    } else if (s->stream) {
        stream_close(s->stream);
        s->stream = NULL;
        s->seqno++;
    }
}

static void jsonrpc_error(struct jsonrpc *rpc, int error)
{
    ovs_assert(error);
    if (!rpc->status) {
        rpc->status = error;
        jsonrpc_cleanup(rpc);
    }
}

static void sess_jsonrpc_session_close(struct jsonrpc_session *s)
{
    if (s) {
        jsonrpc_close(s->rpc);
        reconnect_destroy(s->reconnect);
        if (s->stream)
            if (s->stream->class->close)
                stream_close(s->stream);
        if (s->pstream)
            if (s->pstream->class->close)
                pstream_close(s->pstream);
        free(s);
    }
}


static void jsonrpc_cleanup(struct jsonrpc *rpc)
{
    stream_close(rpc->stream);
    rpc->stream = NULL;

    json_parser_abort(rpc->parser);
    rpc->parser = NULL;

    ofpbuf_list_delete(&rpc->output);
    rpc->backlog = 0;
}


/* ==================================================================== */

/*
 * sess_list_add - add the sessp to the session list.
 * pointer.
 *
 * where:
 * @param0 <sessp>               - I
 *         pointer to the session information.
 * @return: <retb>               - O
 *          0 all good, 1 bad.
 *
 */
static bool sess_list_add(session_p sessp, char *name)
 {
     static char *mod = "sess_list_add";
     bool retb = false;
     sess_manage_p sip = sess_info;
     struct shash_node *shnp = NULL;

     ENTER(mod);
     if (sess_is_initialized()) {
         ovs_rwlock_wrlock(&sip->sess_list_rwlock);
         shnp = shash_add(&sip->sess_list, name, sessp);
         ovs_rwlock_unlock(&sip->sess_list_rwlock);
     }
     else {
        VLOG_ERR(not_init_msg, mod);
        retb = true;
     }
     LEAVE(mod);
     return(retb);
}

/*
 * sess_list_mod_count - increments/decrements and returns the current
 * sess_list_count, this is how many active connected sessions are active.
 *
 * where:
 * @param0 <inc_flag>         - I
 *         if true increments the count, if flase decrements it.
 * @param <lock_flag>         - I
 *         if true uses locking, else not.
 * @returns <count>           - O
 */
static int sess_list_mod_count(enum_counter_direction inc_flag, enum_locking lock_flag)
{
    static char *mod = "sess_list_mod_count";
    int retc;
    sess_manage_p sip = sess_info;

    ENTER(mod);

    if (lock_flag)
        ovs_rwlock_wrlock(&sip->sess_list_rwlock);
    if (inc_flag)
        retc = ++sip->sess_list_count;
    else
        retc = --sip->sess_list_count;
    if (lock_flag)
        ovs_rwlock_unlock(&sip->sess_list_rwlock);

    VLOG_DBG("%s: retc=%d", mod, retc);
    LEAVE(mod);
    return(retc);
}

/*
 * sess_epoll_modify - this adds/deletes a session to/from the epoll list,
 * depending upon the add_flag, if true it adds, if false it deletes.
 *
 * where:
 *   @param0 <sessp>              - I
 *           pointer to created session that this epoll will be waiting for
 *           traffic on.
 *   @param1 <add_flag>           - I
 *
 * returns: <retb>                - O
 *          0 is sucessfull, else at this point you tried to epoll a server fd.
 *
 */
static bool sess_epoll_modify(session_p sessp, bool add_flag)
{
    static char *mod = "sess_epoll_add";
    bool retb = false;
    struct epoll_event event;
    struct stream_fd *s;
    int fd;
    int op, retc;
    sess_manage_p sip = sess_info;

    ENTER(mod);

    /* setup the epoll fd */
    memset(&event, 0, sizeof(struct epoll_event));
    event.events = EPOLLIN;
    event.data.ptr = sessp;
    s = stream_fd_cast(sessp->jsessp->stream);
    fd = s->fd;
    event.data.fd = fd;

    ovs_rwlock_wrlock(&sip->rwlock);
    if (add_flag) {
        op = EPOLL_CTL_ADD;
        sip->n_epoll_fds++;
    }
    else {
        op = EPOLL_CTL_DEL;
        if (--sip->n_epoll_fds < 0)
            sip->n_epoll_fds = 0;
    }
    ovs_rwlock_unlock(&sip->rwlock);

    retc = epoll_ctl(sip->epoll_fd, op, s->fd, &event);
    if (retc) {
        VLOG_ERR("%s: epoll_ctl setup, epoll_fd:%d socket:%d op:%d err:%s",
                 mod, sip->epoll_fd, s->fd, op, strerror(errno));
        retb = true;
    }

    LEAVE(mod);
    return(retb);
}

/*
 * sess_epoll_thread - this handles epoll events andsends the session to the
 * event_q for processing.
 *
 */
static void *sess_epoll_monitor_thread(void *sip_)
{
    static char *mod = "sess_epoll_thread";
    int epoll_timeout, retval, i, retc;
    sess_manage_p sip = sip_;
    eventq_ele_t *evtqp;
    struct epoll_event *events;
    session_p sessp;

    ENTER(mod);
    VLOG_INFO("%s: *********** epoll thread started ********", mod);

    /* setup the epoll fd */
    events = xzalloc(sizeof(struct epoll_event) * sip->max_sessions);
    ovs_rwlock_rdlock(&sip->rwlock);
    sip->epoll_fd = epoll_create(sip->max_sessions);
    if (sip->epoll_fd < 0) {
        VLOG_FATAL("%s: can't create the epoll instance: %s", mod,
                   strerror(errno));
        ovs_rwlock_unlock(&sip->rwlock);
        goto rtn_return;
    }

    ovs_rwlock_unlock(&sip->rwlock);
    epoll_timeout = 50000;     /* 50000 mills timeout, 50 secs */

    for (sip->epoll_monitor_run = true; sip->epoll_monitor_run; ) {
        if (sip->sess_list_count) {
            do {
                retval = epoll_wait(sip->epoll_fd, events,
                                    sip->max_sessions, epoll_timeout);
            } while (retval < 0 && errno == EINTR);
            VLOG_DBG("%s: retval:%d errno:%s", mod, retval, strerror(errno));
            if (retval < 0) {
                VLOG_WARN("%s: epoll_wait failed(%s)", mod, strerror(errno));
            } else if (retval > 0) {
                sip->epoll_n_events = retval;
                VLOG_INFO("%s:+++++++ PSUEDO FIRE UP OPFLEX DISPATCHER "
                          "retval:%d", mod, retval);
                /* TODO dkehn: this is where we put the event into the evetq
                 * that signals message is pending.
                 */

                VLOG_DBG("%s: checking the events: %d", mod, retval);
                for (i = 0; i < retval; i++) {
                    if ((events[i].events & EPOLLERR) ||
                        (events[i].events & EPOLLHUP) ||
                        (!(events[i].events & EPOLLIN))) {
                        VLOG_ERR("%s: epoll error: 0x%08x", mod,
                                 events[i].events);
                    }
                    else {
                        /* put this event on the queue */
                        sessp = sess_get_with_fd(events[i].data.fd);
                        if (sessp == NULL) {
                            VLOG_ERR("%s: epoll event fd:%d "
                                     "not found in session list", mod, 
                                     events[i].data.fd);
                            break;
                        }
                        evtqp = xzalloc(sizeof(eventq_ele_t));
                        evtqp->tv = tv_tod();
                        evtqp->cmd_size = 1;
                        evtqp->state = SESS_EVTQ_ST_DSPH;
                        evtqp->dp = sessp;
                        retc = eventq_add(sip->workq, evtqp);
                        if (retc) {
                            VLOG_ERR("%s: error putting event on the queue",
                                     mod);
                            free(evtqp);
                        }
                    } /* else */
                } /* for */
            } /* else if */
        } /* (sip->sess_list_count) */
    }
 rtn_return:
    LEAVE(mod);
    return(NULL);
}

/*
 * sess_open_jsonrpc - this is what opens a session to a server and establishes
 * the session.
 *
 * where:
 * @param0  <server>         - I
 *          the server string <protocol:address:port>
 * returns: <jsonp>         - O
 *          returns the ptr to jsonrpc structure.
 *
 */
static struct jsonrpc *sess_open_jsonrpc(const char *server)
{
    static char *mod = "sess_open_jsonrpc";
    struct stream *stream;
    struct jsonrpc *jsonp = NULL;
    int error;

    error = stream_open_block(jsonrpc_stream_open(server, &stream,
                              DSCP_DEFAULT), &stream);
    if (error == EAFNOSUPPORT) {
        struct pstream *pstream;

        error = jsonrpc_pstream_open(server, &pstream, DSCP_DEFAULT);
        if (error) {
            VLOG_ERR("%s: failed to connect or listen to \"%s\"", mod, server);
        }

        VLOG_INFO("%s: waiting for connection...", server);
        error = pstream_accept_block(pstream, &stream);
        if (error) {
            VLOG_ERR("%s: failed to accept connection on \"%s\"", mod, server);
        }

        pstream_close(pstream);
    }
    else if (error) {
        VLOG_ERR("%s: failed to connect to \"%s\"", mod, server);
    }
    else {
        jsonp = jsonrpc_open(stream);
    }

    return(jsonp);
}


/*
 * session_worker_thread - when the epoll detects something coming this
 * worker thread is dispatched with the session.
 */
static void *session_worker_thread (void *arg)
{
    static char *mod = "session_worker_thread";

    ENTER(mod);
    worker_p mine = (worker_t*)arg;
    crew_p crew = mine->crew;
    eventq_ele_t *work;
    int dWait = 1;
    session_p sessp;

    /*
     * loop here forever processing work
     */
    for (session_worker_thread_running = true; ; ) {
        /* Wait while there is nothing to do, and the hope of something
         * coming along later. If crew->first is NULL, there's no work.
         */
        if (dWait) {
            VLOG_DBG("%s:%d: Crew waiting for work\n", mod, mine->index);
            dWait = 0;
        }

        ovs_mutex_lock(&crew->mutex);

        /* If there is nothing to do wait for something......
         */
        if (crew->work_count == 0) {
            VLOG_DBG("%s:%d: waiting for work\n", mod, mine->index);
            ovs_mutex_cond_wait(&crew->go, &crew->mutex);
        }

        if (crew->quit == 1) {
            pthread_cond_signal(&crew->done);
            VLOG_DBG("%s:%d: quiting\n", mod, mine->index);
            crew->crew_size--;
            ovs_mutex_unlock(&crew->mutex);
            goto rtn_return;
        }

        VLOG_DBG("%s: crew work_count = %d\n", mod, crew->work_count);

        if (crew->work_count > 0) {
            VLOG_DBG("%s:%ld:%d: ======== Woke: work_count=%d=========\n",
                  mod, pthread_self(), mine->index, crew->work_count);

            /* Remove and process a work item
             */
            work = crew->first;
            crew->first = work->next;
            if (crew->first == NULL)
                crew->last = NULL;

            work->next = NULL;
            crew->work_count--;

            ovs_mutex_unlock (&crew->mutex);

            work->state = 2;
            sessp = (session_p)work->dp;
            VLOG_DBG("%s: Dispatching on: %s \n", mod,
                  tv_show(tv_subtract(tv_tod(), work->tv), false, NULL));
            if (opflex_dispatcher(sessp)) 
                VLOG_ERR("%s: error from opflex_dispatcher", mod);

            free(work);
        }
        else {
            ovs_mutex_unlock(&crew->mutex);
        }
    }

 rtn_return:
    session_worker_thread_running = false;
    LEAVE(mod);
    pthread_exit(NULL);
    return NULL;
}
