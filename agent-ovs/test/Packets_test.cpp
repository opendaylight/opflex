/*
 * Test suite for class Packets functions
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>

#include "Packets.h"

using namespace ovsagent::packets;
using boost::asio::ip::address_v6;

BOOST_AUTO_TEST_SUITE(Packets_test)

BOOST_AUTO_TEST_CASE(chksum) {
    uint8_t packet[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x88, 0xf0, 0x31, 0xb5,
        0x12, 0xb5, 0x86, 0xdd, 0x60, 0x00, 0x00, 0x00, 0x00, 0x18,
        0x3a, 0xff, 0xfd, 0x8c, 0xad, 0x36, 0xce, 0xb3, 0x60, 0x1f,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xfd, 0x8c,
        0xad, 0x36, 0xce, 0xb3, 0x60, 0x1f, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x05, 0x88, 0x00, 0x00, 0x00, 0xe0, 0x00,
        0x00, 0x00, 0xfd, 0x8c, 0xad, 0x36, 0xce, 0xb3, 0x60, 0x1f,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1};

    uint32_t chksum = 0;
    chksum_accum(chksum, (uint16_t*)(packet+14+8), 32);
    uint32_t plen = htonl(24);
    chksum_accum(chksum, (uint16_t*)&plen, 4);
    uint16_t nxthdr = htons(58);
    chksum_accum(chksum, (uint16_t*)&nxthdr, 2);
    chksum_accum(chksum, (uint16_t*)(packet+14+40), 24);
    uint16_t result = htons(chksum_finalize(chksum));

    BOOST_CHECK_EQUAL(0x0ae0, result);
}

BOOST_AUTO_TEST_CASE(ipv6_subnet) {
    struct in6_addr mask;
    struct in6_addr addr;

    compute_ipv6_subnet(address_v6::from_string("ad::da"), 128, &mask, &addr);

    boost::asio::ip::address_v6::bytes_type data;
    std::memcpy(data.data(), &mask, sizeof(struct in6_addr));
    BOOST_CHECK_EQUAL(address_v6(data),
                      address_v6::from_string("ffff:ffff:ffff:ffff:"
                                              "ffff:ffff:ffff:ffff"));

    std::memcpy(data.data(), &addr, sizeof(struct in6_addr));
    BOOST_CHECK_EQUAL(address_v6(data),
                      address_v6::from_string("ad::da"));

    compute_ipv6_subnet(address_v6::from_string("ad::da"), 64, &mask, &addr);
    std::memcpy(data.data(), &mask, sizeof(struct in6_addr));
    BOOST_CHECK_EQUAL(address_v6(data),
                      address_v6::from_string("ffff:ffff:ffff:ffff::"));

    std::memcpy(data.data(), &addr, sizeof(struct in6_addr));
    BOOST_CHECK_EQUAL(address_v6(data),
                      address_v6::from_string("ad::"));

    compute_ipv6_subnet(address_v6::from_string("ad00::da"), 1, &mask, &addr);
    std::memcpy(data.data(), &mask, sizeof(struct in6_addr));
    BOOST_CHECK_EQUAL(address_v6(data),
                      address_v6::from_string("8000::"));

    std::memcpy(data.data(), &addr, sizeof(struct in6_addr));
    BOOST_CHECK_EQUAL(address_v6(data),
                      address_v6::from_string("8000::"));

    compute_ipv6_subnet(address_v6::from_string("ad::da"), 0, &mask, &addr);
    std::memcpy(data.data(), &mask, sizeof(struct in6_addr));
    BOOST_CHECK_EQUAL(address_v6(data),
                      address_v6::from_string("::"));

    std::memcpy(data.data(), &addr, sizeof(struct in6_addr));
    BOOST_CHECK_EQUAL(address_v6(data),
                      address_v6::from_string("::"));

}

BOOST_AUTO_TEST_SUITE_END()
