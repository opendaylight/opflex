/*
 * Test suite for Network functions
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>

#include <opflexagent/Network.h>

#include <opflex/modb/MAC.h>

using namespace opflexagent::network;
using boost::asio::ip::address;
using boost::asio::ip::address_v6;

BOOST_AUTO_TEST_SUITE(Network_test)

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

BOOST_AUTO_TEST_CASE(test_subnet_mask) {
    struct in6_addr mask;

    BOOST_CHECK_EQUAL(0, get_subnet_mask_v4(0));
    BOOST_CHECK_EQUAL(0xffffffff, get_subnet_mask_v4(32));
    BOOST_CHECK_EQUAL(0xffff0000, get_subnet_mask_v4(16));

    boost::asio::ip::address_v6::bytes_type data;
    get_subnet_mask_v6(0, &mask);
    std::memcpy(data.data(), &mask, sizeof(struct in6_addr));
    BOOST_CHECK_EQUAL(address::from_string("::"), address_v6(data));

    get_subnet_mask_v6(128, &mask);
    std::memcpy(data.data(), &mask, sizeof(struct in6_addr));
    BOOST_CHECK_EQUAL(address::from_string("ffff:ffff:ffff:ffff:"
                                           "ffff:ffff:ffff:ffff"),
                      address_v6(data));

    get_subnet_mask_v6(48, &mask);
    std::memcpy(data.data(), &mask, sizeof(struct in6_addr));
    BOOST_CHECK_EQUAL(address::from_string("ffff:ffff:ffff::"),
                      address_v6(data));
}

BOOST_AUTO_TEST_CASE(test_mask_address) {
    BOOST_CHECK_EQUAL(address::from_string("1.2.16.0"),
                      mask_address(address::from_string("1.2.17.4"), 20));

    BOOST_CHECK_EQUAL(address::from_string("1:2:3:4:5::"),
                      mask_address(
                          address::from_string("1:2:3:4:5:6:7::"), 80));
}

BOOST_AUTO_TEST_CASE(test_cidr) {
    cidr_t cidr;

    BOOST_CHECK(cidr_from_string("1.2.3.17", cidr));
    BOOST_CHECK_EQUAL(address::from_string("1.2.3.17"), cidr.first);
    BOOST_CHECK_EQUAL(32, cidr.second);

    BOOST_CHECK(cidr_contains(cidr, address::from_string("1.2.3.17")));

    BOOST_CHECK(cidr_from_string("1.2.3.15/29", cidr));
    BOOST_CHECK_EQUAL(address::from_string("1.2.3.8"), cidr.first);
    BOOST_CHECK_EQUAL(29, cidr.second);

    BOOST_CHECK(cidr_contains(cidr, address::from_string("1.2.3.9")));
    BOOST_CHECK(!cidr_contains(cidr, address::from_string("1.2.3.16")));

    BOOST_CHECK(cidr_from_string("a:b:c:d:e:f::", cidr));
    BOOST_CHECK_EQUAL(address::from_string("a:b:c:d:e:f::"), cidr.first);
    BOOST_CHECK_EQUAL(128, cidr.second);

    BOOST_CHECK(cidr_contains(cidr, address::from_string("a:b:c:d:e:f::")));

    BOOST_CHECK(cidr_from_string("f:9:d:6:e::/64", cidr));
    BOOST_CHECK_EQUAL(address::from_string("f:9:d:6::"), cidr.first);
    BOOST_CHECK_EQUAL(64, cidr.second);

    BOOST_CHECK(cidr_contains(cidr, address::from_string("f:9:d:6:e::3e")));
    BOOST_CHECK(!cidr_contains(cidr, address::from_string("f:9:e:6::")));

    BOOST_CHECK(!cidr_from_string("1.2.3.2000/29", cidr));
    BOOST_CHECK(!cidr_from_string("a:b:c::/f", cidr));
    BOOST_CHECK(!cidr_from_string("foo.bar", cidr));
}

BOOST_AUTO_TEST_CASE(test_link_local) {
    using opflex::modb::MAC;
    BOOST_CHECK_EQUAL(address_v6::from_string("fe80::500c:47ff:fe97:a6ab"),
                      construct_link_local_ip_addr(MAC("52:0c:47:97:a6:ab")));
    BOOST_CHECK_EQUAL(address_v6::from_string("fe80::200:ff:fe00:2"),
                      construct_link_local_ip_addr(MAC("00:00:00:00:00:02")));
    BOOST_CHECK_EQUAL(address_v6::from_string("fe80::200:ff:fe00:1"),
                      construct_link_local_ip_addr(MAC("00:00:00:00:00:01")));
    BOOST_CHECK_EQUAL(address_v6::from_string("fe80::200:ff:fe00:8000"),
                      construct_link_local_ip_addr(MAC("00:00:00:00:80:00")));

}

BOOST_AUTO_TEST_CASE(test_solicited_node) {
#define a6 address_v6::from_string
#define cni construct_solicited_node_ip
    BOOST_CHECK_EQUAL(a6("ff02::1:ff28:9c5a"),
                      cni(a6("fe80::2aa:ff:fe28:9c5a")));
    BOOST_CHECK_EQUAL(a6("ff02::1:ff00:3"),
                      cni(a6("fd2a:c2c1:d59e:bcde::3")));
#undef a6
#undef cni
}

BOOST_AUTO_TEST_SUITE_END()
