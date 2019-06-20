/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _INCLUDE__YAJR__YAJR_HPP
#define _INCLUDE__YAJR__YAJR_HPP

#include <yajr/transport/engine.hpp>

#include <uv.h>

#include <string>

namespace yajr {

int  initLoop(uv_loop_t * loop);
void finiLoop(uv_loop_t * loop);

namespace StateChange {
    enum To {
        CONNECT,
        DISCONNECT,
        FAILURE,
        TRANSPORT_FAILURE,
        DELETE,
    };
}

/**
 * @brief A yajr communication peer.
 */
struct Peer {

  public:

    /**
     * @brief Typedef for a Peer state change callback
     *
     * Callback type for a Peer State Change. The callback is invoked when a
     * Peer becomes CONNECT'ed, or DISCONNECT'ed or when a FAILURE occurs for the
     * Peer. These three states are represented by the \p stateChange parameter.
     *
     * When \p stateChange is FAILURE, \p error carries a non-zero value, which
     * is to be interpreted as a libuv error code.
     *
     * When \p stateChange is TRANSPORT_FAILURE, \p error carries a non-zero
     * value, which is to be interpreted as a transport library error code. For
     * example, the error code would be an OpenSSL error code in case the
     * ZeroCopyOpenSSL transport was attached to the peer. The PlainText
     * transport never emits any TRANSPORT_FAILURE \p stateChange. If you never
     * explicitly attached any transport to the peer, then by default the
     * PlainText transport is the one attached to it, and you will never see any
     * TRANSPORT_FAILURE.
     *
     * When \p stateChange is DELETE, the Peer is about to be deleted. It's a
     * good time to release any resources such as the memory reachable via the
     * \p data pointer.
     */
    typedef void (*StateChangeCb)(
            yajr::Peer            *,
                                    /**< [in] the Peer the callback refers to */
            void                  * data,
                                         /**< [in] Callback data for the Peer */
            yajr::StateChange::To   stateChange,
                                                 /**< [in] New state for Peer */
            int                     error
                                               /**< [in] libuv error for Peer */
    );

    /**
     * @brief Typedef for a uv_loop selector
     *
     * Function pointer type for the uv_loop selector method to be used to
     * select which particular uv_loop to assign a peer to.
     *
     * @return a pointer to the desired uv_loop.
     */
    typedef uv_loop_t * (*UvLoopSelector)(
            void * data
                                         /**< [in] Callback data for the Peer */
    );

    /**
     * @brief Factory for an active yajr TCP communication Peer.
     *
     * This static class method creates a communication Peer and connects it
     * to the desired \p host and \p service at the very next opportunity.
     *
     * Upon successful connect, the \p connectionHandler callback gets invoked
     * with the Peer pointer, the \p data pointer supplied to Peer::create(),
     * StateChange::To::CONNECT, and an error value of 0 (success).
     *
     * Upon an error, the \p connectionHandler callback gets invoked
     * with the Peer pointer, the \p data pointer supplied to Peer::create(),
     * StateChange::To::FAILURE, and a non-zero error value. If the Peer had
     * managed to connect before the failure, the callback will get invoked once
     * again shortly after, with a state of DISCONNECT.
     *
     * Upon disconnect, the \p connectionHandler callback gets invoked
     * with the Peer pointer, the \p data pointer supplied to Peer::create(),
     * StateChange::To::DISCONNECT, and possibly a non-zero error value. If the
     * disconnect is due to a failure, the callback will have just been invoked
     * with a FAILURE state immediately before this time.
     *
     * If \p data was allocated by the caller, \p connectionHandler should be
     * releasing \p data upon StateChange::To::DELETE.
     *
     * @return a pointer to the Peer created
     **/
    static Peer * create(
            std::string const     & host,
                                         /**< [in] the hostname to connect to */
            std::string const     & service,
                                               /**< [in] service name or port */
            StateChangeCb           connectionHandler,
                                              /**< [in] state change callback */
            void                  * data              = NULL,
                                                      /**< [in] callback data */
            UvLoopSelector          uvLoopSelector    = NULL,
                                     /**< [in] uv_loop selector for this Peer */
            bool nullTermination = true
                 /**< [in] add null byte to end of every rpc message sent out */
    );

    /**
     * @brief Factory for an active yajr Unix Domain Socket communication Peer.
     *
     * @return a pointer to the Peer created
     * @see create
     **/
    static Peer * create(
            std::string const     & socketName,
                                /**< [in] UNIX Socket/pipe name to connect to */
            StateChangeCb           connectionHandler,
                                              /**< [in] state change callback */
            void                  * data              = NULL,
                                                      /**< [in] callback data */
            UvLoopSelector          uvLoopSelector    = NULL
                                     /**< [in] uv_loop selector for this Peer */
    );

    /**
     * @brief disconnect the peer
     *
     * Disconnect the peer. Active peers will re-attempt to connect, passive
     * peers will be destroyed asynchronously.
     *
     * For passive peers, disconnect() and destroy() have the same effect.
     */
    virtual void disconnect(
            bool now = false          /**< [in] do not wait for a clean close */
        ) = 0;

    /**
     * @brief perform an asynchronous delete
     *
     * Perform an asynchronous delete of this communication peer, disconnecting
     * it if still connected and destroying it as soon as no more callbacks are
     * pending for this Peer.
     *
     * For passive peers, disconnect() and destroy() have the same effect.
     */
    virtual void destroy(
            bool now = false          /**< [in] do not wait for a clean close */
        ) = 0;

    /**
     * @brief retrieves the pointer to the opaque data associated with this peer
     *
     * @return a pointer to the opaque data for this peer, the same pointer that
     * is provided to the State Change callback
     */
    virtual void * getData() const = 0;

    /**
     * @brief retrieves the address of the remote peer
     *
     * Retrieves the address of the remote peer the underlying socket is
     * connected to, in the buffer pointed to by \p remoteAddress. The integer
     * pointed to by the \p len argument should be initialized to indicate the
     * amount of space pointed to by \p remoteAddress. On return it contains the
     * actual size of the structure returned (in bytes). The structure is
     * truncated if the buffer provided is too small. In this case, \p *len will
     * return a value greater than was supplied to the call.
     *
     * The peer must be connected for this method to work.
     *
     * @return An error code is returned on failure, or 0 on success.
     */
    virtual int getPeerName(struct sockaddr* remoteAddress, int* len) const = 0;

    /**
     * @brief retrieves the address of the local peer
     *
     * Retrieves the address of the local peer for the underlying socket, in the
     * buffer pointed to by \p localAddress. The integer
     * pointed to by the \p len argument should be initialized to indicate the
     * amount of space pointed to by \p localAddress. On return it contains the
     * actual size of the structure returned (in bytes). The structure is
     * truncated if the buffer provided is too small. In this case, \p *len will
     * return a value greater than was supplied to the call.
     *
     * The peer must be connected for this method to work.
     *
     * @return An error code is returned on failure, or 0 on success.
     */
    virtual int getSockName(struct sockaddr* localAddress, int* len) const = 0;

    /**
     * @brief start performing periodic keep-alive
     *
     * start performing periodic keep-alive exchanges via the json-rpc
     * "echo" method. Abort the connection after 2 times interval worth of
     * silence from the other end.
     */
    virtual void startKeepAlive(
            uint64_t                begin             = 100,
            uint64_t                repeat            = 1250,
            uint64_t                interval          = 9000
    ) = 0;

    /**
     * @brief stop performing periodic keep-alive
     */
    virtual void stopKeepAlive() = 0;

  protected:
    Peer() {}
    ~Peer() {}
};

struct Listener {

  public:

    /**
     * @brief Typedef for an Accept callback
     *
     * Callback type for an Accept callback. The callback is invoked when a
     * Peer is ACCEPT'ed.
     *
     * @return void pointer to callback data for the StateChange callback for
     * the accepted passive peer, or NULL if error != 0.
     */
    typedef void * (*AcceptCb)(
            Listener               *,
                                /**< [in] the Listener the callback refers to */
            void                   * data,
                                     /**< [in] Callback data for the Listener */
            int                      error
                                               /**< [in] libuv error for Peer */
    );

    /**
     * @brief Factory for a passive yajr Listener.
     *
     * This static class method creates a yajr Listener and binds it to
     * the desired \p ip and \p port at the very next opportunity. Upon
     * accept'ing a client, a passive peer gets created with \p connectionHandler
     * as a state change callback, and the callback data returned by the
     * \p acceptHandler callback.
     *
     * The \p acceptHandler callback gets invoked upon accept'ing a client. It
     * get invoked with the following parameters: the Listener pointer, the
     * \p data pointer supplied to Listener::create(), and an \p error value.
     * The \p error value will be 0 in case of a successful accept, or a libuv
     * error value otherwise. If \p error is 0, \p acceptHandler is expected to
     * return a void pointer to the callback \p data to be associated with the
     * passive peer that is being accepted, and to be passed by that peer to its
     * \p connectionHandler. If \p error is non-zero, \p acceptHandler is
     * expected to return NULL, since no passive peer is being created. If the
     * \p acceptHandler callback is allocating memory, the \p connectionHandler
     * callback should be releasing that memory when invoked with a state of
     * DISCONNECTED.
     *
     * Upon an error, the \p connectionHandler callback gets invoked
     * with the Peer pointer, the \p data pointer supplied to Peer::create(),
     * StateChange::To::FAILURE, and a non-zero error value. If the Peer had
     * managed to connect before the failure, the callback will get invoked once
     * again shortly after, with a state of DISCONNECT.
     *
     * Upon disconnect, the \p connectionHandler callback gets invoked
     * with the Peer pointer, the \p data pointer supplied to Peer::create(),
     * StateChange::To::DICONNECT, and possibly a non-zero error value. If the
     * disconnect is due to a failure, the callback will have just been invoked
     * with a FAILURE state immediately before this time.
     *
     * @return a pointer to the Peer created
     **/
    static Listener * create(
        const std::string&           ip_address,
                /**< [in] the ip address to bind to, or "0.0.0.0" to bind all */
        uint16_t                     port,
                                                /**< [in] the port to bind to */
        Peer::StateChangeCb          connectionHandler,
               /**< [in] state change callback for the accepted passive peers */
        AcceptCb                     acceptHandler     = NULL,
                                  /**< [in] accept callback for this listener */
        void                       * data              = NULL,
                              /**< [in] callback data for the accept callback */
        uv_loop_t                  * listenerUvLoop    = NULL,
                                       /**< [in] libuv loop for this Listener */
        Peer::UvLoopSelector         uvLoopSelector    = NULL
                 /**< [in] libuv loop selector for the accepted passive peers */
    );

    /**
     * @brief Factory for passive yajr Unix Domain Socket communication Listener.
     *
     * @return a pointer to the Peer created
     * @see create
     **/
    static Listener * create(
        std::string const     & socketName,
                                   /**< [in] UNIX Socket/pipe name to bind to */
        Peer::StateChangeCb          connectionHandler,
               /**< [in] state change callback for the accepted passive peers */
        AcceptCb                     acceptHandler     = NULL,
                                  /**< [in] accept callback for this listener */
        void                       * data              = NULL,
                              /**< [in] callback data for the accept callback */
        uv_loop_t                  * listenerUvLoop    = NULL,
                                       /**< [in] libuv loop for this Listener */
        Peer::UvLoopSelector         uvLoopSelector    = NULL
                 /**< [in] libuv loop selector for the accepted passive peers */
    );

    /**
     * @brief perform an asynchronous delete
     *
     * Perform an asynchronous delete of this Listener, destroying it as soon as
     * no more callbacks are pending for it.
     */
    virtual void destroy(
            bool now = false          /**< [in] do not wait for a clean close */
        ) = 0;

  protected:
    Listener() {}
    ~Listener() {}
};

} /* yajr namespace */

#endif /* _INCLUDE__YAJR__YAJR_HPP */

