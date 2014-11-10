/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file MockOpflexServer.h
 * @brief Interface definition file for MockOpflexServer
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <vector>
#include <utility>

#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/OpflexListener.h"
#include "opflex/engine/internal/OpflexHandler.h"

#include "MockServerHandler.h"

#pragma once
#ifndef OPFLEX_ENGINE_TEST_MOCKOPFLEXSERVER_H
#define OPFLEX_ENGINE_TEST_MOCKOPFLEXSERVER_H

namespace opflex {
namespace engine {
namespace internal {

/**
 * An opflex server we can use for mocking interactions with a real
 * Opflex server
 */
class MockOpflexServer : public HandlerFactory {
public:
    typedef std::pair<uint8_t, std::string> peer_t;
    typedef std::vector<peer_t> peer_vec_t;

    MockOpflexServer(int port, uint8_t roles, peer_vec_t peers);
    ~MockOpflexServer();

    // See HandlerFactory::newHandler
    virtual OpflexHandler* newHandler(OpflexConnection* conn);

    const peer_vec_t& getPeers() { return peers; }
    int getPort() { return port; }
    uint8_t getRoles() { return roles; }

private:
    int port;
    uint8_t roles;

    peer_vec_t peers;

    OpflexListener listener;
};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_TEST_MOCKOPFLEXSERVER_H */

