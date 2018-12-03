/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _INCLUDE__YAJR__INTERNAL__WALKANDDUMPHANDLESCB_HPP
#define _INCLUDE__YAJR__INTERNAL__WALKANDDUMPHANDLESCB_HPP

#include <opflex/logging/internal/logging.hpp>

namespace yajr {
    namespace comms {
        namespace internal {

template < ::opflex::logging::OFLogHandler::Level LOGGING_LEVEL >
void Peer::LoopData::walkAndDumpHandlesCb(uv_handle_t* h, void* loopData) {

    LOG(LOGGING_LEVEL)
        << static_cast<Peer::LoopData const *>(loopData)
        << " pending handle of type "
        << getUvHandleType(h)
        << " @"
        << reinterpret_cast<void const *>(h)
        << " which is "
        << (uv_is_closing(h) ? "" : "not ")
        << "closing"
        ;

}

} // namespace internal
} // namespace comms
} // namespace yajr

#endif /* _INCLUDE__YAJR__INTERNAL__WALKANDDUMPHANDLESCB_HPP */

