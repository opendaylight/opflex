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


#include <opflex/yajr/internal/comms.hpp>

#include <opflex/logging/internal/logging.hpp>

namespace yajr {
    namespace comms {
        namespace internal {

void ::yajr::comms::internal::ActiveUnixPeer::onFailedConnect(int rc) {

    void retry_later(ActivePeer * peer);

    onError(rc);

    if (!uv_is_closing(this->getHandle())) {
        uv_close(this->getHandle(), on_close);
    }

    /* retry later */
    retry_later(this);
}

void ::yajr::comms::internal::ActiveUnixPeer::retry() {

    if (destroying_) {
        LOG(INFO) << this << "Not retrying because of pending destroy";
        return;
    }
    if (uvRefCnt_ != 1) {
        LOG(INFO) << this  << " has to wait for reference count to fall to 1 before retrying";
        insert(internal::Peer::LoopData::RETRY_TO_CONNECT);
        return;
    }

    int rc;
    if ((rc = uv_pipe_init(
                    getUvLoop(),
                    reinterpret_cast<uv_pipe_t *>(getHandle()),
                    0))) {
        LOG(WARNING)
            << "uv_pipe_init: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
            ;
        onError(rc);
        insert(internal::Peer::LoopData::RETRY_TO_CONNECT);
        return;
    }
 
    /* potentially switch uv loop if some rebalancing is needed */
    uv_loop_t * newLoop = uvLoopSelector_(getData());

    if (newLoop != getHandle()->loop) {
        getLoopData()->down();
        getHandle()->loop = newLoop;
        getLoopData()->up();
    }

    /* uv_pipe_connect errors are always asynchronous */
    uv_pipe_connect(&connect_req_,
                    reinterpret_cast<uv_pipe_t *>(getHandle()),
                    socketName_.c_str(),
                    on_active_connection);
    /* workaround for libuv synchronous uv_pipe_connect() failures bug */
    getLoopData()->kickLibuv();

    up();
    status_ = internal::Peer::kPS_CONNECTING;
    insert(internal::Peer::LoopData::ATTEMPTING_TO_CONNECT);
}


} // namespace internal
} // namespace comms
} // namespace yajr
