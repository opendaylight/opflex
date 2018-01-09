/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Headers for the ARP protocol
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_ARP_H
#define OPFLEXAGENT_ARP_H

namespace opflexagent {
namespace arp {

/**
 * The ARP header
 */
struct arp_hdr {
    /**
     * Hardware type
     */
    uint16_t htype;
    /**
     * Protocol type
     */
    uint16_t ptype;
    /**
     * Hardware address length
     */
    uint8_t hlen;
    /**
     * Protocol address length
     */
    uint8_t plen;
    /**
     * Operation
     */
    uint16_t op;
};

namespace op {

/**
 * An ARP request
 */
const uint16_t REQUEST = 1;

/**
 * An ARP reply
 */
const uint16_t REPLY = 2;

/**
 * Reverse request
 */
const uint16_t REVERSE_REQUEST = 3;

/**
 * Reverse reply
 */
const uint16_t REVERSE_REPLY = 4;

} /* namespace op */
} /* namespace arp */
} /* namespace opflexagent */

#endif /* OPFLEXAGENT_ARP_H */
