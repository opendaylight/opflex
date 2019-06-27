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

#include <yajr/transport/engine.hpp>

#include <cassert>

namespace yajr {
    namespace transport {

class PlainText : public Transport::Engine {
  public:
    static TransportEngine< PlainText > & getPlainTextTransport();
  private:
    PlainText();
    PlainText(const PlainText &);
    PlainText & operator=(const PlainText &);
};

template< >
TransportEngine< PlainText >::TransportEngine(PlainText * _ /* = NULL */);

template< >
inline TransportEngine< PlainText >::~TransportEngine() {
}

} /* yajr::transport namespace */
} /* yajr namespace */

#endif /* _____COMMS__INCLUDE__YAJR__TRANSPORT__PLAINTEXT_HPP */

