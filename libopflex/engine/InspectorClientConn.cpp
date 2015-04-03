/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for InspectorClientConn class.
 *
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

#include "opflex/engine/internal/InspectorClientConn.h"
#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/logging/internal/logging.hpp"

namespace opflex {
namespace engine {
namespace internal {

using yajr::Peer;

InspectorClientConn::InspectorClientConn(HandlerFactory& handlerFactory,
                                         InspectorClientImpl* client_,
                                         const std::string& name_)
    : OpflexConnection(handlerFactory), client(client_),
      name(name_), peer(NULL) {

}

InspectorClientConn::~InspectorClientConn() {

}

void InspectorClientConn::connect() {
    uv_loop_init(&client_loop);
    yajr::initLoop(&client_loop);

    timer.data = this;
    uv_timer_init(&client_loop, &timer);
    uv_timer_start(&timer, on_timer, 5000, 5000);

    peer = yajr::Peer::create(name,
                              on_state_change,
                              this, loop_selector);

    uv_run(&client_loop, UV_RUN_DEFAULT);
    uv_loop_close(&client_loop);
}

void InspectorClientConn::disconnect() {
    close();
}

void InspectorClientConn::messagesReady() {

}

void InspectorClientConn::close() {
    OpflexConnection::disconnect();
    if (peer) peer->destroy();
    peer = NULL;
}

uv_loop_t* InspectorClientConn::loop_selector(void * data) {
    InspectorClientConn* conn = (InspectorClientConn*)data;
    return &conn->client_loop;
}

void InspectorClientConn::on_state_change(Peer * p, void * data,
                                          yajr::StateChange::To stateChange,
                                          int error) {
    InspectorClientConn* conn = (InspectorClientConn*)data;
    switch (stateChange) {
    case yajr::StateChange::CONNECT:
        conn->handler->connected();
        break;
    case yajr::StateChange::DISCONNECT:
        conn->handler->disconnected();
        conn->close();
        break;
    case yajr::StateChange::TRANSPORT_FAILURE:
        break;
    case yajr::StateChange::FAILURE:
        LOG(ERROR) << "Connection error: " << uv_strerror(error);
        conn->close();
        break;
    case yajr::StateChange::DELETE:
        LOG(INFO) << "Connection closed";

        uv_timer_stop(&conn->timer);
        uv_close((uv_handle_t*)&conn->timer, NULL);
        yajr::finiLoop(&conn->client_loop);
        break;
    }
}

void InspectorClientConn::on_timer(uv_timer_t* timer) {
    InspectorClientConn* conn = (InspectorClientConn*)timer->data;
    LOG(ERROR) << "Timed out on operation";
    conn->close();
}

const std::string& BAD_NAME("BAD_NAME");
const std::string& BAD_DOMAIN("BAD_DOMAIN");
const std::string& InspectorClientConn::getName() { return BAD_NAME; }
const std::string& InspectorClientConn::getDomain() { return BAD_DOMAIN; }

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
