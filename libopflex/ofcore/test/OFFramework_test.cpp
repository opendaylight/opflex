/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for OFFramework class.
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


#include "config.h"

#include <boost/test/unit_test.hpp>

#include "opflex/ofcore/OFFramework.h"


BOOST_AUTO_TEST_SUITE(OFFramework_test)

BOOST_AUTO_TEST_CASE( version ) {
    using opflex::ofcore::OFFramework;
    const std::vector<int> v = OFFramework::getVersion();

    BOOST_CHECK_EQUAL(SDK_PVERSION, v[0]);
    BOOST_CHECK_EQUAL(SDK_SVERSION, v[1]);
    BOOST_CHECK_EQUAL(SDK_IVERSION, v[2]);
    BOOST_CHECK_EQUAL(3, v.size());
    BOOST_CHECK_EQUAL(SDK_FULL_VERSION, OFFramework::getVersionStr());
}

BOOST_AUTO_TEST_CASE( init ) {
    using opflex::ofcore::OFFramework;

    OFFramework fw;

    fw.start();
    fw.stop();
}

BOOST_AUTO_TEST_CASE( init_adaptor ) {
    using opflex::ofcore::OFFramework;

    OFFramework fw;
    opflex::ofcore::MainLoopAdaptor* adaptor = fw.startSync();
    adaptor->runOnce();
    fw.stop();
}

BOOST_AUTO_TEST_SUITE_END()
