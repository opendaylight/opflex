/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Headers for the ethernet protocol
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_ETH_H
#define OPFLEXAGENT_ETH_H

namespace opflexagent {
namespace eth {

/**
 * Length of an ethernet address
 */
const uint8_t ADDR_LEN = 6;

/**
 * The ethernet header
 */
struct eth_header {
    /**
     * Destination address
     */
    uint8_t eth_dst[ADDR_LEN];
    /**
     * Source address
     */
    uint8_t eth_src[ADDR_LEN];
    /**
     * Type of packet
     */
    uint16_t eth_type;
};

namespace type {
/**
 * IP ether type
 */
const uint16_t IP = 0x0800;

/**
 * ARP ether type
 */
const uint16_t ARP = 0x0806;

/**
 * RARP ether type
 */
const uint16_t RARP = 0x8035;

/**
 * IPv6 ether type
 */
const uint16_t IPV6 = 0x86dd;

} /* namespace type */

} /* namespace eth */
} /* namespace opflexagent */

#endif
