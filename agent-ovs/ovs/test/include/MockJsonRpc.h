/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEX_MOCKJSONRPC_H
#define OPFLEX_MOCKJSONRPC_H

#include "JsonRpc.h"
#include "MockRpcConnection.h"

namespace opflexagent {

/**
 * Mock object for testing
 */
class MockJsonRpc : public JsonRpc {
public:
    MockJsonRpc() : JsonRpc() {}
    virtual ~MockJsonRpc() {}

    virtual void start() {
        LOG(DEBUG) << "Starting MockJsonRpc ...";
        shared_ptr<OvsdbConnection> ptr = make_shared<MockRpcConnection>(this);
        setRpcConnectionPtr(ptr);
        getRpcConnectionPtr()->start();
    }

    virtual void connect() {
        getRpcConnectionPtr()->connect();
    }
};

}

#endif //OPFLEX_MOCKJSONRPC_H
