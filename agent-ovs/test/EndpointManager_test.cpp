/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for endpoint manager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>

#include "BaseFixture.h"

namespace ovsagent {

BOOST_AUTO_TEST_SUITE(EndpointManager_test)

BOOST_FIXTURE_TEST_CASE( basic, BaseFixture ) {
    LOG(INFO) << "Test";

}

BOOST_AUTO_TEST_SUITE_END()

} /* namespace ovsagent */
