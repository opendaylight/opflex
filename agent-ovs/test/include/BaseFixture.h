/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for base fixture
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <glog/logging.h>

#include <opflex/ofcore/OFFramework.h>
#include <modelgbp/metadata/metadata.hpp>

#include "Agent.h"

#pragma once
#ifndef OVSAGENT_TEST_BASEFIXTURE_H
#define OVSAGENT_TEST_BASEFIXTURE_H

namespace ovsagent {

using opflex::ofcore::MockOFFramework;

/**
 * A fixture that adds an object store
 */
class BaseFixture {
public:
    BaseFixture() : agent(framework) {
        google::InitGoogleLogging("");
        agent.start();
    }

    virtual ~BaseFixture() {
        agent.stop();
    }

    MockOFFramework framework;
    Agent agent;
};

} /* namespace ovsagent */

#endif /* OVSAGENT_TEST_BASEFIXTURE_H */
