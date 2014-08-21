/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#ifndef SESS_H
#define SESS_H 1

#include <time.h>

#include "byteq.h"
#include "stream-provider.h"
#include "stream.h"
#include "stream-ssl.h"
#include "ovs-thread.h"
#include "eventq.h"
#include "shash.h"


typedef enum _enum_counter_direction {
    CNTR_DEC = 0,
    CNTR_INC,
} enum_counter_direction;

typedef enum _enum_locking {
    WITH_LOCKING=0,
    WOUT_LOCKING,
} enum_locking;

typedef enum _enum_data_format {
    DF_JSON = 1,
    DF_XML,
} enum_data_format;

typedef enum _enum_sess_type {
    SESS_TYPE_SERVER = 0,
    SESS_TYPE_CLIENT,
} enum_sess_type;

typedef enum _enum_sess_comm_type {
    SESS_COMM_ASYNC=0,
    SESS_COMM_SYNC,
} enum_sess_comm_type;

typedef enum _enum_sess_evtq_state {
    SESS_EVTQ_ST_NEW = 0,
    SESS_EVTQ_ST_DSPH,
} enum_sess_evtq_state;

/* the data in the session will be the data defined in the sess_list->data
 *  pointer. 
 */
typedef struct _session {
    /* session rwlock
     */
    struct ovs_rwlock rwlock;

    /* Represets when this session was started, used to find the oldest session
     */
    struct timeval sess_start_ts;

    /* When set indicated that a worker thread owns it.
     */
    pthread_t sess_thread_id;

    /* this is the fd for this session
     */
    int fd;

    /* Session type is going to be used to define if this is an async
     * or sync model. If async its fd is going to be in the epoll_monitor's
     * list, if sync the fd will not be in the epoll, in othere words the caller
     * will have to do a revc to get messgaes from it.
     */
    enum_sess_comm_type comm_type;

    /* This is the point to either the client data, if it was a active
     * session established, or server session point if it was a passive
     * session setup.
     */
    struct jsonrpc_session *jsessp;

} session_t, *session_p;

typedef struct _sess_manage {
    /* this defines the serializer that this session will use.
     */
    enum_data_format df_type;

    struct ovs_rwlock rwlock;

    /* This is the default controller that was read from the configuration
     * file.
     */
    const char *default_controller;

    /* max number of session from the config file
     */
    int max_sessions;

    /* max session handling  threads
     */
    int max_sess_threads;

    /* session queuse depth, the number of elements in the queue , this
     * is something that between the max_sess_threads and the quese depth
     * is how much scaling the policy management can handle.
     */
    int sess_q_depth;

    /* this is the session handler work crew.
     */
    crew_p workq;

    /* event_q for putting session that are to handled by the worker threads.
     */
    eventq_p sess_event_q;

    /* the run flag for the session monitor thread.
     */
    bool epoll_monitor_run;

    enum_data_format stream_format;

    /* the epoll fd that is for the waiter.
     */
    int epoll_fd;

    /* this is number of fds epoll in monitoring.
     */
    int n_epoll_fds;

    /* protocol defines the type of layer 4 protocol we are going to be using
     * i.e. OPFLEX.
     */
    int protocol;

    /* number of events returned from the epoll_wait call
     */
    int epoll_n_events;

    /* this is the session open that when invoked will open a
     * connection to *name using the protocol that is represented
     * by protocol.
     */
    int (*open)(char *name);

    /* for whenever its necessary to terminate a session this call will be
     * used with the session struct
     */
    int (*close)(void);

    /* this will start the server thread for listening for un-solicited
     * connections.
     */
    int (*start_server)(void);

    int(*stop_server)(void);

    int (*get_active_sessions)(void);

    /* this should represent the number of active (open) sessions in
     * the sess_cache.
     */
    int active_session;

    /* each session either active or in-active will be defined in in the
     * sess_list.
     */
    struct ovs_rwlock sess_list_rwlock;

    /* this represent the active connected session.
     */
    int sess_list_count;

    /* This is the session list of active connections, if when a connection
     *  is no longer valid it is removed.
     */
    struct shash sess_list;

} sess_manage_t, *sess_manage_p;

/*
 * Global protos
 */
extern bool sess_initialize(void);
extern void sess_cleanup(void);
extern session_p sess_open(char *name, enum_sess_type sess_type,
                      enum_sess_comm_type ctype);
extern session_p sess_get(char *name);
extern bool sess_close(char *name);
extern session_p sess_list_search(char *name);
extern bool sess_is_initialized(void);
extern int sess_get_session_count(void);
extern bool sess_is_alive(char *name);
extern bool sess_is_connected(char *name);
extern void *sess_recv(char *name, long timeout_secs, bool cvt_to_string);

#endif
