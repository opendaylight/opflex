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

namespace opflex { namespace comms {

int  initLoop(uv_loop_t * loop);
void finiLoop(uv_loop_t * loop);

namespace StateChange {
enum {
    CONNECT,
    DISCONNECT,
    FAILURE,
};
}

class Peer;
typedef void (*state_change_cb)(Peer *, void * data, int stateChange, int error);
typedef uv_loop_t * (*uv_loop_selector_fn)(void);

class Peer {

  public:

    static Peer * create(
        char const * host,
        char const * service,
        state_change_cb connectionHandler,
        void * data = NULL,
        uv_loop_selector_fn uv_loop_selector = NULL
    );

    virtual void destroy() = 0;

    virtual void startKeepAlive(
            uint64_t interval = 2500,
            uint64_t begin = 100,
            uint64_t repeat = 1250) = 0;

    virtual void stopKeepAlive() = 0;

  protected:
    Peer() {}
    ~Peer() {}
};

class Listener;
typedef void * (*accept_cb)(Listener *, void * data, int error);

class Listener {
  public:

    static Listener * create(
        char const * ip_address,
        uint16_t port,
        state_change_cb connectionHandler,
        accept_cb acceptHandler = NULL,
        void * data = NULL,
        uv_loop_t * listener_uv_loop = NULL,
        uv_loop_selector_fn uv_loop_selector = NULL
    );

    virtual void destroy() = 0;

  protected:
    Listener() {}
    ~Listener() {}
};

}}

#endif /* _INCLUDE__OPFLEX__COMMS_HPP */
