/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexConnection
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


#include <boost/scoped_ptr.hpp>

#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/logging/internal/logging.hpp"
#include "LockGuard.h"

#include "yajr/transport/ZeroCopyOpenSSL.hpp"
#include "yajr/rpc/message_factory.hpp"

static uv_once_t ssl_once = UV_ONCE_INIT;

namespace opflex {
namespace engine {
namespace internal {

using rapidjson::Value;
using std::string;
using boost::scoped_ptr;
using yajr::rpc::OutboundRequest;
using yajr::rpc::OutboundResult;
using yajr::rpc::OutboundError;
using yajr::transport::ZeroCopyOpenSSL;

OpflexConnection::OpflexConnection(HandlerFactory& handlerFactory)
    : handler(handlerFactory.newHandler(this))
    ,requestId(1) ,connGeneration(0)
{
    uv_mutex_init(&queue_mutex);
    connect();
}

OpflexConnection::~OpflexConnection() {
    cleanup();
    if (handler)
        delete handler;
    uv_mutex_destroy(&queue_mutex);
}

static void init_ssl() {
    ZeroCopyOpenSSL::initOpenSSL(true);
}

void OpflexConnection::initSSL() {
    uv_once(&ssl_once, init_ssl);
}

void OpflexConnection::connect() {}

void OpflexConnection::cleanup() {
    util::LockGuard guard(&queue_mutex);
    connGeneration += 1;
    while (write_queue.size() > 0) {
        delete write_queue.front().first;
        write_queue.pop_front();
    }
}

void OpflexConnection::disconnect() {
    cleanup();
}

void OpflexConnection::close() {
    disconnect();
}

bool OpflexConnection::isReady() {
    return handler->isReady();
}

void OpflexConnection::notifyReady() {

}

void OpflexConnection::doWrite(OpflexMessage* message) {
    if (getPeer() == NULL) return;

    PayloadWrapper wrapper(message);
    switch (message->getType()) {
    case OpflexMessage::REQUEST:
        {
            yajr::rpc::MethodName method(message->getMethod().c_str());
            uint64_t xid = message->getReqXid();
            if (xid == 0) xid = requestId++;
            OutboundRequest outm(wrapper, &method, xid, getPeer());
            outm.send();
        }
        break;
    case OpflexMessage::RESPONSE:
        {
            OutboundResult outm(*getPeer(), wrapper, message->getId());
            outm.send();
        }
        break;
    case OpflexMessage::ERROR_RESPONSE:
        {
            OutboundError outm(*getPeer(), wrapper, message->getId());
            outm.send();
        }
        break;
    }
}

void OpflexConnection::processWriteQueue() {
    util::LockGuard guard(&queue_mutex);
    while (write_queue.size() > 0) {
        const write_queue_item_t& qi = write_queue.front();
        // Avoid writing messages from a previous reconnect attempt
        if (qi.second < connGeneration) {
            LOG(DEBUG) << "Ignoring " << qi.first->getMethod()
                       << " of type " << qi.first->getType();
            continue;
        }
        scoped_ptr<OpflexMessage> message(qi.first);
        write_queue.pop_front();
        doWrite(message.get());
    }
}

void OpflexConnection::sendMessage(OpflexMessage* message, bool sync) {
    if (sync) {
        scoped_ptr<OpflexMessage> messagep(message);
        doWrite(message);
    } else {
        util::LockGuard guard(&queue_mutex);
        write_queue.push_back(std::make_pair(message, connGeneration));
    }
    messagesReady();
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
