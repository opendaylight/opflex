/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for endpoint manager
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

#include "ovs.h"
#include "PolicyManager.h"

namespace ovsagent {

/**
 * Utility functions for packet manipulation
 */
class Packets {
public:
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
    static void chksum_accum(uint32_t& chksum, uint16_t* addr, size_t len);
    
    /**
     * Finalize the computation of a checksum.  Does not change the
     * intermediate state, so can be used to compute a partial
     * checksum.
     *
     * @param chksum the value of the sum before finalization
     * @return The final checksum value.
     */
    static uint16_t chksum_finalize(uint32_t chksum);

    /**
     * For a subnet with prefix length 64, construct an IP address
     * using the EUI-64 format in the lower 64 bits.
     * @param prefix the prefix address
     * @param srcMac the MAC address of the interface
     * @param dstAddr the in6_addr struct in which the result will be stored
     */
    static void construct_auto_ip(boost::asio::ip::address_v6 prefix,
                                  const uint8_t* srcMac,
                                  /* out */ struct in6_addr* dstAddr);

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
    static ofpbuf* compose_icmp6_neigh_ad(uint32_t naFlags,
                                          const uint8_t* srcMac,
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
    static ofpbuf* compose_icmp6_router_ad(const uint8_t* srcMac,
                                           const uint8_t* dstMac,
                                           const struct in6_addr* dstIp,
                                           const opflex::modb::URI& egUri,
                                           PolicyManager& polMgr);

};

}

#endif /* OVSAGENT_PACKETS_H */
