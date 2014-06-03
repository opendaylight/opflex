/* Copyright (c) 2014 Cisco Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Copyright (c) 2007-2014 Nicira, Inc.
 *
 * This file is offered under your choice of two licenses: Apache 2.0 or GNU
 * GPL 2.0 or later.  The permission statements for each of these licenses is
 * given below.  You may license your modifications to this file under either
 * of these licenses or both.  If you wish to license your modifications under
 * only one of these licenses, delete the permission text for the other
 * license.
 *
 * ----------------------------------------------------------------------
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * ----------------------------------------------------------------------
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 * ----------------------------------------------------------------------
 */

#ifndef _LINUX_POLICYAGENT_H
#define _LINUX_POLICYAGENT_H 1

#include <linux/types.h>
#include <linux/if_ether.h>

/**
 * struct pag_header - header for PAG Generic Netlink messages.
 * @dp_ifindex: ifindex of local port for datapath (0 to make a request not
 * specific to a datapath).
 *
 * Attributes following the header are specific to a particular PAG Generic
 * Netlink family, but all of the PAG families use this header.
 */

struct pag_header {
	int dp_ifindex;
};

/* Datapaths. */

#define PAG_DATAPATH_FAMILY  "pag_datapath"
#define PAG_DATAPATH_MCGROUP "pag_datapath"

/* V2:
 *   - API users are expected to provide PAG_DP_ATTR_USER_FEATURES
 *     when creating the datapath.
 */
#define PAG_DATAPATH_VERSION 2

/* First PAG datapath version to support features */
#define PAG_DP_VER_FEATURES 2

enum pag_datapath_cmd {
	PAG_DP_CMD_UNSPEC,
	PAG_DP_CMD_NEW,
	PAG_DP_CMD_DEL,
	PAG_DP_CMD_GET,
	PAG_DP_CMD_SET
};

/**
 * enum pag_datapath_attr - attributes for %PAG_DP_* commands.
 * @PAG_DP_ATTR_NAME: Name of the network device that serves as the "local
 * port".  This is the name of the network device whose dp_ifindex is given in
 * the &struct pag_header.  Always present in notifications.  Required in
 * %PAG_DP_NEW requests.  May be used as an alternative to specifying
 * dp_ifindex in other requests (with a dp_ifindex of 0).
 * @PAG_DP_ATTR_UPCALL_PID: The Netlink socket in userspace that is initially
 * set on the datapath port (for PAG_ACTION_ATTR_MISS).  Only valid on
 * %PAG_DP_CMD_NEW requests. A value of zero indicates that upcalls should
 * not be sent.
 * @PAG_DP_ATTR_STATS: Statistics about packets that have passed through the
 * datapath.  Always present in notifications.
 * @PAG_DP_ATTR_MEGAFLOW_STATS: Statistics about mega flow masks usage for the
 * datapath. Always present in notifications.
 *
 * These attributes follow the &struct pag_header within the Generic Netlink
 * payload for %PAG_DP_* commands.
 */
enum pag_datapath_attr {
	PAG_DP_ATTR_UNSPEC,
	PAG_DP_ATTR_NAME,		/* name of dp_ifindex netdev */
	PAG_DP_ATTR_UPCALL_PID,		/* Netlink PID to receive upcalls */
	PAG_DP_ATTR_STATS,		/* struct pag_dp_stats */
	PAG_DP_ATTR_MEGAFLOW_STATS,	/* struct pag_dp_megaflow_stats */
	PAG_DP_ATTR_USER_FEATURES,	/* PAG_DP_F_*  */
	__PAG_DP_ATTR_MAX
};

#define PAG_DP_ATTR_MAX (__PAG_DP_ATTR_MAX - 1)

struct pag_dp_stats {
	__u64 n_hit;             /* Number of flow table matches. */
	__u64 n_missed;          /* Number of flow table misses. */
	__u64 n_lost;            /* Number of misses not sent to userspace. */
	__u64 n_flows;           /* Number of flows present */
};

struct pag_dp_megaflow_stats {
	__u64 n_mask_hit;	 /* Number of masks used for flow lookups. */
	__u32 n_masks;		 /* Number of masks for the datapath. */
	__u32 pad0;		 /* Pad for future expension. */
	__u64 pad1;		 /* Pad for future expension. */
	__u64 pad2;		 /* Pad for future expension. */
};

struct pag_vport_stats {
	__u64   rx_packets;		/* total packets received       */
	__u64   tx_packets;		/* total packets transmitted    */
	__u64   rx_bytes;		/* total bytes received         */
	__u64   tx_bytes;		/* total bytes transmitted      */
	__u64   rx_errors;		/* bad packets received         */
	__u64   tx_errors;		/* packet transmit problems     */
	__u64   rx_dropped;		/* no space in linux buffers    */
	__u64   tx_dropped;		/* no space available in linux  */
};

/* Allow last Netlink attribute to be unaligned */
#define PAG_DP_F_UNALIGNED	(1 << 0)

/* Fixed logical ports. */
#define PAGP_LOCAL      ((__u32)0)

/* Packet transfer. */

#define PAG_PACKET_FAMILY "pag_packet"
#define PAG_PACKET_VERSION 0x1

enum pag_packet_cmd {
	PAG_PACKET_CMD_UNSPEC,

	/* Kernel-to-user notifications. */
	PAG_PACKET_CMD_MISS,    /* Flow table miss. */
	PAG_PACKET_CMD_ACTION,  /* PAG_ACTION_ATTR_USERSPACE action. */

	/* Userspace commands. */
	PAG_PACKET_CMD_EXECUTE  /* Apply actions to a packet. */
};

/**
 * enum pag_packet_attr - attributes for %PAG_PACKET_* commands.
 * @PAG_PACKET_ATTR_PACKET: Present for all notifications.  Contains the entire
 * packet as received, from the start of the Ethernet header onward.  For
 * %PAG_PACKET_CMD_ACTION, %PAG_PACKET_ATTR_PACKET reflects changes made by
 * actions preceding %PAG_ACTION_ATTR_USERSPACE, but %PAG_PACKET_ATTR_KEY is
 * the flow key extracted from the packet as originally received.
 * @PAG_PACKET_ATTR_KEY: Present for all notifications.  Contains the flow key
 * extracted from the packet as nested %PAG_KEY_ATTR_* attributes.  This allows
 * userspace to adapt its flow setup strategy by comparing its notion of the
 * flow key against the kernel's.
 * @PAG_PACKET_ATTR_ACTIONS: Contains actions for the packet.  Used
 * for %PAG_PACKET_CMD_EXECUTE.  It has nested %PAG_ACTION_ATTR_* attributes.
 * @PAG_PACKET_ATTR_USERDATA: Present for an %PAG_PACKET_CMD_ACTION
 * notification if the %PAG_ACTION_ATTR_USERSPACE action specified an
 * %PAG_USERSPACE_ATTR_USERDATA attribute, with the same length and content
 * specified there.
 *
 * These attributes follow the &struct pag_header within the Generic Netlink
 * payload for %PAG_PACKET_* commands.
 */
enum pag_packet_attr {
	PAG_PACKET_ATTR_UNSPEC,
	PAG_PACKET_ATTR_PACKET,      /* Packet data. */
	PAG_PACKET_ATTR_KEY,         /* Nested PAG_KEY_ATTR_* attributes. */
	PAG_PACKET_ATTR_ACTIONS,     /* Nested PAG_ACTION_ATTR_* attributes. */
	PAG_PACKET_ATTR_USERDATA,    /* PAG_ACTION_ATTR_USERSPACE arg. */
	__PAG_PACKET_ATTR_MAX
};

#define PAG_PACKET_ATTR_MAX (__PAG_PACKET_ATTR_MAX - 1)

/* Virtual ports. */

#define PAG_VPORT_FAMILY  "pag_vport"
#define PAG_VPORT_MCGROUP "pag_vport"
#define PAG_VPORT_VERSION 0x1

enum pag_vport_cmd {
	PAG_VPORT_CMD_UNSPEC,
	PAG_VPORT_CMD_NEW,
	PAG_VPORT_CMD_DEL,
	PAG_VPORT_CMD_GET,
	PAG_VPORT_CMD_SET
};

enum pag_vport_type {
	PAG_VPORT_TYPE_UNSPEC,
	PAG_VPORT_TYPE_NETDEV,   /* network device */
	PAG_VPORT_TYPE_INTERNAL, /* network device implemented by datapath */
	PAG_VPORT_TYPE_GRE,	 /* GRE tunnel. */
	PAG_VPORT_TYPE_VXLAN,    /* VXLAN tunnel */
	PAG_VPORT_TYPE_GRE64 = 104, /* GRE tunnel with 64-bit keys */
	PAG_VPORT_TYPE_LISP = 105,  /* LISP tunnel */
	__PAG_VPORT_TYPE_MAX
};

#define PAG_VPORT_TYPE_MAX (__PAG_VPORT_TYPE_MAX - 1)

/**
 * enum pag_vport_attr - attributes for %PAG_VPORT_* commands.
 * @PAG_VPORT_ATTR_PORT_NO: 32-bit port number within datapath.
 * @PAG_VPORT_ATTR_TYPE: 32-bit %PAG_VPORT_TYPE_* constant describing the type
 * of vport.
 * @PAG_VPORT_ATTR_NAME: Name of vport.  For a vport based on a network device
 * this is the name of the network device.  Maximum length %IFNAMSIZ-1 bytes
 * plus a null terminator.
 * @PAG_VPORT_ATTR_OPTIONS: Vport-specific configuration information.
 * @PAG_VPORT_ATTR_UPCALL_PID: The Netlink socket in userspace that
 * PAG_PACKET_CMD_MISS upcalls will be directed to for packets received on
 * this port.  A value of zero indicates that upcalls should not be sent.
 * @PAG_VPORT_ATTR_STATS: A &struct pag_vport_stats giving statistics for
 * packets sent or received through the vport.
 *
 * These attributes follow the &struct pag_header within the Generic Netlink
 * payload for %PAG_VPORT_* commands.
 *
 * For %PAG_VPORT_CMD_NEW requests, the %PAG_VPORT_ATTR_TYPE and
 * %PAG_VPORT_ATTR_NAME attributes are required.  %PAG_VPORT_ATTR_PORT_NO is
 * optional; if not specified a free port number is automatically selected.
 * Whether %PAG_VPORT_ATTR_OPTIONS is required or optional depends on the type
 * of vport.  %PAG_VPORT_ATTR_STATS is optional and other attributes are
 * ignored.
 *
 * For other requests, if %PAG_VPORT_ATTR_NAME is specified then it is used to
 * look up the vport to operate on; otherwise dp_idx from the &struct
 * pag_header plus %PAG_VPORT_ATTR_PORT_NO determine the vport.
 */
enum pag_vport_attr {
	PAG_VPORT_ATTR_UNSPEC,
	PAG_VPORT_ATTR_PORT_NO,	/* u32 port number within datapath */
	PAG_VPORT_ATTR_TYPE,	/* u32 PAG_VPORT_TYPE_* constant. */
	PAG_VPORT_ATTR_NAME,	/* string name, up to IFNAMSIZ bytes long */
	PAG_VPORT_ATTR_OPTIONS, /* nested attributes, varies by vport type */
	PAG_VPORT_ATTR_UPCALL_PID, /* u32 Netlink PID to receive upcalls */
	PAG_VPORT_ATTR_STATS,	/* struct pag_vport_stats */
	__PAG_VPORT_ATTR_MAX
};

#define PAG_VPORT_ATTR_MAX (__PAG_VPORT_ATTR_MAX - 1)

/* PAG_VPORT_ATTR_OPTIONS attributes for tunnels.
 */
enum {
	PAG_TUNNEL_ATTR_UNSPEC,
	PAG_TUNNEL_ATTR_DST_PORT, /* 16-bit UDP port, used by L4 tunnels. */
	__PAG_TUNNEL_ATTR_MAX
};

#define PAG_TUNNEL_ATTR_MAX (__PAG_TUNNEL_ATTR_MAX - 1)

/* Flows. */

#define PAG_FLOW_FAMILY  "pag_flow"
#define PAG_FLOW_MCGROUP "pag_flow"
#define PAG_FLOW_VERSION 0x1

enum pag_flow_cmd {
	PAG_FLOW_CMD_UNSPEC,
	PAG_FLOW_CMD_NEW,
	PAG_FLOW_CMD_DEL,
	PAG_FLOW_CMD_GET,
	PAG_FLOW_CMD_SET
};

struct pag_flow_stats {
	__u64 n_packets;         /* Number of matched packets. */
	__u64 n_bytes;           /* Number of matched bytes. */
};

enum pag_key_attr {
	PAG_KEY_ATTR_UNSPEC,
	PAG_KEY_ATTR_ENCAP,	/* Nested set of encapsulated attributes. */
	PAG_KEY_ATTR_PRIORITY,  /* u32 skb->priority */
	PAG_KEY_ATTR_IN_PORT,   /* u32 PAG dp port number */
	PAG_KEY_ATTR_ETHERNET,  /* struct pag_key_ethernet */
	PAG_KEY_ATTR_VLAN,	/* be16 VLAN TCI */
	PAG_KEY_ATTR_ETHERTYPE,	/* be16 Ethernet type */
	PAG_KEY_ATTR_IPV4,      /* struct pag_key_ipv4 */
	PAG_KEY_ATTR_IPV6,      /* struct pag_key_ipv6 */
	PAG_KEY_ATTR_TCP,       /* struct pag_key_tcp */
	PAG_KEY_ATTR_UDP,       /* struct pag_key_udp */
	PAG_KEY_ATTR_ICMP,      /* struct pag_key_icmp */
	PAG_KEY_ATTR_ICMPV6,    /* struct pag_key_icmpv6 */
	PAG_KEY_ATTR_ARP,       /* struct pag_key_arp */
	PAG_KEY_ATTR_ND,        /* struct pag_key_nd */
	PAG_KEY_ATTR_SKB_MARK,  /* u32 skb mark */
	PAG_KEY_ATTR_TUNNEL,	/* Nested set of pag_tunnel attributes */
	PAG_KEY_ATTR_SCTP,      /* struct pag_key_sctp */
	PAG_KEY_ATTR_TCP_FLAGS,	/* be16 TCP flags. */

#ifdef __KERNEL__
	PAG_KEY_ATTR_IPV4_TUNNEL,  /* struct pag_key_ipv4_tunnel */
#endif

	PAG_KEY_ATTR_MPLS = 62, /* array of struct pag_key_mpls.
				 * The implementation may restrict
				 * the accepted length of the array. */
	__PAG_KEY_ATTR_MAX
};

#define PAG_KEY_ATTR_MAX (__PAG_KEY_ATTR_MAX - 1)

enum pag_tunnel_key_attr {
	PAG_TUNNEL_KEY_ATTR_ID,			/* be64 Tunnel ID */
	PAG_TUNNEL_KEY_ATTR_IPV4_SRC,		/* be32 src IP address. */
	PAG_TUNNEL_KEY_ATTR_IPV4_DST,		/* be32 dst IP address. */
	PAG_TUNNEL_KEY_ATTR_TOS,		/* u8 Tunnel IP ToS. */
	PAG_TUNNEL_KEY_ATTR_TTL,		/* u8 Tunnel IP TTL. */
	PAG_TUNNEL_KEY_ATTR_DONT_FRAGMENT,	/* No argument, set DF. */
	PAG_TUNNEL_KEY_ATTR_CSUM,		/* No argument. CSUM packet. */
	__PAG_TUNNEL_KEY_ATTR_MAX
};

#define PAG_TUNNEL_KEY_ATTR_MAX (__PAG_TUNNEL_KEY_ATTR_MAX - 1)

/**
 * enum pag_frag_type - IPv4 and IPv6 fragment type
 * @PAG_FRAG_TYPE_NONE: Packet is not a fragment.
 * @PAG_FRAG_TYPE_FIRST: Packet is a fragment with offset 0.
 * @PAG_FRAG_TYPE_LATER: Packet is a fragment with nonzero offset.
 *
 * Used as the @ipv4_frag in &struct pag_key_ipv4 and as @ipv6_frag &struct
 * pag_key_ipv6.
 */
enum pag_frag_type {
	PAG_FRAG_TYPE_NONE,
	PAG_FRAG_TYPE_FIRST,
	PAG_FRAG_TYPE_LATER,
	__PAG_FRAG_TYPE_MAX
};

#define PAG_FRAG_TYPE_MAX (__PAG_FRAG_TYPE_MAX - 1)

struct pag_key_ethernet {
	__u8	 eth_src[ETH_ALEN];
	__u8	 eth_dst[ETH_ALEN];
};

struct pag_key_mpls {
	__be32 mpls_lse;
};

struct pag_key_ipv4 {
	__be32 ipv4_src;
	__be32 ipv4_dst;
	__u8   ipv4_proto;
	__u8   ipv4_tos;
	__u8   ipv4_ttl;
	__u8   ipv4_frag;	/* One of PAG_FRAG_TYPE_*. */
};

struct pag_key_ipv6 {
	__be32 ipv6_src[4];
	__be32 ipv6_dst[4];
	__be32 ipv6_label;	/* 20-bits in least-significant bits. */
	__u8   ipv6_proto;
	__u8   ipv6_tclass;
	__u8   ipv6_hlimit;
	__u8   ipv6_frag;	/* One of PAG_FRAG_TYPE_*. */
};

struct pag_key_tcp {
	__be16 tcp_src;
	__be16 tcp_dst;
};

struct pag_key_udp {
	__be16 udp_src;
	__be16 udp_dst;
};

struct pag_key_sctp {
	__be16 sctp_src;
	__be16 sctp_dst;
};

struct pag_key_icmp {
	__u8 icmp_type;
	__u8 icmp_code;
};

struct pag_key_icmpv6 {
	__u8 icmpv6_type;
	__u8 icmpv6_code;
};

struct pag_key_arp {
	__be32 arp_sip;
	__be32 arp_tip;
	__be16 arp_op;
	__u8   arp_sha[ETH_ALEN];
	__u8   arp_tha[ETH_ALEN];
};

struct pag_key_nd {
	__u32 nd_target[4];
	__u8  nd_sll[ETH_ALEN];
	__u8  nd_tll[ETH_ALEN];
};

/**
 * enum pag_flow_attr - attributes for %PAG_FLOW_* commands.
 * @PAG_FLOW_ATTR_KEY: Nested %PAG_KEY_ATTR_* attributes specifying the flow
 * key.  Always present in notifications.  Required for all requests (except
 * dumps).
 * @PAG_FLOW_ATTR_ACTIONS: Nested %PAG_ACTION_ATTR_* attributes specifying
 * the actions to take for packets that match the key.  Always present in
 * notifications.  Required for %PAG_FLOW_CMD_NEW requests, optional for
 * %PAG_FLOW_CMD_SET requests.
 * @PAG_FLOW_ATTR_STATS: &struct pag_flow_stats giving statistics for this
 * flow.  Present in notifications if the stats would be nonzero.  Ignored in
 * requests.
 * @PAG_FLOW_ATTR_TCP_FLAGS: An 8-bit value giving the OR'd value of all of the
 * TCP flags seen on packets in this flow.  Only present in notifications for
 * TCP flows, and only if it would be nonzero.  Ignored in requests.
 * @PAG_FLOW_ATTR_USED: A 64-bit integer giving the time, in milliseconds on
 * the system monotonic clock, at which a packet was last processed for this
 * flow.  Only present in notifications if a packet has been processed for this
 * flow.  Ignored in requests.
 * @PAG_FLOW_ATTR_CLEAR: If present in a %PAG_FLOW_CMD_SET request, clears the
 * last-used time, accumulated TCP flags, and statistics for this flow.
 * Otherwise ignored in requests.  Never present in notifications.
 * @PAG_FLOW_ATTR_MASK: Nested %PAG_KEY_ATTR_* attributes specifying the
 * mask bits for wildcarded flow match. Mask bit value '1' specifies exact
 * match with corresponding flow key bit, while mask bit value '0' specifies
 * a wildcarded match. Omitting attribute is treated as wildcarding all
 * corresponding fields. Optional for all requests. If not present,
 * all flow key bits are exact match bits.
 *
 * These attributes follow the &struct pag_header within the Generic Netlink
 * payload for %PAG_FLOW_* commands.
 */
enum pag_flow_attr {
	PAG_FLOW_ATTR_UNSPEC,
	PAG_FLOW_ATTR_KEY,       /* Sequence of PAG_KEY_ATTR_* attributes. */
	PAG_FLOW_ATTR_ACTIONS,   /* Nested PAG_ACTION_ATTR_* attributes. */
	PAG_FLOW_ATTR_STATS,     /* struct pag_flow_stats. */
	PAG_FLOW_ATTR_TCP_FLAGS, /* 8-bit OR'd TCP flags. */
	PAG_FLOW_ATTR_USED,      /* u64 msecs last used in monotonic time. */
	PAG_FLOW_ATTR_CLEAR,     /* Flag to clear stats, tcp_flags, used. */
	PAG_FLOW_ATTR_MASK,      /* Sequence of PAG_KEY_ATTR_* attributes. */
	__PAG_FLOW_ATTR_MAX
};

#define PAG_FLOW_ATTR_MAX (__PAG_FLOW_ATTR_MAX - 1)

/**
 * enum pag_sample_attr - Attributes for %PAG_ACTION_ATTR_SAMPLE action.
 * @PAG_SAMPLE_ATTR_PROBABILITY: 32-bit fraction of packets to sample with
 * @PAG_ACTION_ATTR_SAMPLE.  A value of 0 samples no packets, a value of
 * %UINT32_MAX samples all packets and intermediate values sample intermediate
 * fractions of packets.
 * @PAG_SAMPLE_ATTR_ACTIONS: Set of actions to execute in sampling event.
 * Actions are passed as nested attributes.
 *
 * Executes the specified actions with the given probability on a per-packet
 * basis.
 */
enum pag_sample_attr {
	PAG_SAMPLE_ATTR_UNSPEC,
	PAG_SAMPLE_ATTR_PROBABILITY, /* u32 number */
	PAG_SAMPLE_ATTR_ACTIONS,     /* Nested PAG_ACTION_ATTR_* attributes. */
	__PAG_SAMPLE_ATTR_MAX,
};

#define PAG_SAMPLE_ATTR_MAX (__PAG_SAMPLE_ATTR_MAX - 1)

/**
 * enum pag_userspace_attr - Attributes for %PAG_ACTION_ATTR_USERSPACE action.
 * @PAG_USERSPACE_ATTR_PID: u32 Netlink PID to which the %PAG_PACKET_CMD_ACTION
 * message should be sent.  Required.
 * @PAG_USERSPACE_ATTR_USERDATA: If present, its variable-length argument is
 * copied to the %PAG_PACKET_CMD_ACTION message as %PAG_PACKET_ATTR_USERDATA.
 */
enum pag_userspace_attr {
	PAG_USERSPACE_ATTR_UNSPEC,
	PAG_USERSPACE_ATTR_PID,	      /* u32 Netlink PID to receive upcalls. */
	PAG_USERSPACE_ATTR_USERDATA,  /* Optional user-specified cookie. */
	__PAG_USERSPACE_ATTR_MAX
};

#define PAG_USERSPACE_ATTR_MAX (__PAG_USERSPACE_ATTR_MAX - 1)

/**
 * struct pag_action_push_mpls - %PAG_ACTION_ATTR_PUSH_MPLS action argument.
 * @mpls_lse: MPLS label stack entry to push.
 * @mpls_ethertype: Ethertype to set in the encapsulating ethernet frame.
 *
 * The only values @mpls_ethertype should ever be given are %ETH_P_MPLS_UC and
 * %ETH_P_MPLS_MC, indicating MPLS unicast or multicast. Other are rejected.
 */
struct pag_action_push_mpls {
	__be32 mpls_lse;
	__be16 mpls_ethertype; /* Either %ETH_P_MPLS_UC or %ETH_P_MPLS_MC */
};

/**
 * struct pag_action_push_vlan - %PAG_ACTION_ATTR_PUSH_VLAN action argument.
 * @vlan_tpid: Tag protocol identifier (TPID) to push.
 * @vlan_tci: Tag control identifier (TCI) to push.  The CFI bit must be set
 * (but it will not be set in the 802.1Q header that is pushed).
 *
 * The @vlan_tpid value is typically %ETH_P_8021Q.  The only acceptable TPID
 * values are those that the kernel module also parses as 802.1Q headers, to
 * prevent %PAG_ACTION_ATTR_PUSH_VLAN followed by %PAG_ACTION_ATTR_POP_VLAN
 * from having surprising results.
 */
struct pag_action_push_vlan {
	__be16 vlan_tpid;	/* 802.1Q TPID. */
	__be16 vlan_tci;	/* 802.1Q TCI (VLAN ID and priority). */
};

/**
 * enum pag_action_attr - Action types.
 *
 * @PAG_ACTION_ATTR_OUTPUT: Output packet to port.
 * @PAG_ACTION_ATTR_USERSPACE: Send packet to userspace according to nested
 * %PAG_USERSPACE_ATTR_* attributes.
 * @PAG_ACTION_ATTR_PUSH_VLAN: Push a new outermost 802.1Q header onto the
 * packet.
 * @PAG_ACTION_ATTR_POP_VLAN: Pop the outermost 802.1Q header off the packet.
 * @PAG_ACTION_ATTR_SAMPLE: Probabilitically executes actions, as specified in
 * the nested %PAG_SAMPLE_ATTR_* attributes.
 * @PAG_ACTION_ATTR_SET: Replaces the contents of an existing header.  The
 * single nested %PAG_KEY_ATTR_* attribute specifies a header to modify and its
 * value.
 * @PAG_ACTION_ATTR_PUSH_MPLS: Push a new MPLS label stack entry onto the
 * top of the packets MPLS label stack.  Set the ethertype of the
 * encapsulating frame to either %ETH_P_MPLS_UC or %ETH_P_MPLS_MC to
 * indicate the new packet contents.
 * @PAG_ACTION_ATTR_POP_MPLS: Pop an MPLS label stack entry off of the
 * packet's MPLS label stack.  Set the encapsulating frame's ethertype to
 * indicate the new packet contents. This could potentially still be
 * %ETH_P_MPLS if the resulting MPLS label stack is not empty.  If there
 * is no MPLS label stack, as determined by ethertype, no action is taken.
 *
 * Only a single header can be set with a single %PAG_ACTION_ATTR_SET.  Not all
 * fields within a header are modifiable, e.g. the IPv4 protocol and fragment
 * type may not be changed.
 */

enum pag_action_attr {
	PAG_ACTION_ATTR_UNSPEC,
	PAG_ACTION_ATTR_OUTPUT,	      /* u32 port number. */
	PAG_ACTION_ATTR_USERSPACE,    /* Nested PAG_USERSPACE_ATTR_*. */
	PAG_ACTION_ATTR_SET,          /* One nested PAG_KEY_ATTR_*. */
	PAG_ACTION_ATTR_PUSH_VLAN,    /* struct pag_action_push_vlan. */
	PAG_ACTION_ATTR_POP_VLAN,     /* No argument. */
	PAG_ACTION_ATTR_SAMPLE,       /* Nested PAG_SAMPLE_ATTR_*. */
	PAG_ACTION_ATTR_PUSH_MPLS,    /* struct pag_action_push_mpls. */
	PAG_ACTION_ATTR_POP_MPLS,     /* __be16 ethertype. */
	__PAG_ACTION_ATTR_MAX
};

#define PAG_ACTION_ATTR_MAX (__PAG_ACTION_ATTR_MAX - 1)

#endif /* _LINUX_OPENVSWITCH_H */
