/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for mock endpoint source
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_TEST_MOCKENDPOINTSOURCE_H
#define OPFLEXAGENT_TEST_MOCKENDPOINTSOURCE_H

namespace opflexagent {

class MockEndpointSource : public EndpointSource {
public:
    MockEndpointSource(EndpointManager* manager)
        : EndpointSource(manager) {

    }

    virtual ~MockEndpointSource() { }

    virtual void start() { }
    virtual void stop() { }
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_TEST_MOCKENDPOINTSOURCE_H */
