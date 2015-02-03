/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OVSAGENT_TEST_MOCKSWITCHCONNECTION_H_
#define OVSAGENT_TEST_MOCKSWITCHCONNECTION_H_

#include "SwitchConnection.h"

/**
 * Mock switch connection object useful for tests
 */
class MockSwitchConnection : public SwitchConnection {
public:
    MockSwitchConnection() : SwitchConnection("mockBridge") {
    }
    virtual ~MockSwitchConnection() {
        clear();
    }

    void clear() {
        BOOST_FOREACH(ofpbuf* msg, sentMsgs) {
            ofpbuf_delete(msg);
        }
        sentMsgs.clear();
    }

    ofp_version GetProtocolVersion() { return OFP13_VERSION; }

    int SendMessage(ofpbuf *msg) {
        sentMsgs.push_back(msg);
        return 0;
    }

    std::vector<ofpbuf*> sentMsgs;
};

#endif /* OVSAGENT_TEST_MOCKSWITCHCONNECTION_H_ */
