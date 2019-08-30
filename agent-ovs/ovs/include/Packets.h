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

#ifndef OPFLEXAGENT_PACKETS_H
#define OPFLEXAGENT_PACKETS_H

#include <cstdint>
#include <arpa/inet.h>

#include <boost/asio/ip/address.hpp>

#include <opflexagent/PolicyManager.h>
#include <opflexagent/Endpoint.h>

class OfpBuf;

namespace opflexagent {
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
 * Mac addresses assigned for standards use that should be filtered by
 * a bridge
 */
extern const uint8_t MAC_ADDR_FILTERED[6];

/**
 * Mask for MAC_ADDR_FILTERED
 */
extern const uint8_t MAC_ADDR_FILTERED_MASK[6];

/**
 * Link-local ipv4 address used for optimized DHCP replies when not
 * otherwise specified by the user.
 *
 * 169.254.32.32
 */
extern const boost::asio::ip::address_v4 LINK_LOCAL_DHCP;

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
 * Compose an ICMP6 neighbor advertisement ethernet frame
 *
 * @param naFlags the flags to set in the NA
 * @param srcMac the source MAC
 * @param dstMac the target MAC
 * @param srcIp the source IP
 * @param dstIp the destination Ip
 * @return a ofpbuf containing the message
 */
OfpBuf compose_icmp6_neigh_ad(uint32_t naFlags,
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
 * @param dstIp the destination IP.  Should be the solicited node IP
 * address for the target IP
 * @param targetIp The target IP for the solicitation
 * @return a ofpbuf containing the message
 */
OfpBuf compose_icmp6_neigh_solit(const uint8_t* srcMac,
                                 const uint8_t* dstMac,
                                 const struct in6_addr* srcIp,
                                 const struct in6_addr* dstIp,
                                 const struct in6_addr* targetIp);

/**
 * Compose an ICMP6 router advertisement ethernet frame
 *
 * @param srcMac the source MAC
 * @param dstMac the dst MAC
 * @param dstIp the destination Ip
 * @param egUri the endpoint group associated with the request
 * @param polMgr the policy manager
 * @return a ofpbuf containing the message.  Can be NULL
 */
OfpBuf compose_icmp6_router_ad(const uint8_t* srcMac,
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
 * @param leaseTime the lease time to send to the client
 */
OfpBuf compose_dhcpv4_reply(uint8_t message_type,
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
                            const boost::optional<uint16_t>& interfaceMtu,
                            const boost::optional<uint32_t>& leaseTime);

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
 * @param t1 the time before client should contact dhcp server to renew
 * @param t2 the time before client should broadcast renewal attempt
 * @param preferredLifetime The preferred lifetime for the IPv6
 * addresses
 * @param validLifetime the valid lifetime for the IPv6 addresses
 */
OfpBuf compose_dhcpv6_reply(uint8_t message_type,
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
                            bool rapid,
                            const boost::optional<uint32_t>& t1,
                            const boost::optional<uint32_t>& t2,
                            const boost::optional<uint32_t>& preferredLifetime,
                            const boost::optional<uint32_t>& validLifetime);

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
 * @param rarp use RARP ethertype instead
 */
OfpBuf compose_arp(uint16_t op,
                   const uint8_t* srcMac,
                   const uint8_t* dstMac,
                   const uint8_t* sha,
                   const uint8_t* tha,
                   uint32_t spa,
                   uint32_t tpa,
                   bool rarp = false);

} /* namespace packets */
} /* namespace opflexagent */

#endif /* OPFLEXAGENT_PACKETS_H */
