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
#include <yajr/rpc/gen/echo.hpp>

#include <cstring>

namespace yajr {
    namespace rpc {

template<>
void InbRes<&yajr::rpc::method::echo>::process() const {

    VLOG(6)
        << "Got echo reply at "
        << getReceived()
    ;

#if 0
    rapidjson::Value const & payload = getPayload();

    for(size_t i = 1; i <= yajr::comms::internal::EchoGen::kNcanaries; ++i) {
        VLOG(7)
            << "About to check canary number "
            << i
        ;
        if (payload[i].GetStringLength() != strlen(yajr::comms::internal::EchoGen::canary)) {
            LOG(ERROR)
                << "wrong lenght for canary number "
                << i
            ;
            assert(0);
        }
        if (strncmp(payload[i].GetString(), yajr::comms::internal::EchoGen::canary,
                1+strlen(yajr::comms::internal::EchoGen::canary))) {
            LOG(ERROR)
                << "wrong value for canary number "
                << i
            ;
            assert(0);
        }
    }
#endif

}

}
}

