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

#ifndef OPFLEXAGENT_DHCP_H
#define OPFLEXAGENT_DHCP_H

namespace opflexagent {
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

// ******************
// DHCP Message types
// ******************

namespace message_type {
/**
 * DHCP discover message type
 */
const uint8_t DISCOVER = 1;
/**
 * DHCP offer message type
 */
const uint8_t OFFER = 2;
/**
 * DHCP request message type
 */
const uint8_t REQUEST = 3;
/**
 * DHCP decline message type
 */
const uint8_t DECLINE = 4;
/**
 * DHCP ack message type
 */
const uint8_t ACK = 5;
/**
 * DHCP nak message type
 */
const uint8_t NAK = 6;
/**
 * DHCP release message type
 */
const uint8_t RELEASE = 7;

} /* namespace message_type */

// ************
// DHCP Options
// ************

namespace option {

/**
 * DHCP pad option (single byte)
 */
const uint8_t PAD = 0;
/**
 * DHCP end option signals end of packet (single byte)
 */
const uint8_t END = 255;
/**
 * Subnet mask option (code, len (4), 4 byte mask)
 */
const uint8_t SUBNET_MASK = 1;
/**
 * Router option (code, len (variable), list of 4 byte router IPs)
 */
const uint8_t ROUTER = 3;
/**
 * DNS server option (code, len (4), list of 4 byte DNS IPs)
 */
const uint8_t DNS = 6;
/**
 * Domain name option (code, len (variable), unterminated domain
 * string)
 */
const uint8_t DOMAIN_NAME = 15;
/**
 * Interface MTU option (code, len (2), 16-bit unsigned interger)
 */
const uint8_t INTERFACE_MTU = 26;
/**
 * Requested IP option (code, len (4), 4 byte IP)
 */
const uint8_t REQUESTED_IP = 50;
/**
 * Lease time option (code, len (4), 4 time in seconds)
 */
const uint8_t LEASE_TIME = 51;
/**
 * Message type option (code, len (1), 1 byte message type)
 */
const uint8_t MESSAGE_TYPE = 53;
/**
 * Server identifier option (code, len (4), 4 byte IP)
 */
const uint8_t SERVER_IDENTIFIER = 54;
/**
 * Server identifier option (code, len (variable), list of one byte
 * option codes)
 */
const uint8_t PARAM_REQ_LIST = 55;
/**
 * Server identifier option (code, len (variable), unterminated
 * message string)
 */
const uint8_t MESSAGE = 56;
/**
 * Server identifier option (code, len (variable), list of (prefixlen
 * (1 byte), significant network address bytes (ceil(prefixlen/8)
 * bytes), next hop IP (4 bytes))
 */
const uint8_t CLASSLESS_STATIC_ROUTE = 121;

/**
 * Length of options containing only an IP address
 */
const uint8_t IP_LEN = 4;
/**
 * Length of lease type options
 */
const uint8_t LEASE_TIME_LEN = 4;
/**
 * Length of message type option
 */
const uint8_t MESSAGE_TYPE_LEN = 1;

} /* namespace option */
} /* namespace dhcp */

namespace dhcp6 {

/**
 * DHCPv6 header used for client/server messages
 */
struct dhcp6_hdr {
    /**
     * Identifies the DHCP message type
     */
    uint8_t msg_type;

    /**
     * The transaction ID for this message exchange.
     */
    uint8_t transaction_id[3];
};

/**
 * Header for DHCPv6 options
 */
struct dhcp6_opt_hdr {
    /**
     * An unsigned integer identifying the specific option type
     * carried in this option.
     */
    uint16_t option_code;

    /**
     * An unsigned integer giving the length of the option-data field
     * in this option in octets.
     */
    uint16_t option_len;
};

// ********************
// DHCPv6 Message Types
// ********************

namespace message_type {

/**
 * DHCPv6 Solicit message type.  A client sends a Solicit message to
 * locate servers.
 */
const uint8_t SOLICIT = 1;

/**
 * DHCPv6 Advertise message type.  A server sends an Advertise message
 * to indicate that it is available for DHCP service, in response to a
 * Solicit message received from a client.
 */
const uint8_t ADVERTISE =  2;

/**
 * DHCPv6 Request message type.  A client sends a Request message to
 * request configuration parameters, including IP addresses, from a
 * specific server.
 */
const uint8_t REQUEST =  3;

/**
 * DHCPv6 Confirm message type.  A client sends a Confirm message to
 * any available server to determine whether the addresses it was
 * assigned are still appropriate to the link to which the client is
 * connected.
 */
const uint8_t CONFIRM =  4;

/**
 * DHCPv6 Renew message type.  A client sends a Renew message to the
 * server that originally provided the client's addresses and
 * configuration parameters to extend the lifetimes on the addresses
 * assigned to the client and to update other configuration
 * parameters.
 */
const uint8_t RENEW =  5;

/**
 * DHCPv6 Rebind message type.  A client sends a Rebind message to any
 * available server to extend the lifetimes on the addresses assigned
 * to the client and to update other configuration parameters; this
 * message is sent after a client receives no response to a Renew
 * message.
 */
const uint8_t REBIND =  6;

/**
 * DHCPv6 Reply message type.  A server sends a Reply message
 * containing assigned addresses and configuration parameters in
 * response to a Solicit, Request, Renew, Rebind message received from
 * a client.  A server sends a Reply message containing configuration
 * parameters in response to an Information-request message.  A server
 * sends a Reply message in response to a Confirm message confirming
 * or denying that the addresses assigned to the client are
 * appropriate to the link to which the client is connected.  A server
 * sends a Reply message to acknowledge receipt of a Release or
 * Decline message.
 */
const uint8_t REPLY =  7;

/**
 * DHCPv6 Release message type.  A client sends a Release message to
 * the server that assigned addresses to the client to indicate that
 * the client will no longer use one or more of the assigned
 * addresses.
 */
const uint8_t RELEASE =  8;

/**
 * DHCPv6 Decline message type.  A client sends a Decline message to a
 * server to indicate that the client has determined that one or more
 * addresses assigned by the server are already in use on the link to
 * which the client is connected.
 */
const uint8_t DECLINE =  9;

/**
 * DHCPv6 Reconfigure message type.  A server sends a Reconfigure
 * message to a client to inform the client that the server has new or
 * updated configuration parameters, and that the client is to
 * initiate a Renew/Reply or Information-request/Reply transaction
 * with the server in order to receive the updated information.
 */
const uint8_t RECONFIGURE =  10;

/**
 * DHCPv6 Information-request message type.  A client sends an
 * Information-request message to a server to request configuration
 * parameters without the assignment of any IP addresses to the
 * client.
 */
const uint8_t INFORMATION_REQUEST =  11;

/**
 * DHCPv6 Relay-forward message type.  A relay agent sends a
 * Relay-forward message to relay messages to servers, either directly
 * or through another relay agent.  The received message, either a
 * client message or a Relay-forward message from another relay agent,
 * is encapsulated in an option in the Relay-forward message.
 */
const uint8_t RELAY_FORW =  12;

/**
 * DHCPv6 Relay-reply message type.  A server sends a Relay-reply
 * message to a relay agent containing a message that the relay agent
 * delivers to a client.  The Relay-reply message may be relayed by
 * other relay agents for delivery to the destination relay agent.
 *
 * The server encapsulates the client message as an option in the
 * Relay-reply message, which the relay agent extracts and relays to
 * the client.
 */
const uint8_t RELAY_REPL =  13;

} /* namespace message_type */

// **************
// DHCPv6 Options
// **************

namespace option {

/**
 * Client identifier option code.  Used to carry a DUID identifying a
 * client between a client and a server.
 */
const uint16_t CLIENT_IDENTIFIER = 1;
/**
 * Servier identifier option code.  Used to carry a DUID (see section
 * 9) identifying a server between a client and a server.
 */
const uint16_t SERVER_IDENTIFIER = 2;
/**
 * Identity association for non-temporary addresses option code.  Used
 * to carry an IA_NA, the parameters associated with the IA_NA, and
 * the non-temporary addresses associated with the IA_NA.
 */
const uint16_t IA_NA = 3;
/**
 * Identity Association for the Temporary Addresses option code.  Used
 * to carry an IA_TA, the parameters associated with the IA_TA and the
 * addresses associated with the IA_TA.
 */
const uint16_t IA_TA = 4;
/**
 * IA Address option code.  Used to specify IPv6 addresses associated
 * with an IA_NA or an IA_TA.  The IA Address option must be
 * encapsulated in the Options field of an IA_NA or IA_TA option.
 */
const uint16_t IAADDR = 5;
/**
 * The Option Request option code.  Used to identify a list of options
 * in a message between a client and a server.
 */
const uint16_t ORO = 6;
/**
 * The Authentication option code.  Carries authentication information
 * to authenticate the identity and contents of DHCP messages.
 */
const uint16_t AUTH = 11;
/**
 * Status code option code.  Returns a status indication related to
 * the DHCP message or option in which it appears.
 */
const uint16_t STATUS_CODE = 13;
/**
 * Rapid Commit option code.  Used to signal the use of the two
 * message exchange for address assignment.
 */
const uint16_t RAPID_COMMIT = 14;
/**
 * Reconfigure Accept option code.  A client uses the Reconfigure
 * Accept option to announce to the server whether the client is
 * willing to accept Reconfigure messages, and a server uses this
 * option to tell the client whether or not to accept Reconfigure
 * messages.
 */
const uint16_t RECONF_ACCEPT = 20;
/**
 * DNS Recursive Name Server option code.  Provides a list of one or
 * more IPv6 addresses of DNS recursive name servers to which a
 * client's DNS resolver MAY send DNS queries
 */
const uint16_t DNS_SERVERS = 23;
/**
 * Domain Search List option code.  Specifies the domain search list
 * the client is to use when resolving hostnames with DNS.
 */
const uint16_t DOMAIN_LIST = 24;

} /* namespace option */
} /* namespace dhcp6 */
} /* namespace opflexagent */

#endif /* OPFLEXAGENT_DHCP_H */
