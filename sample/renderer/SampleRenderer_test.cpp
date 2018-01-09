/*
 * Test suite for SampleRenderer
 *
 * Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>

#include <opflexagent/test/ModbFixture.h>

#include "SampleRenderer.h"

BOOST_AUTO_TEST_SUITE(SampleRenderer_test)

BOOST_FIXTURE_TEST_CASE(sample, opflexagent::ModbFixture) {
    samplerenderer::SampleRenderer sample(agent);
    sample.start();
    sample.stop();
}

BOOST_AUTO_TEST_SUITE_END()
