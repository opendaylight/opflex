/*
 * Test suite for class KeyedRateLimiter
 *
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/KeyedRateLimiter.h>
#include <opflexagent/logging.h>

#include <boost/test/unit_test.hpp>

#include <thread>
#include <chrono>

namespace opflexagent {

BOOST_AUTO_TEST_SUITE(KeyedRateLimiter_test)

BOOST_AUTO_TEST_CASE(limit) {
    KeyedRateLimiter<std::string, 5, 10> l;
    BOOST_CHECK(l.event("test"));
    BOOST_CHECK_EQUAL(false, l.event("test"));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    BOOST_CHECK_EQUAL(false, l.event("test"));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    BOOST_CHECK(l.event("test"));

    std::this_thread::sleep_for(std::chrono::milliseconds(55));
    BOOST_CHECK(l.event("test"));
}

BOOST_AUTO_TEST_SUITE_END()

}
