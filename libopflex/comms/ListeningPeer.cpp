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


#include <yajr/internal/comms.hpp>

#include <opflex/logging/internal/logging.hpp>

namespace yajr {
    namespace comms {
        namespace internal {

void ListeningPeer::destroy(bool now) {

    VLOG(1)
        << this
    ;
 // Peer::destroy();

    assert(!destroying_);
    if (destroying_) {
        LOG(WARNING)
            << this
            << " Double destroy() detected"
        ;

        return;
    }

    destroying_ = 1;

    if (down()) {
        return;
    }

    if (connected_) {
        connected_ = 0;
        if (!uv_is_closing((uv_handle_t*)getHandle())) {
            uv_close((uv_handle_t*)getHandle(), on_close);
        }
    }
}

} // namespace internal
} // namespace comms
} // namespace yajr

