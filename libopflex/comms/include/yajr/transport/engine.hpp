/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _____COMMS__INCLUDE__YAJR__TRANSPORT__ENGINE_HPP
#define _____COMMS__INCLUDE__YAJR__TRANSPORT__ENGINE_HPP

namespace yajr { namespace comms { namespace transport {

class Transport {

  public:

    struct Callbacks {
        uv_alloc_cb allocCb_;
        uv_read_cb  onRead_;
    };

    class EngineData {};

    Transport(Callbacks * callbacks, EngineData * data)
        : callbacks_(callbacks), data_(data) {}

    Callbacks const * callbacks_;
    EngineData * data_;

};

template< class Engine >
class TransportEngine : public Transport {
  public:
    TransportEngine();
    ~TransportEngine();
}

template< class Engine >
TransportEngine::TransportEngine()
    :
        Transport(
            &Engine::kCallbacks
          , new (std::no_throw) Engine()
        )
    {}

template< class Engine >
TransportEngine::~TransportEngine() {
    if (data_) {
        delete static_cast< Engine * >(data_);
    }
}

}}}

#endif /* _____COMMS__INCLUDE__YAJR__TRANSPORT__ENGINE_HPP */
