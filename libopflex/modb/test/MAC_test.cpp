/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for MAC.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>

#include "opflex/modb/MAC.h"

BOOST_AUTO_TEST_SUITE(MAC_test)

using namespace opflex;
using namespace modb;

BOOST_AUTO_TEST_CASE( basic ) {
    MAC mac1(0x112233445566llu);
    MAC mac2("11:22:33:44:55:66");
    uint8_t bytes[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    MAC mac3(bytes);

    MAC mac4 = mac2;
    uint8_t bytes2[6] = {0x0a, 0xbb, 0xcc, 0xdd, 0x0e, 0xff};
    MAC mac5(bytes2);
    MAC mac6("a:bb:cc:dd:e:ff");

    BOOST_CHECK_EQUAL("11:22:33:44:55:66", mac1.toString());
    BOOST_CHECK_EQUAL("11:22:33:44:55:66", mac2.toString());
    BOOST_CHECK_EQUAL("11:22:33:44:55:66", mac3.toString());
    BOOST_CHECK_EQUAL("0a:bb:cc:dd:0e:ff", mac5.toString());
    BOOST_CHECK_EQUAL(mac2, mac3);
    BOOST_CHECK_EQUAL(mac4, mac3);
    BOOST_CHECK_EQUAL(mac5, mac6);
    BOOST_CHECK_EQUAL(MAC("10:ff:00:a3:01:01"), MAC(0x10ff00a30101llu));
    BOOST_CHECK(mac4 != mac5);
}

BOOST_AUTO_TEST_CASE( error ) {
    BOOST_CHECK_THROW(MAC(""), std::invalid_argument);
    BOOST_CHECK_THROW(MAC("gg:11:22:33:44:44:55"), std::invalid_argument);
    BOOST_CHECK_THROW(MAC("00:11:22:33:44::55"), std::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END()
