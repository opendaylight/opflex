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

#include <uv.h>

namespace yajr {

int  initLoop(uv_loop_t * loop);
void finiLoop(uv_loop_t * loop);

namespace StateChange {
    enum To {
        CONNECT,
        DISCONNECT,
        FAILURE,
    };
};

struct Peer {

  public:

    typedef void (*StateChangeCb)(
            yajr::Peer            *,
            void                  * data,
            yajr::StateChange::To   stateChange,
            int                     error
    );

    typedef uv_loop_t * (*UvLoopSelector)(void);

    static Peer * create(
            char const            * host,
            char const            * service,
            StateChangeCb           connectionHandler,
            void                  * data              = NULL,
            UvLoopSelector          uvLoopSelector    = NULL
    );

    virtual void destroy() = 0;

    virtual void startKeepAlive(
            uint64_t                begin             = 100,
            uint64_t                repeat            = 1250,
            uint64_t                interval          = 2500
    ) = 0;

    virtual void stopKeepAlive() = 0;

  protected:
    Peer() {}
    ~Peer() {}
};

struct Listener {

  public:

    typedef void * (*AcceptCb)(
            Listener               *,
            void                   * data,
            int                      error
    );

    static Listener * create(
        char const                 * ip_address,
        uint16_t                     port,
        Peer::StateChangeCb          connectionHandler,
        AcceptCb                     acceptHandler     = NULL,
        void                       * data              = NULL,
        uv_loop_t                  * listenerUvLoop    = NULL,
        Peer::UvLoopSelector         uvLoopSelector    = NULL
    );

    virtual void destroy() = 0;

  protected:
    Listener() {}
    ~Listener() {}
};

}

#endif /* _INCLUDE__YAJR__YAJR_HPP */
