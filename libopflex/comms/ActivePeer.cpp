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

void ActivePeer::destroy(bool now) {

    VLOG(6)
        << this
    ;

    bool alreadyBeingDestroyed = destroying_;

    if (!alreadyBeingDestroyed || now) {
        CommunicationPeer::destroy(now);
    }

    if (alreadyBeingDestroyed) {
        VLOG(1)
            << this
            << " multiple destroy()s detected"
        ;
        return;
    }

    VLOG(5)
        << this
        << " down() for destruction"
    ;
    down();
}

} // namespace internal
} // namespace comms
} // namespace yajr

