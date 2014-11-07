/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexServerConnection
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <arpa/inet.h>

#include "opflex/engine/internal/OpflexServerConnection.h"
#include "opflex/engine/internal/OpflexListener.h"
#include "opflex/logging/internal/logging.hpp"
#include "LockGuard.h"

namespace opflex {
namespace engine {
namespace internal {

using std::string;

OpflexServerConnection::OpflexServerConnection(OpflexListener* listener_)
    : OpflexConnection(listener_->handlerFactory), 
      listener(listener_) {
    uv_tcp_init(&listener->server_loop, &tcp_handle);
    tcp_handle.data = this;
    int rc = uv_accept((uv_stream_t*)&listener->bind_socket, 
                       (uv_stream_t*)&tcp_handle);
    if (rc < 0) {
        LOG(ERROR) << "Could not accept connection: "
                   << uv_strerror(rc);
    }

    struct sockaddr_storage name;
    int len = sizeof(name);
    rc = uv_tcp_getpeername(&tcp_handle, (struct sockaddr*)&name, &len);
    if (rc < 0) {
        LOG(ERROR) << "New connection but could not get remote peer IP address"
                   << uv_strerror(rc);
    } else {
        char addrbuffer[INET6_ADDRSTRLEN];
        inet_ntop(name.ss_family, 
                  name.ss_family == AF_INET
                  ? (void *) &(((struct sockaddr_in*)&name)->sin_addr)
                  : (void *) &(((struct sockaddr_in6*)&name)->sin6_addr),
                  addrbuffer, INET6_ADDRSTRLEN);
        remote_peer = addrbuffer;
        LOG(INFO) << "New connection from " << remote_peer;
    }

    uv_read_start((uv_stream_t*)&tcp_handle, alloc_cb, read_cb);
}

OpflexServerConnection::~OpflexServerConnection() {

}

const std::string& OpflexServerConnection::getName() {
    return listener->getName();
}

const std::string& OpflexServerConnection::getDomain() {
    return listener->getDomain();
}

void OpflexServerConnection::shutdown_cb(uv_shutdown_t* req, int status) {
    OpflexServerConnection* conn = 
        (OpflexServerConnection*)req->handle->data;
    uv_close((uv_handle_t*)&conn->tcp_handle, OpflexListener::on_conn_closed);
}

void OpflexServerConnection::disconnect() {
    uv_read_stop((uv_stream_t*)&tcp_handle);
    {
        util::LockGuard guard(&write_mutex);
        int rc = uv_shutdown(&shutdown, (uv_stream_t*)&tcp_handle, shutdown_cb);
        if (rc < 0) {
            LOG(ERROR) << "Could not shut down socket: " << uv_strerror(rc);
        }
    }
}

void OpflexServerConnection::write(const rapidjson::StringBuffer* buf) {
    OpflexConnection::write((uv_stream_t*)&tcp_handle, buf);
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
