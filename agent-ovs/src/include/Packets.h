/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Utility functions for packet generation/manipulation
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OVSAGENT_PACKETS_H
#define OVSAGENT_PACKETS_H

#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/ip.h>

#include <boost/asio/ip/address.hpp>

#include <opflex/modb/MAC.h>

#include "ovs.h"
#include "PolicyManager.h"
#include "Endpoint.h"

namespace ovsagent {
namespace packets {

/**
 * The ethernet broadcast MAC
 */
extern const uint8_t MAC_ADDR_BROADCAST[6];

/**
 * Mask for ethernet multicast MACs
 */
extern const uint8_t MAC_ADDR_MULTICAST[6];

/**
 * MAC address for IPv6 multicast packets
 */
extern const uint8_t MAC_ADDR_IPV6MULTICAST[6];

/**
 * Zero MAC address
 */
extern const uint8_t MAC_ADDR_ZERO[6];

/**
 * Compute an internet checksum over the specified data.  chksum
 * should be first initialized to zero, then chksum_accum called for
 * each block of data, and finally call chksum_finalize to get the
 * result
 *
 * @param chksum the checksum to accumulate
 * @param addr the data
 * @param len the length of the data
 */
void chksum_accum(uint32_t& chksum, uint16_t* addr, size_t len);

/**
 * Finalize the computation of a checksum.  Does not change the
 * intermediate state, so can be used to compute a partial
 * checksum.
 *
 * @param chksum the value of the sum before finalization
 * @return The final checksum value.
 */
uint16_t chksum_finalize(uint32_t chksum);

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
 * Compose an ICMP6 neighbor advertisement ethernet frame
 *
 * @param naFlags the flags to set in the NA
 * @param srcMac the source MAC
 * @param dstMac the target MAC
 * @param srcIp the source IP
 * @param dstIp the destination Ip
 * @return a ofpbuf containing the message
 */
ofpbuf* compose_icmp6_neigh_ad(uint32_t naFlags,
                               const uint8_t* srcMac,
                               const uint8_t* dstMac,
                               const struct in6_addr* srcIp,
                               const struct in6_addr* dstIp);

/**
 * Compose an ICMP6 neighbor solicitation ethernet frame
 *
 * @param srcMac the source MAC
 * @param dstMac the target MAC
 * @param srcIp the source IP
 * @param dstIp the destination Ip
 * @return a ofpbuf containing the message
 */
ofpbuf* compose_icmp6_neigh_solit(const uint8_t* srcMac,
                                  const uint8_t* dstMac,
                                  const struct in6_addr* srcIp,
                                  const struct in6_addr* dstIp);

/**
 * Compose an ICMP6 router advertisement ethernet frame
 *
 * @param srcMac the source MAC
 * @param dstMac the dst MAC
 * @param dstIp the destination Ip
 * @param egUri the endpoint group associated with the request
 * @param polMgr the policy manager
 * @return a ofpbuf containing the message
 */
ofpbuf* compose_icmp6_router_ad(const uint8_t* srcMac,
                                const uint8_t* dstMac,
                                const struct in6_addr* dstIp,
                                const opflex::modb::URI& egUri,
                                PolicyManager& polMgr);

/**
 * A convenience typedef for static routes
 */
typedef Endpoint::DHCPv4Config::static_route_t static_route_t;

/**
 * Compose a DHCPv4 offer, ACK, or NACK
 *
 * @param message_type the message type of the reply
 * @param xid the transaction ID for the message
 * @param srcMac the MAC address for the DHCP server
 * @param clientMac the MAC address for the requesting client
 * @param clientIp the IP address to return to the client
 * @param prefixLen the length of the prefix for use in the subnet
 * mask
 * @param serverIp the apparent IP address to use when sending the
 * reply.  Defaults to a link-local IP 169.254.32.32
 * @param routers the list of routers to return to the client
 * @param dnsServers the list of DNS servers to return to the client
 * @param domain The domain to return to the client
 * @param staticRoutes classless static routes to return to the client
 * @param interfaceMtu value of interface MTU to return to the client
 */
ofpbuf* compose_dhcpv4_reply(uint8_t message_type,
                             uint32_t xid,
                             const uint8_t* srcMac,
                             const uint8_t* clientMac,
                             uint32_t clientIp,
                             uint8_t prefixLen,
                             const boost::optional<std::string>& serverIp,
                             const std::vector<std::string>& routers,
                             const std::vector<std::string>& dnsServers,
                             const boost::optional<std::string>& domain,
                             const std::vector<static_route_t>& staticRoutes,
                             const boost::optional<uint16_t>& interfaceMtu);

/**
 * Compose a DHCPv6 Advertise or Reply message
 *
 * @param message_type the message type to send
 * @param xid the transaction ID for the message
 * @param srcMac the MAC address for the DHCP server
 * @param clientMac the MAC address for the requesting client
 * @param dstIp the destination IP for the reply
 * @param client_id the client ID to include in the reply
 * @param client_id_len the length of the client ID
 * @param iaid the IA ID for the identity association
 * @param ips the list of IP addresses to return to the client
 * @param dnsServers the list of DNS servers to return to the client
 * @param searchList the DNS search path to return to the client
 * @param temporary true if the client requested a temporary address
 * @param rapid true if this is a rapid commit reply
 */
ofpbuf* compose_dhcpv6_reply(uint8_t message_type,
                             const uint8_t* xid,
                             const uint8_t* srcMac,
                             const uint8_t* clientMac,
                             const struct in6_addr* dstIp,
                             uint8_t* client_id,
                             uint16_t client_id_len,
                             uint8_t* iaid,
                             const std::vector<boost::asio::ip::address_v6>& ips,
                             const std::vector<std::string>& dnsServers,
                             const std::vector<std::string>& searchList,
                             bool temporary,
                             bool rapid);

/**
 * Compose an ARP packet
 *
 * @param op the opcode
 * @param srcMac the source MAC for the ethernet header
 * @param dstMac the destination MAC for the ethernet header
 * @param sha the source hardware address
 * @param tha the target hardware address
 * @param spa the source protocol address
 * @param tpa the target protocol address
 */
ofpbuf* compose_arp(uint16_t op,
                    const uint8_t* srcMac,
                    const uint8_t* dstMac,
                    const uint8_t* sha,
                    const uint8_t* tha,
                    uint32_t spa,
                    uint32_t tpa);

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
} /* namespace ovsagent */

#endif /* OVSAGENT_PACKETS_H */
