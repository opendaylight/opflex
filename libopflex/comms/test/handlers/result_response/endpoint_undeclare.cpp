/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif


#include <yajr/rpc/methods.hpp>

namespace yajr {
    namespace rpc {

template<>
void InbRes<&yajr::rpc::method::endpoint_undeclare>::process() const {

    VLOG(6)
        << "Got endpoint_undeclare reply at "
        << getReceived()
    ;

}

}
}

