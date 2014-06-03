/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPF_API_H
#define OPF_API_H

/* 
 * Managed Object (MO) URI. Indicates the position of
 * a MO subtree within the larger Managment Information Tree (MIT)
 * Structure is TBD, but possibilities include hash, string with
 * heirarchical namespace, integer offsets, etc.
 */
struct opf_mo_uri;

/*
 * Managed Object
 */
struct opf_mo {
    int size;
    struct opf_mo_uri location;
    void *subtree;
};

struct opf_dod {
    void *dod;
    int len;
};

/* 
 * enum for describing serialization type to be used.
 * Can be applied towards MO or packet serialiation/deserialization
 */
enum opf_ser_type {
    OPF_SERTYPE_BIN,
    OPF_SERTYPE_JSON,
    OPF_SERTYPE_XML
};

/*
 * Data structure for abstracting the implementation used for
 * communicating with an OpFlex peer. The structure of this is
 * TBD for now, but this could be something like the rconn
 * abstraction in OVS
 */
struct opf_conn;

/* Transparent type for identifying OpFlex peer. */
struct opf_peer {
    char *name;
    char *domain;
    char *role;
    enum opf_ser_type ser_type;
    struct opf_sercurity;
};

/*
 * Encapsulates an OpFlex session. Not clear whether the API
 * should expose the actual data structure, or just keep it
 * opaque, with getters and setters. The Connection Manager
 * portion of the library is responsible for creating these
 * objects, and providing notifications when they are no
 * longer valid.
 */
struct opf_session {
    struct opf_conn conn;
    struct opf_peer *peer;
};

/* purposefully opaque */
struct opf_session_itr;

/* events returned from session APIs */
enum {
    OPF_EVENT_NEW_SESSION,            /* a new session was created */
    OPF_EVENT_ASYNC_MESSAGE,          /* OpFlex message from peer */
    OPF_EVENT_MESSAGE_SEND_FAILED,    /* Failed to send OpFlex message */
    OPF_EVENT_RESPONSE_MESSAGE,       /* reply to message we sent */
    OPF_EVENT_CONNECTION_LOST,        /* connection to OpFlex peer lost */
    OPF_EVENT_SESSION_LOST,           /* session with OpFlex peer is lost */
};

/* type of sessions, defined by peer */
enum {
    OPF_SESSION_TYPE_PR        = (1<<0),    /* Policy Repository/Authority */
    OPF_SESSION_TYPE_EPR       = (1<<1),    /* End Point Registry */
    OPF_SESSION_TYPE_OBSERVER  = (1<<2),    /* Observer */
};

/* events returned from the managed object APIs */
enum opf_mo_event {
    OPF_EVENT_MO_CHANGED,
    OPF_EVENT_MO_DELETED
};

enum opf_ep_status {
    OPF_EP_STATUS_ATTACH,
    OPF_EP_STATUS_DETACH,
    OPF_EP_STATUS_MODIFY
};

struct opf_id_list {
    const char *identifiers[];
    int identifier_len;
};

/*
 * TBD: what to do about all the strings -- where
 * does that memory exist (ownership)?
 */
struct opf_msg_resolvers {
    int (*policy_resolution_rsp)(struct opf_mo *subtree, 
                                 int ttl, int cookie, int id );
    int (*policy_update_req)(const char *context, 
                             struct opf_mo *subtree, 
                             int ttl, int cookie, int id );
    int (*policy_trigger_req)(const char *policy_type,
                              const char *context,
                              const char *policy_name,
                              int ttl, int cookie, int id );
    int (*policy_trigger_rsp)(int cookie, int id);
    int (*ep_declaration_rsp)(int cookie, int id);
    int (*ep_request_rsp)(const char *subject,
                          const char *context,
                          const char *policy_name,
                          const char *identifier,
                          const char *data,
                          enum opf_ep_status status,
                          int ttl, int cookie, int id);
    int (*ep_policy_update_req)(const char *subject,
                                const char *context,
                                const char *policy_name,
                                const char *identifier,
                                const char *data,
                                enum opf_ep_status status,
                                int ttl, int cookie, int id);
    int (*state_report_rsp)(int cookie, int id);
};

/**
 *
 *  The OpFlex Initialization API
 *
 *  TODO: add PR configuration info?
 *
 *  PARAMETERS:
 *  -----------
 *  name [in]:
 *      The name of this agent
 *  admin_domain [in]:
 *      The administrative domain for the agent
 *  identifier [in]:
 *      The identifier to use for this agent
 *  resolvers [in]:
 *      Resolvers for the respective message.
 */
int 
opf_init ( const char *name, 
           const char *admin_domain,
           const struct opf_id_list *identifier,
           struct ofp_msg_resolvers *resolvers );

/****************
 *
 * Session APIs
 *
 * When the opf_init() call is made, the agent automatically 
 * attempts to establish communication using the initial 
 * communication parameters provided. This communication
 * will likely result in the client establishing additional 
 * sessions, using the information provided in the response.
 * The Session Manager keeps track of sessions and autonomously
 * performs the operations needed to manage the sessions (e.g.
 * send/receive OpFlex Hello messages to check channel liveness).
 *
 * Threading:
 * The Session APIs are synchronous.
 *
 * Memory Management:
 * The Session APIs return iterators, which are allocated by the
 * API. A separate API must be called to free the iterator. The
 * session objects returned by iterator calls are snapshot copies 
 * of the original session object. They must not be freed by the
 * caller, and are freed either by calling the API to free the
 * iterator, or when the iterator proceeds beyond that session.
 *****************/

/*
 * Get an iterator for all the sessions known to this node. The session
 * Manager makes a copy of the session list when this call is made, and
 * both the copy of the list and the iterator exist until the iterator
 * is freed (using opf_session_iterator_put call) or until the 
 * opf_session_iterator_next() returns NULL.  This ensures that
 * the iterator is always working with a unmodified verions of the list.
 * It's possible that sessions returned by the iterator may be invalidated
 * before they can be used. This can be determined by error return codes 
 * in APIs where the invalid session handle is used.
 *
 * PARAMETERS
 * ----------
 * iterator [out]:
 *     Returns an iterator that can be used to traverse the list of
 *     active sessions. If no sessions are present, returns NULL.
 * type_filter [in]:
 *     Bitmask used to limit iterator to sessions of a given type.
 *     If 0, all sessions are returned.
 *
 * RETURNS
 * -------
 * >=0 for number of active sessions
 * < 0 (errno)
 */
int
opf_get_session_iterator ( struct opf_session_itr *iterator,
                           int type_filter );

/*
 * Get the next OpFlex session
 *
 * PARAMETERS
 * ----------
 * iterator [in/out]
 *     Iterator to use for iterating sessions
 *
 * RETURNS
 * -------
 * pointer to next session in the list
 * 0 if no more sessions in the list          
 *
 */
struct opf_session *
opf_session_iterator_next ( struct opf_session_itr *iterator );

/*
 * Release the resources associated with the session iterator
 *
 * PARAMETERS
 * ----------
 * iterator [in]
 *     Iterator (and associated list) to free
 *
 * RETURNS
 * -------
 * None
 *
 */
void
opf_session_iterator_put ( struct opf_session_iter iterator );

/*****************
 * Message APIs
 *
 * The Message APIs are used for exchanging OpFlex messages
 * with other participants in the OpFlex fabric. 
 *
 * Threading:
 * The APIs to send messages are all asynchronous. In order to support
 * responses to the asynchronous send calls, and to handle reception of
 * asynchronous requests from other OpFlex participants, a blocking
 * call with an optional timeout is provided. The blocking call waits
 * for either a message or event, scoped by a session, with an optional
 * timeout. Failure to send a given message is also provided via this
 * blocking call.
 *
 * Message reception involves waiting for a message notification,
 * and once the notification is received, invoking the resolver.
 * When a message notification is received, the message has already
 * been deserialized and parsed. However, instead of invoking callbacks
 * in a different context, the user invokes the generic resolver API,
 * which calls the pre-registered resolver appropriate for that message
 * type. This allows the resolver to execute in the context of the
 * notify (or other) thread, passing it the appropriate arguments.
 *
 * Memory Management:
 * Since the APIs to send messages are asynchronous, the APIs
 * make shallow copies of all the parameters passed by the
 * caller. This allows the caller to release memory after 
 * calling the API. The shallow copies are freed once the
 * message has been serialized and sent.
 *
 * The resolvers perform allocation of any memory needed to
 * pass information to the resolver function. Any memory by the
 * notification API must be freed by the user.
 *
 *****************/

/*
 *
 * Wait for a message or an event associated with a given
 * OpFlex session. Messages can be responses to requests
 * sent by this agent, or they can by asynchronous requests
 * sent by other OpFlex participants. If a message is 
 * received, the opf_notify_msg_cb() can be called to 
 * get the message associated with the notification by
 * passing the cookie obtained from this noitfy
 *
 * PARAMETERS
 * ----------
 * session [in]:
 *     session to wait for event on. If NULL, then the caller
 *     will only receive notifications that aren't associated
 *     with any session (e.g. OPF_EVENT_NEW_SESSION).
 * event [in/out]:
 *     provides event to the caller. If the value is non-zero,
 *     the call only returns for the specified events.
 * cookie [out]:
 *     For message events, provides the cookie sent in the message.
 *     In response messages, this cookie can be used to look up the
 *     request. In request messages, this cookie must be sent in
 *     the response packet. returns NULL for non-message events
 * timeout_ms [in]:
 *     The maximum time to wait, in milliseconds. If 0, will wait forever.
 *
 * RETURNS
 * -------
 * >= 0 to indicate an event occured
 * < 0 (errno)
 */
int
opf_notify_wait ( struct opf_session *session, 
                  int *event, 
                  int *cookie, 
                  int timeout_ms );

/*
 * This call invokes the callback handler for the received message.
 * The callback is invoked in the context of the caller, not delayed.
 * For request/response messages, the cookie allows the response to
 * be associated with a request.
 */
int
opf_do_msg_cb ( struct opf_session *session, int cookie );

/*
 * Send a request to the Policy Repository to resolve the named policy
 * This is a non-blocking call. If an error occurs when sending the
 * request, the error is returned via opf_notify_wait().
 *
 * session [in]:
 *     session to use for sending the request
 * subject [in]: 
 *     the class of entity for which the policy is being resolved
 * context [in]: 
 *     scopes the policy resolution request (e.g. tennant name)
 * policy [in]: 
 *     a named policy to be resolved
 * dod [in]: 
 *     device opaque data
 * on_behalf_of [in]: 
 *     URI for MO that triggered resolution request
 * cookie [in]:
 *     The cookie is used to associate responses to requests. The
 *     cookie is opaque to the API, but is sent as a field in the
 *     request message. The response to this message contains the
 *     same cookie value.
 * id [out]: 
 *     session-unique ID assigned to this request
 *
 * returns: 0 if success 
 *          errno if send of request fails
 *
 */
int
opf_resolve_policy_req ( struct opf_session *session,
                         const char *subject, 
                         const char *context, 
                         const char *policy, 
                         const struct opf_mo_uri *on_behalf_of, 
                         struct opf_dod *dod,
                         int cookie, int *id );

/*
 * Send an echo request to a peer OpFlex participant
 * This is a non-blocking call. If an error occurs when sending the
 * request, the error is returned via opf_notify_wait().
 *
 * session [in]:
 *     session to send the echo request on
 * params [in]:
 *     params to embed in the echo request
 * cookie [in]:
 *     The cookie is used to associate responses to requests. The
 *     cookie is opaque to the API, but is sent as a field in the
 *     request message. The response to this message contains the
 *     same cookie value.
 * id [out]: 
 *     session-unique ID assigned to this request
 *
 * returns: 0 if success 
 *          errno if send of request fails
 */
int
opf_send_echo_req ( struct opf_session *session, 
                    const char *params,
                    int cookie, int *id );

/*
 * Send a policy trigger to an OpFlex peer
 * This is a non-blocking call. If an error occurs when sending the
 * request, the error is returned via opf_notify_wait().
 *
 * session [in]:
 *     session to send the echo request on
 * policy_type [in]:
 *     ???
 * context [in]
 *     scopes the policy resolution request (e.g. tennant name)
 * policy [in]: 
 *     a named policy to be resolved
 * ttl [in]:
 *     time to live of the policy being triggered
 * cookie [in]:
 *     The cookie is used to associate responses to requests. The
 *     cookie is opaque to the API, but is sent as a field in the
 *     request message. The response to this message contains the
 *     same cookie value.
 * id [out]: 
 *     session-unique ID assigned to this request
 *
 * returns: 0 if success 
 *          errno if send of request fails
 */
int
opf_policy_trigger_req ( struct opf_session *session,
                         const char *policy_type,
                         const char *context,
                         const char *policy
                         int ttl, int cookie, int *id );

/*
 * Send an Endpoint Declaration message to the End Point Registry
 * This is a non-blocking call. If an error occurs when sending the
 * request, the error is returned via opf_notify_wait().
 *
 * session [in]:
 *     session to use for sending the request
 * subject [in]: 
 *     the class of entity for which the policy is being resolved
 * context [in]: 
 *     scopes the policy resolution request (e.g. tennant name)
 * policy [in]: 
 *     a named policy to be resolved
 * dod [in]: 
 *     device opaque data
 * identifier [in]: 
 *     End Point Identifier -- can be a list
 * status [in]:
 *     one of endpoint attachment, detachment, or modification
 * ttl [in]:
 *     time to live for endpoint declaration
 * cookie [in]:
 *     The cookie is used to associate responses to requests. The
 *     cookie is opaque to the API, but is sent as a field in the
 *     request message. The response to this message contains the
 *     same cookie value.
 * id [out]: 
 *     session-unique ID assigned to this request
 *
 * returns: 0 if success 
 *          errno if send of request fails
 *
 */
int
opf_ep_declaration_req ( struct opf_session *session,
                         const char *subject,
                         const char *context,
                         const char *policy,
                         struct opf_id_list *list,
                         struct opf_dod *dod,
                         enum opf_ep_status status,
                         int ttl, int cookie, int *id );
/*
 * Send an Endpoint Request message to the End Point Registry
 * This is a non-blocking call. If an error occurs when sending the
 * request, the error is returned via opf_notify_wait().
 *
 * session [in]:
 *     session to use for sending the request
 * subject [in]: 
 *     the class of entity for which the policy is being resolved
 * context [in]: 
 *     scopes the policy resolution request (e.g. tennant name)
 * dod [in]: 
 *     device opaque data
 * identifier [in]: 
 *     End Point Identifier -- can be a list
 * status [in]:
 *     one of endpoint attachment, detachment, or modification
 * cookie [in]:
 *     The cookie is used to associate responses to requests. The
 *     cookie is opaque to the API, but is sent as a field in the
 *     request message. The response to this message contains the
 *     same cookie value.
 * id [out]: 
 *     session-unique ID assigned to this request
 *
 * returns: 0 if success 
 *          errno if send of request fails
 *
 */
int
opf_ep_request_req ( struct opf_session *session,
                     const char *subject,
                     const char *context,
                     struct opf_id_list *list,
                     struct opf_dod *dod,
                     enum opf_ep_status status,
                     int cookie, int *id );
            

/*****************
 *
 * Managed Object (MO) APIs
 *
 * This set of APIs for using the store of managed objects.
 *
 * Threading model:
 * The managed object APIs are all synhronous.
 *
 * Memory Management:
 * The managed objects (MOs) are never created nor destroyed via these APIs. These
 * APIs only deal with the MO's membership in the greater Management Information
 * Tree (MIT). MOs are created as the result of a policy update, either requested
 * or provided asynchronously. An MO can be freed once it has been removed from
 * the MIT and is no longer needed. Updates
 *****************/

/*
 * Install a managed object subtree in the MIT
 *
 * PARAMETERS:
 * -----------
 * mo [in]:
 *     The managed object to install.
 * mo_uri [in]:
 *     The parent node to install the MO under.
 */
int
opf_mo_add( struct opf_mo *mo, struct opf_mo_uri *uri );

/*
 * Delete a managed object subtree from the MIT
 *
 * PARAMETERS:
 * -----------
 * mo_uri [in]:
 *     The URI for the subtree to remove
 * mo [out]:
 *     The MO subtree that was removed from the MIT
 */
int
opf_mo_del( struct opf_mo_uri *subtree, struct opf_mo *mo );

/*
 * Update a managed object subtree in the MIT
 *
 * PARAMETERS:
 * -----------
 * mo [in/out]:
 *     The user provides a new MO to replace the existing MO
 *     in the tree. The previous MO is returned as the output.
 * mo_uri [in]:
 *     The parent node to install the MO under.
 */
int
opf_mo_update( struct opf_mo *mo, struct opf_mo_uri *uri );


/*
 * Wait for an MO event.
 *
 *
 * PARAMETERS:
 * -----------
 * mo_uri [in]:
 *     The parent node of the subtree to wait on. If NULL,
 *     it will block on an event from any MO
 * event [in/out]:
 *     provides event to the caller. If the value is non-zero,
 *     the call only returns for the specified events.
 * timeout_ms [in]:
 *     The maximum time to wait, in milliseconds. If 0, will wait forever.
 */
int
opf_mo_event ( struct opf_mo_uri *uri, 
               enum opf_mo_event *event,
               int timeout_ms );
#endif /* OPF_API_H */
