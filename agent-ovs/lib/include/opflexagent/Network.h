/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Utility functions for network addresses and prefixes
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_NETWORK_H
#define OPFLEXAGENT_NETWORK_H

#include <utility>
#include <string>
#include <unordered_set>

#include <boost/asio/ip/address.hpp>
#include <opflex/modb/MAC.h>

#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/ip6.h>
#include <netinet/ip.h>

namespace opflexagent {
namespace network {

/**
 * Type representing a subnet IP addres string and prefix length
 */
typedef std::pair<std::string, uint8_t> subnet_t;

} // namespace network
} // namespace opflexagent

namespace std {

/**
 * Template specialization for std::hash<subnet_t>, making
 * it suitable as a key in a std::unordered_map
 */
template<> struct hash<opflexagent::network::subnet_t> {
    /**
     * Hash the subnet_t
     */
    std::size_t operator()(const opflexagent::network::subnet_t& u) const;
};

} /* namespace std */

namespace opflexagent {
namespace network {

/**
 * A set of subnets
 */
typedef std::unordered_set<subnet_t> subnets_t;

/**
 * subnet_t stream insertion
 */
std::ostream& operator<<(std::ostream &os, const subnet_t& subnet);

/**
 * subnets_t stream insertion
 */
std::ostream& operator<<(std::ostream &os, const subnets_t& subnets);

/**
 * For a subnet with prefix length 64, construct an IP address
 * using the EUI-64 format in the lower 64 bits.
 * @param prefix the prefix address
 * @param srcMac the MAC address of the interface
 * @param dstAddr the in6_addr struct in which the result will be stored
 */
void construct_auto_ip(boost::asio::ip::address_v6 prefix,
                       const uint8_t* srcMac,
                       /* out */ struct in6_addr* dstAddr);

/**
 * For a subnet with prefix length 64, construct an IP address
 * using the EUI-64 format in the lower 64 bits.
 * @param prefix the prefix address
 * @param srcMac the MAC address of the interface
 * @return the ip address object
 */
boost::asio::ip::address_v6
construct_auto_ip_addr(boost::asio::ip::address_v6 prefix,
                       const uint8_t* srcMac);

/**
 * Construct a link-local IPv6 address for the given MAC
 *
 * @param srcMac the MAC address of the interface
 * @return the ip address object
 */
boost::asio::ip::address_v6
construct_link_local_ip_addr(const uint8_t* srcMac);

/**
 * Construct a link-local IPv6 address for the given MAC
 *
 * @param srcMac the MAC address of the interface
 * @return the ip address object
 */
boost::asio::ip::address_v6
construct_link_local_ip_addr(const opflex::modb::MAC& srcMac);

/**
 * Construct the solicited-node multicast address for the given IP
 * address
 *
 * @param ip the IP address to use
 * @return the solicited-node multicast address corresponding to the IP
 */
boost::asio::ip::address_v6
construct_solicited_node_ip(const boost::asio::ip::address_v6& ip);

/**
 * Get IPv4 subnet mask for specified prefix length.
 *
 * @param prefixLen the prefix length
 * @return the subnet mask
 */
uint32_t get_subnet_mask_v4(uint8_t prefixLen);

/**
 * Get IPv6 subnet mask for specified prefix length.
 *
 * @param prefixLen the prefix length
 * @param mask return the subnet mask
 */
void get_subnet_mask_v6(uint8_t prefixLen, in6_addr *mask);

/**
 * Compute the mask for an IPv6 subnet
 *
 * @param netAddr the network address for the subnet
 * @param prefixLen the prefix length
 * @param mask return the computed mask
 * @param addr return the masked network address
 */
void compute_ipv6_subnet(boost::asio::ip::address_v6 netAddr,
                         uint8_t prefixLen,
                         /* out */ struct in6_addr* mask,
                         /* out */ struct in6_addr* addr);

/**
 * Retain prefix of an IP address and zero out the remaining bits.
 *
 * @param addrIn IP address to mask
 * @param prefixLen number of prefix bits to retain
 * @return the masked IP address
 */
boost::asio::ip::address mask_address(const boost::asio::ip::address& addrIn,
                                      uint8_t prefixLen);

/**
 * Check whether the given IP address is a link-local IPv4 address in
 * 169.254/16 or a link-local IPv6 address in fe80::/8
 *
 * @param addr the address to check
 * @return true if the address is link-local
 */
bool is_link_local(const boost::asio::ip::address& addr);

/**
 * Convenience typedef to represent CIDRs.
 * First element is the base IP address and the second element is the
 * prefix length.
 */
typedef std::pair<boost::asio::ip::address, uint8_t> cidr_t;

/**
 * Parse the string representation of CIDR into an object of CIDR type.
 *
 * @param cidrStr the CIDR string to parse
 * @param cidr the output CIDR object if parsing was successful
 * @return true if the provided CIDR string is valid and was successfully
 * parsed
 */
bool cidr_from_string(const std::string& cidrStr, cidr_t& cidr);

/**
 * Check if provided IP address belong to a CIDR range.
 *
 * @param cidr the CIDR to check against
 * @param addr the IP address to check
 * @return true if the CIDR and IP address are valid, and the address
 * is contained in the address-range specified by the CIDR
 */
bool cidr_contains(const cidr_t& cidr, const boost::asio::ip::address& addr);

} /* namespace packets */
} /* namespace opflexagent */

#endif /* OPFLEXAGENT_NETWORK_H */
