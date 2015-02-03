/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Headers for the DHCP protocol
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OVSAGENT_DHCP_H
#define OVSAGENT_DHCP_H

namespace ovsagent {
namespace dhcp {

/**
 * The DHCPv4 header
 */
struct dhcp_hdr {
    /** Message op code / message type */
    uint8_t op;
    /** Hardware address type */
    uint8_t htype;
    /** Hardware address length */
    uint8_t hlen;
    /**
     * Client sets to zero, optionally used by relay agents when
     * booting via a relay agent.
     */
    uint8_t hops;
    /** Transaction ID */
    uint32_t xid;
    /**
     * Filled in by client, seconds elapsed since client began address
     * acquisition or renewal process.
     */
    uint16_t secs;
    /** Flags */
    uint16_t flags;
    /** Client IP address (renew) */
    uint32_t ciaddr;
    /** 'your' (client) IP address. */
    uint32_t yiaddr;
    /** IP address of next server to use in bootstrap */
    uint32_t siaddr;
    /** Relay agent IP address */
    uint32_t giaddr;
    /** Client hardware address */
    uint8_t chaddr[16];
    /** Optional server host name */
    char sname[64];
    /** Boot file name */
    char file[128];
    /** magic cookie: 99, 130, 83 and 99 */
    uint8_t cookie[4];
};

#define DHCP_HTYPE_ETHERNET 1

#define DHCP_MESSAGE_TYPE_DISCOVER 1
#define DHCP_MESSAGE_TYPE_OFFER 2
#define DHCP_MESSAGE_TYPE_REQUEST 3
#define DHCP_MESSAGE_TYPE_DECLINE 4
#define DHCP_MESSAGE_TYPE_ACK 5
#define DHCP_MESSAGE_TYPE_NAK 6
#define DHCP_MESSAGE_TYPE_RELEASE 7

#define DHCP_OPTION_PAD 0
#define DHCP_OPTION_END 255
#define DHCP_OPTION_SUBNET_MASK 1
#define DHCP_OPTION_ROUTER 3
#define DHCP_OPTION_DNS 6
#define DHCP_OPTION_DOMAIN_NAME 15
#define DHCP_OPTION_REQUESTED_IP 50
#define DHCP_OPTION_LEASE_TIME 51
#define DHCP_OPTION_MESSAGE_TYPE 53
#define DHCP_OPTION_SERVER_IDENTIFIER 54
#define DHCP_OPTION_PARAM_REQ_LIST 55
#define DHCP_OPTION_MESSAGE 56
#define DHCP_OPTION_CLASSLESS_STATIC_ROUTE 121

/**
 * A DHCP option header used for PAD and END.  Note that the padding
 * in this struct means you shouldn't call sizeof
 */
struct dhcp_option_hdr_base {
    /** Option type code */
    uint8_t code;
};

/**
 * A DHCP option header used for all options except PAD and END.  Note
 * that the padding in this struct means you shouldn't call sizeof
 */
struct dhcp_option_hdr {
    /** Option type code */
    uint8_t code;
    /** Length of option not including header */
    uint8_t len;
};

#define DHCP_OPTION_IP_LEN 4
#define DHCP_OPTION_LEASE_TIME_LEN 4
#define DHCP_OPTION_MESSAGE_TYPE_LEN 1

} /* namespace dhcp */
} /* namespace ovsagent */

#endif /* OVSAGENT_DHCP_H */
