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

#include <opflex/logging/InMemoryLogHandler.hpp>

namespace asynchronous_sockets {

    extern opflex::logging::InMemoryLogHandler const & iMLH;

}

namespace yajr {
    namespace rpc {

template<>
void InbReq<&yajr::rpc::method::get_debug_logs>::process() const {

    OutboundResult (
            this,
            asynchronous_sockets::iMLH
        )
        . send();

    VLOG(6);

}

} /* yajr::rpc namespace */
} /* yajr namespace */

