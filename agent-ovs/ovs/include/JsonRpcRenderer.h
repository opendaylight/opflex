/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for JsonRpcRendered
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef OPFLEX_JSONRPCRENDERER_H
#define OPFLEX_JSONRPCRENDERER_H

#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <opflexagent/Agent.h>
#include "JsonRpc.h"

namespace opflexagent {

using boost::asio::deadline_timer;
using boost::posix_time::milliseconds;

class JsonRpcRenderer {

protected:
    JsonRpcRenderer(Agent &agent_);
    bool connect();
    void cleanup();

protected:
    Agent &agent;
    TaskQueue taskQueue;
    condition_variable cv;
    mutex handlerMutex;
    unique_ptr<JsonRpc> jRpc;
    std::shared_ptr<boost::asio::deadline_timer> connection_timer;
    // retry interval in seconds
    const long CONNECTION_RETRY = 60;
    bool timerStarted = false;
};
}
#endif //OPFLEX_JSONRPCRENDERER_H