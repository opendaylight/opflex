/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _INCLUDE__OPFLEX__COMMS_HPP
#define _INCLUDE__OPFLEX__COMMS_HPP

#include <uv.h>
#include <boost/function.hpp>

namespace opflex { namespace comms {

namespace internal {

class CommunicationPeer;
class ActivePeer;
class ListeningPeer;

typedef boost::function<void (CommunicationPeer *)> ConnectionHandler;

typedef union peer_db_ peer_db_t;

extern peer_db_t peers;

}

typedef uv_loop_t * (*uv_loop_selector_fn)(void);

int  initCommunicationLoop(uv_loop_t * loop);
void finiCommunicationLoop(uv_loop_t * loop);

int comms_passive_listener(
        internal::ConnectionHandler & connectionHandler,
        char const * ip_address, uint16_t port,
        uv_loop_t * listener_uv_loop = NULL,
        uv_loop_selector_fn uv_loop_selector = NULL,
        opflex::comms::internal::ListeningPeer * peer = NULL);

int comms_active_connection(
        internal::ConnectionHandler & connectionHandler,
        char const * host = NULL,
        char const * service = NULL,
        uv_loop_selector_fn uv_loop_selector = NULL,
        opflex::comms::internal::ActivePeer * peer = NULL);

}}

#endif /* _INCLUDE__OPFLEX__COMMS_HPP */
