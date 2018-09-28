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

#ifdef COMMS_DEBUG_OBJECT_COUNT
::boost::atomic<size_t> PassivePeer::counter(0);
#endif

#ifdef EXTRA_CHECKS
bool PassivePeer::__checkInvariants() const {
    return CommunicationPeer::__checkInvariants();
}
#endif

} // namespace internal
} // namespace comms
} // namespace yajr

