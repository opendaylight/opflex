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

#include <uv.h>

#include <boost/type_traits/is_base_of.hpp>
#include <boost/static_assert.hpp>

#include <vector>
#include <new>

namespace yajr {

    namespace comms {
        namespace internal {
            class CommunicationPeer;
        }
    }

namespace transport {

/**
 * Transport
 */
class Transport {

  public:

    struct Engine {};

    /**
     * Callbacks
     */
    struct Callbacks {
        /** alloc cb */
        uv_alloc_cb allocCb_;
        /** read cb */
        uv_read_cb  onRead_;
        /** send cb */
        int (*sendCb_)(comms::internal::CommunicationPeer const *);
        /** on sent cb */
        void (*onSent_)(comms::internal::CommunicationPeer const *);
        /** delete cb */
        void (*deleteCb_)(Transport::Engine *);
    };

    /**
     * Construct a transport instance
     * @param callbacks callbacks
     * @param data engine data
     */
    Transport(Callbacks * callbacks, Engine * data)
        : callbacks_(callbacks), data_(data) {}

    ~Transport() {
        callbacks_->deleteCb_(data_);
    }

    /**
     * Get the transport engine
     * @tparam E type of engine
     * @return transport engine
     */
    template< class E >
    E * getEngine() const {

        BOOST_STATIC_ASSERT_MSG(
                (boost::is_base_of<Transport::Engine, E>::value),
                "engine type must be a descendant of Transport::Engine"
                );

        return static_cast< E * >(data_);
    }

    /** List of libuv callbacks */
    Callbacks const * callbacks_;

  private:
    /** transport engine data */
    Transport::Engine * data_;
};

/**
 * Callbacks
 * @tparam E type
 */
template< class E>
struct Cb {
    /**
     * libuv alloc cb
     * @param _ libuv handle
     * @param size size
     * @param buf buffer
     */
    static void alloc_cb(uv_handle_t * _, size_t size, uv_buf_t* buf);
    /**
     * libuv on read cb
     * @param h libuv stream
     * @param nread nread
     * @param buf buffer
     */
    static void on_read(uv_stream_t * h, ssize_t nread, uv_buf_t const * buf);
    /**
     * libuv send cb
     * @return rc
     */
    static int send_cb(comms::internal::CommunicationPeer const *);
    /**
     * on_sent cb
     */
    static void on_sent(comms::internal::CommunicationPeer const *);
    /**
     * Called on delete
     */
    static void __on_delete(Transport::Engine *);

    /** callbacks */
    static Transport::Callbacks kCb;

    struct StaticHelpers;
};

template< class E>
Transport::Callbacks Cb< E >::kCb = {
    &Cb< E >::alloc_cb,
    &Cb< E >::on_read,
    &Cb< E >::send_cb,
    &Cb< E >::on_sent,
    &Cb< E >::__on_delete,
};

template< class E >
void Cb< E >::__on_delete(Transport::Engine * data) {

    BOOST_STATIC_ASSERT_MSG(
            (boost::is_base_of<Transport::Engine, E>::value),
            "engine type must be a descendant of Transport::Engine"
            );

    if (!data) {
        return;
    }
    delete static_cast< E * >(data);
}

/**
 * Transport Engine
 * @tparam E Engine type
 */
template< class E >
class TransportEngine : public Transport {
  public:
    /**
     * Construct a transport engine
     */
    TransportEngine(E *);
    ~TransportEngine();
};

template< class E >
TransportEngine< E >::TransportEngine(E * e)
    :
        Transport(
            &Cb< E >::kCb
          , e
        )
    {
        BOOST_STATIC_ASSERT_MSG(
                (boost::is_base_of<Transport::Engine, E>::value),
                "engine type must be a descendant of Transport::Engine"
                );
    }

template< class E >
TransportEngine< E >::~TransportEngine() {
}

} /* yajr::transport namespace */
} /* yajr namespace */

#endif /* _____COMMS__INCLUDE__YAJR__TRANSPORT__ENGINE_HPP */

