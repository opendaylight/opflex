/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _____COMMS__INCLUDE__YAJR__TRANSPORT__PLAINTEXT_HPP
#define _____COMMS__INCLUDE__YAJR__TRANSPORT__PLAINTEXT_HPP

namespace yajr { namespace comms { namespace transport {

class PlainText {
  public:
    static Transport::Callbacks kCallbacks = {
        alloc_cb,
        on_read,
    };

  private:
    static void alloc_cb(uv_handle_t * _, size_t size, uv_buf_t* buf);
    static void on_read(uv_stream_t * h, ssize_t nread, uv_buf_t const * buf);
};

template< >
TransportEngine< PlainText >::TransportEngine()
    :
        Transport(
            &PlainText::kCallbacks
          , NULL
        )
    {}

template< >
TransportEngine::~TransportEngine() {
}

}}}

#endif /* _____COMMS__INCLUDE__YAJR__TRANSPORT__PLAINTEXT_HPP */
