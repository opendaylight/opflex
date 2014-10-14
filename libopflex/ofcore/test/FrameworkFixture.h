/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for base framework fixture
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef CORE_TEST_FRAMEWORKFIXTURE_H
#define CORE_TEST_FRAMEWORKFIXTURE_H

#include "opflex/ofcore/OFFramework.h"
#include "MDFixture.h"

namespace opflex {
namespace ofcore {

/**
 * A fixture that adds a framework instance
 */
class FrameworkFixture : public modb::MDFixture {
public:
    FrameworkFixture()
        : MDFixture() {
        framework.setModel(md);
        framework.start();
    }

    ~FrameworkFixture() {
        framework.stop();
    }

    MockOFFramework framework;
};

} /* namespace modb */
} /* namespace opflex */

#endif /* CORE_TEST_FRAMEWORKFIXTURE_H */
