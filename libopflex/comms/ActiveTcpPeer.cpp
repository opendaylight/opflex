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
::boost::atomic<size_t> ActiveTcpPeer::counter(0);
#endif

void ::yajr::comms::internal::ActiveTcpPeer::retry() {

    if (destroying_) {

        LOG(INFO)
            << this
            << "Not retrying because of pending destroy"
        ;

        return;

    }
    if (uvRefCnt_ != 1) {
        LOG(INFO)
            << this
            << " has to wait for reference count to fall to 1 before retrying"
        ;

        insert(internal::Peer::LoopData::RETRY_TO_CONNECT);

        return;

    }


    struct addrinfo const hints = (struct addrinfo){
        /* .ai_flags    = */ 0,
        /* .ai_family   = */ AF_UNSPEC,
        /* .ai_socktype = */ SOCK_STREAM,
        /* .ai_protocol = */ IPPROTO_TCP,
    };

    /* potentially switch uv loop if some rebalancing is needed */
    uv_loop_t * newLoop = uvLoopSelector_(getData());

    if (newLoop != getHandle()->loop) {
        getLoopData()->down();
        getHandle()->loop = newLoop;
        getLoopData()->up();
    }

    int rc;
    if ((rc = uv_getaddrinfo(
                    getUvLoop(),
                    &dns_req_,
                    on_resolved,
                    getHostname(),
                    getService(),
                    &hints))) {
        LOG(WARNING)
            << "uv_getaddrinfo: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
        ;
        onError(rc);
        insert(internal::Peer::LoopData::RETRY_TO_CONNECT);
    } else {
        VLOG(1)
            << this
            << " up() for a pending getaddrinfo()";
        PLOG('D');
        up();
        insert(internal::Peer::LoopData::ATTEMPTING_TO_CONNECT);
    }

}

void ActiveTcpPeer::onFailedConnect(int rc) {

    extern void retry_later(ActivePeer * peer);

    PLOG('#');
    if ((rc = connect_to_next_address(this))) {
        LOG(WARNING)
            << this
            << "connect: no more resolved addresses"
        ;

        /* since all attempts for all possible IP addresses have failed,
         * issue an error callback
         */
        onError(rc);

        if (!uv_is_closing((uv_handle_t*)this->getHandle())) {
            PLOG('M');
            uv_close((uv_handle_t*)this->getHandle(), on_close);
        } else PLOG('m');

        /* retry later */
        retry_later(this);
    }

}

} // namespace internal
} // namespace comms
} // namespace yajr

