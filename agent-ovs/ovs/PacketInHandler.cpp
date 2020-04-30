/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for PacketInHandler
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <netinet/ip.h>
#include <linux/icmp.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include <boost/system/error_code.hpp>

#include "PacketInHandler.h"
#include "Packets.h"
#include "ActionBuilder.h"
#include <opflexagent/logging.h>
#include "dhcp.h"
#include "udp.h"
#include "eth.h"
#include "IntFlowManager.h"
#include "FlowConstants.h"
#include "ovs-shim.h"
#include "ovs-ofputil.h"

// OVS lib
#include <lib/util.h>
#include <openvswitch/ofp-msgs.h>
#include <openvswitch/match.h>

using std::string;
using std::vector;
using std::unordered_map;
using std::unordered_set;
using std::shared_ptr;
using std::placeholders::_1;
using std::placeholders::_2;
using boost::asio::ip::address;
using boost::asio::ip::address_v6;
using boost::asio::ip::address_v4;
using boost::optional;
using opflex::modb::MAC;
using opflex::modb::URI;
using modelgbp::gbp::Subnet;

namespace opflexagent {

PacketInHandler::PacketInHandler(Agent& agent_,
                                 IntFlowManager& intFlowManager_)
    : agent(agent_), intFlowManager(intFlowManager_),
      intPortMapper(NULL), accessPortMapper(NULL),
      intFlowReader(NULL),
      intSwConnection(NULL), accSwConnection(NULL) {}

void PacketInHandler::registerConnection(SwitchConnection* intConnection,
                                         SwitchConnection* accessConnection) {
    intSwConnection = intConnection;
    accSwConnection = accessConnection;
}

void PacketInHandler::setPortMapper(PortMapper* intMapper,
                                    PortMapper* accessMapper) {
    intPortMapper = intMapper;
    accessPortMapper = accessMapper;
}

void PacketInHandler::start() {
    if (intSwConnection)
        intSwConnection->RegisterMessageHandler(OFPTYPE_PACKET_IN, this);
}

void PacketInHandler::stop() {
    if (intSwConnection)
        intSwConnection->UnregisterMessageHandler(OFPTYPE_PACKET_IN, this);
}

typedef std::function<void (ActionBuilder&)> output_act_t;
typedef boost::optional<output_act_t> opt_output_act_t;

static opt_output_act_t tunnelOutActions(IntFlowManager& intFlowManager,
                                         optional<URI>& egUri,
                                         uint32_t outPort) {
    opt_output_act_t outActions;
    if (egUri && (outPort == intFlowManager.getTunnelPort())) {
        outActions =
            [&egUri, &intFlowManager](ActionBuilder& ab) {
            address tunDst = intFlowManager.getEPGTunnelDst(egUri.get());
            IntFlowManager::actionTunnelMetadata(ab,
                                                 intFlowManager.getEncapType(),
                                                 tunDst);
        };
    }
    return outActions;
}

static opt_output_act_t pushVlanActions(uint32_t vlan) {
    return opt_output_act_t([vlan](ActionBuilder& ab) {
            ab.pushVlan().setVlanVid(vlan);
        });
}

static void send_packet_out(SwitchConnection* conn,
                            OfpBuf& b,
                            ofputil_protocol& proto,
                            uint32_t in_port,
                            uint32_t out_port = OFPP_IN_PORT,
                            opt_output_act_t outActions = boost::none) {
    // send reply as packet-out
    struct ofputil_packet_out po;
    match_init_catchall(&po.flow_metadata);
    po.buffer_id = UINT32_MAX;
    po.packet = b.data();
    po.packet_len = b.size();
    match_set_in_port(&po.flow_metadata, in_port);

    ActionBuilder ab;
    if (outActions) {
        outActions.get()(ab);
    }
    ab.output(out_port);
    ab.build(&po);

    OfpBuf message(ofputil_encode_packet_out(&po, proto));
    int error = conn->SendMessage(message);
    free(po.ofpacts);
    if (error) {
        LOG(ERROR) << "Could not write packet-out: " << ovs_strerror(error);
    }
}

typedef shared_ptr<const Endpoint> ep_ptr;

static void send_packet_out(Agent& agent,
                            SwitchConnection* intConn,
                            SwitchConnection* accConn,
                            IntFlowManager& intFlowManager,
                            PortMapper* intPortMapper,
                            PortMapper* accPortMapper,
                            optional<URI> egUri,
                            OfpBuf& b,
                            ofputil_protocol& proto,
                            uint32_t in_port,
                            uint32_t out_port) {
    string iface;
    opt_output_act_t outActions =
        tunnelOutActions(intFlowManager, egUri, out_port);
    opt_output_act_t outActionsSkipVlan;
    SwitchConnection* conn = intConn;
    ep_ptr ep;
    bool send_untagged = false;

    try {
        if (out_port == OFPP_IN_PORT)
            iface = intPortMapper->FindPort(in_port);
        else
            iface = intPortMapper->FindPort(out_port);

        unordered_set<string> eps;
        agent.getEndpointManager().getEndpointsByIface(iface, eps);
        if (eps.size() == 0) {
            LOG(WARNING) << "No endpoint found for output packet"
                         << " on " << iface;
            return;
        }
        if (eps.size() > 1)
            LOG(WARNING) << "Multiple possible endpoints for output packet "
                         << " on " << iface;

        ep = agent.getEndpointManager().getEndpoint(*eps.begin());
        if (ep && ep->getAccessInterface() && ep->getAccessUplinkInterface()) {
            if (!accConn || !accPortMapper) {
                return;
            }
            conn = accConn;
            uint32_t accPort =
                accPortMapper->FindPort(ep->getAccessInterface().get());
            uint32_t accUplinkPort =
                accPortMapper->FindPort(ep->getAccessUplinkInterface().get());
            if (accPort != OFPP_NONE && accUplinkPort != OFPP_NONE) {
                in_port = accUplinkPort;
                out_port = accPort;

                if (ep->getAccessIfaceVlan()) {
                    outActionsSkipVlan = outActions;
                    if (ep->isAccessAllowUntagged())
                        send_untagged = true;
                    outActions = [&ep](ActionBuilder& ab) {
                        ab.pushVlan();
                        ab.setVlanVid(ep->getAccessIfaceVlan().get());
                    };
                }
            }
        }
    } catch (std::out_of_range&) {
        LOG(WARNING) << "Port " << out_port << " not found in int bridge";
    }

    send_packet_out(conn, b, proto, in_port, out_port, outActions);
    /*
     * Openshift bootstrap does not support vlans, so make a copy
     */
    if (send_untagged)
        send_packet_out(conn, b, proto, in_port, out_port, outActionsSkipVlan);
}

/**
 * Handle a packet-in for router neighbor discovery messages.  The
 * reply is written as a packet-out to the connection
 *
 * @param agent the agent object
 * @param intFlowManager the flow manager
 * @param intConn the openflow switch connection
 * @param accConn the openflow switch connection
 * @param pi the packet-in
 * @param proto an openflow proto object
 * @param pkt the packet from the packet-in
 * @param flow the parsed flow from the packet
 */
static void handleNDPktIn(Agent& agent,
                          IntFlowManager& intFlowManager,
                          SwitchConnection* intConn,
                          SwitchConnection* accConn,
                          PortMapper* intPortMapper,
                          PortMapper* accPortMapper,
                          struct ofputil_packet_in& pi,
                          ofputil_protocol& proto,
                          const struct dp_packet* pkt,
                          struct flow& flow) {
    uint32_t epgId = (uint32_t)pi.flow_metadata.flow.regs[0];
    PolicyManager& polMgr = agent.getPolicyManager();
    optional<URI> egUri = polMgr.getGroupForVnid(epgId);

    if (flow.dl_type != htons(eth::type::IPV6) ||
        flow.nw_proto != 58 /* ICMPv6 */)
        return;

    size_t l4_size = dpp_l4_size(pkt);
    if (l4_size < sizeof(struct icmp6_hdr))
        return;
    struct icmp6_hdr* icmp = (struct icmp6_hdr*) dpp_l4(pkt);
    if (icmp->icmp6_code != 0)
        return;

    OfpBuf b((struct ofpbuf*)NULL);

    const uint8_t* mac = intFlowManager.getRouterMacAddr();
    bool router = true;
    // Use the MAC address from the metadata if available
    uint64_t metadata = ovs_ntohll(pi.flow_metadata.flow.metadata);
    if ((1u & ((uint8_t*)&metadata)[7]) == 1) {
        mac = (uint8_t*)&metadata;
        router = false;
    }

    if (icmp->icmp6_type == ND_NEIGHBOR_SOLICIT) {
        /* Neighbor solicitation */
        struct nd_neighbor_solicit* neigh_sol =
            (struct nd_neighbor_solicit*) dpp_l4(pkt);
        if (l4_size < sizeof(*neigh_sol))
            return;

        LOG(DEBUG) << "Handling ICMPv6 neighbor solicitation";
        uint32_t flags = ND_NA_FLAG_SOLICITED | ND_NA_FLAG_OVERRIDE;
        if (router) flags |= ND_NA_FLAG_ROUTER;

        b = packets::compose_icmp6_neigh_ad(flags,
                                            mac,
                                            flow.dl_src.ea,
                                            &neigh_sol->nd_ns_target,
                                            &flow.ipv6_src);
    } else if (icmp->icmp6_type == ND_ROUTER_SOLICIT && egUri) {
        /* Router solicitation */
        struct nd_router_solicit* router_sol =
            (struct nd_router_solicit*) dpp_l4(pkt);
        if (l4_size < sizeof(*router_sol))
            return;

        LOG(DEBUG) << "Handling ICMPv6 router solicitation";

        b = packets::compose_icmp6_router_ad(mac,
                                             flow.dl_src.ea,
                                             &flow.ipv6_src,
                                             egUri.get(),
                                             polMgr);
        if (!b.get()) return;
    }

    if (((uint8_t*)&metadata)[6] == 1) {
        uint32_t vlan = pi.flow_metadata.flow.regs[0];
        send_packet_out(intConn, b, proto, OFPP_CONTROLLER,
                        pi.flow_metadata.flow.regs[7], pushVlanActions(vlan));
    } else {
        send_packet_out(agent, intConn, accConn, intFlowManager,
                        intPortMapper, accPortMapper, egUri, b, proto,
                        OFPP_CONTROLLER,
                        pi.flow_metadata.flow.in_port.ofp_port);
    }
}

static void handleDHCPv4PktIn(Agent& agent,
                              IntFlowManager& intFlowManager,
                              PortMapper* intPortMapper,
                              PortMapper* accPortMapper,
                              SwitchConnection* intConn,
                              SwitchConnection* accConn,
                              const shared_ptr<const Endpoint>& ep,
                              std::string iface,
                              struct ofputil_packet_in& pi,
                              ofputil_protocol& proto,
                              struct dp_packet* pkt,
                              struct flow& flow) {
    using namespace dhcp;
    using namespace udp;

    const optional<Endpoint::DHCPv4Config>& v4c = ep->getDHCPv4Config();
    if (!v4c) return;

    const optional<string>& dhcpIpStr = v4c.get().getIpAddress();
    if (!dhcpIpStr) return;

    boost::system::error_code ec;
    address_v4 dhcpIp = address_v4::from_string(dhcpIpStr.get(), ec);
    if (ec) {
        LOG(INFO) << "bad ip " << dhcpIpStr.get() << " " << ec.message();
        return;
    }

    size_t l4_size = dpp_l4_size(pkt);
    if (l4_size < (sizeof(struct udp_hdr) + sizeof(struct dhcp_hdr) + 1))
        return;
    struct dhcp_hdr* dhcp_pkt =
        (struct dhcp_hdr*) ((char*)dpp_l4(pkt) + sizeof(struct udp_hdr));
    if (dhcp_pkt->op != 1 ||
        dhcp_pkt->htype != 1 ||
        dhcp_pkt->hlen != 6 ||
        dhcp_pkt->cookie[0] != 99 ||
        dhcp_pkt->cookie[1] != 130 ||
        dhcp_pkt->cookie[2] != 83 ||
        dhcp_pkt->cookie[3] != 99)
        return;

    uint8_t message_type = 0;
    address_v4 requested_ip = address_v4(ntohl(dhcp_pkt->ciaddr));

    char* cur = (char*)dhcp_pkt + sizeof(struct dhcp_hdr);
    size_t remaining =
        l4_size - sizeof(struct udp_hdr) - sizeof(struct dhcp_hdr);
    while (remaining > 0) {
        struct dhcp_option_hdr* hdr = (struct dhcp_option_hdr*)cur;
        if (hdr->code == option::END)
            break;
        if (hdr->code == option::PAD) {
            cur += 1;
            remaining -= 1;
            continue;
        }

        if (remaining <= ((size_t)hdr->len + 2))
            break;
        switch (hdr->code) {
        case option::MESSAGE_TYPE:
            message_type = ((uint8_t*)cur)[2];
            break;
        case option::REQUESTED_IP:
            uint32_t ipVal;
            memcpy(&ipVal, (uint8_t*)cur + 2, sizeof(ipVal));
            requested_ip = address_v4(ntohl(ipVal));
        }

        cur += hdr->len + 2;
        remaining -= hdr->len + 2;
    }

    MAC srcMac(flow.dl_src.ea);

    uint8_t prefixLen = v4c.get().getPrefixLen().get_value_or(32);
    uint8_t reply_type = message_type::NAK;

    switch(message_type) {
    case message_type::REQUEST:
        if (requested_ip == dhcpIp) {
            LOG(DEBUG) << "Handling DHCP REQUEST for IP "
                       << requested_ip << " from " << srcMac;
            reply_type = message_type::ACK;
        } else {
            LOG(WARNING) << "Rejecting DHCP REQUEST for IP "
                         << requested_ip << " from " << srcMac
                         << " on interface \"" << iface << "\"";
        }
        break;
    case message_type::DISCOVER:
        LOG(DEBUG) << "Handling DHCP DISCOVER from " << srcMac;
        reply_type = message_type::OFFER;
        break;
    default:
        LOG(DEBUG) << "Ignoring unexpected DHCP message of type "
                   << (uint16_t)message_type << " from " << srcMac;
        return;
    }

    uint8_t serverMac[6];
    if (v4c.get().getServerMac()) {
        v4c.get().getServerMac()->toUIntArray(serverMac);
    } else {
        memcpy(serverMac, intFlowManager.getDHCPMacAddr(), sizeof(serverMac));
    }

    OfpBuf b(packets::compose_dhcpv4_reply(reply_type,
                                           dhcp_pkt->xid,
                                           serverMac,
                                           flow.dl_src.ea,
                                           dhcpIp.to_ulong(),
                                           prefixLen,
                                           v4c.get().getServerIp(),
                                           v4c.get().getRouters(),
                                           v4c.get().getDnsServers(),
                                           v4c.get().getDomain(),
                                           v4c.get().getStaticRoutes(),
                                           v4c.get().getInterfaceMtu(),
                                           v4c.get().getLeaseTime()));

    send_packet_out(agent, intConn, accConn, intFlowManager,
                    intPortMapper, accPortMapper, URI::ROOT, b,
                    proto, OFPP_CONTROLLER,
                    pi.flow_metadata.flow.in_port.ofp_port);
}

static void handleDHCPv6PktIn(Agent& agent,
                              IntFlowManager& intFlowManager,
                              PortMapper* intPortMapper,
                              PortMapper* accPortMapper,
                              SwitchConnection* intConn,
                              SwitchConnection* accConn,
                              const shared_ptr<const Endpoint>& ep,
                              struct ofputil_packet_in& pi,
                              ofputil_protocol& proto,
                              struct dp_packet* pkt,
                              struct flow& flow) {
    using namespace dhcp6;
    using namespace udp;

    const optional<Endpoint::DHCPv6Config>& v6c = ep->getDHCPv6Config();
    if (!v6c) return;

    const unordered_set<string>& addresses = ep->getIPs();

    boost::system::error_code ec;
    vector<address_v6> v6addresses;
    for (const string& addrStr : addresses) {
        address_v6 addr_v6 = address_v6::from_string(addrStr, ec);
        if (ec) continue;
        v6addresses.push_back(addr_v6);
    }

    size_t l4_size = dpp_l4_size(pkt);
    if (l4_size < (sizeof(struct udp_hdr) + sizeof(struct dhcp6_hdr) + 1))
        return;
    struct dhcp6_hdr* dhcp_pkt =
        (struct dhcp6_hdr*)((char*)dpp_l4(pkt) + sizeof(struct udp_hdr));

    uint8_t message_type = dhcp_pkt->msg_type;
    uint8_t* client_id = NULL;
    uint16_t client_id_len = 0;
    uint8_t* iaid = NULL;
    bool temporary = false;
    bool rapid_commit = false;

    const size_t opt_hdr_len = sizeof(struct dhcp6_opt_hdr);

    char* cur = (char*)dhcp_pkt + sizeof(struct dhcp6_hdr);
    size_t remaining =
        l4_size - sizeof(struct udp_hdr) - sizeof(struct dhcp6_hdr);
    while (remaining >= opt_hdr_len) {
        struct dhcp6_opt_hdr* hdr = (struct dhcp6_opt_hdr*)cur;
        uint16_t opt_len = ntohs(hdr->option_len);
        uint16_t opt_code = ntohs(hdr->option_code);

        if (remaining < ((size_t)opt_len + opt_hdr_len))
            break;
        switch (opt_code) {
        case option::CLIENT_IDENTIFIER:
            client_id = (uint8_t*)(cur + opt_hdr_len);
            client_id_len = opt_len;
            if (client_id_len > 32) return;
            break;
        case option::IA_TA:
            temporary = true;
            /* fall through */
        case option::IA_NA:
            {
                // We only support one identity association
                if (opt_len < 4) return;
                iaid = (uint8_t*)(cur + opt_hdr_len);
            }
            break;
        case option::RAPID_COMMIT:
            rapid_commit = true;
            break;
        }

        cur += opt_len + opt_hdr_len;
        remaining -= opt_len + opt_hdr_len;
    }

    uint8_t reply_type = message_type::REPLY;
    MAC srcMac(flow.dl_src.ea);

    switch(message_type) {
    case message_type::SOLICIT:
        LOG(DEBUG) << "Handling DHCPv6 Solicit from " << srcMac;
        if (rapid_commit)
            reply_type = message_type::REPLY;
        else
            reply_type = message_type::ADVERTISE;
        break;
    case message_type::REQUEST:
        LOG(DEBUG) << "Handling DHCPv6 Request from " << srcMac;
        break;
    case message_type::CONFIRM:
        LOG(DEBUG) << "Handling DHCPv6 Confirm from " << srcMac;
        break;
    case message_type::RENEW:
        LOG(DEBUG) << "Handling DHCPv6 Renew from " << srcMac;
        break;
    case message_type::REBIND:
        LOG(DEBUG) << "Handling DHCPv6 Rebind from " << srcMac;
        break;
    case message_type::INFORMATION_REQUEST:
        LOG(DEBUG) << "Handling DHCPv6 Information-request from " << srcMac;
        break;
    default:
        LOG(DEBUG) << "Ignoring unexpected DHCPv6 message of type "
                   << message_type << " from " << srcMac;
        return;
    }

    OfpBuf b(packets::compose_dhcpv6_reply(reply_type,
                                           dhcp_pkt->transaction_id,
                                           intFlowManager.getDHCPMacAddr(),
                                           flow.dl_src.ea,
                                           &flow.ipv6_src,
                                           client_id,
                                           client_id_len,
                                           iaid,
                                           v6addresses,
                                           v6c.get().getDnsServers(),
                                           v6c.get().getSearchList(),
                                           temporary,
                                           rapid_commit,
                                           v6c.get().getT1(),
                                           v6c.get().getT2(),
                                           v6c.get().getPreferredLifetime(),
                                           v6c.get().getValidLifetime()));

    send_packet_out(agent, intConn, accConn, intFlowManager,
                    intPortMapper, accPortMapper, URI::ROOT, b, proto,
                    OFPP_CONTROLLER,
                    pi.flow_metadata.flow.in_port.ofp_port);
}

typedef std::function<bool (const Endpoint&)> ep_pred;
/*
 * Find EPs on an interface that match a predicate
 */
static unordered_set<ep_ptr> findEpsForIface(EndpointManager& epMgr,
                                             std::string iface,
                                             ep_pred pred) {
    unordered_set<ep_ptr> eps;
    unordered_set<string> try_uuids;
    epMgr.getEndpointsByIface(iface, try_uuids);

    for (const string& epUuid : try_uuids) {
        ep_ptr try_ep = epMgr.getEndpoint(epUuid);
        if (!try_ep) continue;
        if (pred(*try_ep)) {
            eps.insert(try_ep);
        }
    }

    return eps;
}

/**
 * Handle a packet-in for DHCP messages.  The reply is written as a
 * packet-out to the connection
 *
 * @param v4 true if this is a DHCPv4 message, or false for DHCPv6
 * @param agent the agent object
 * @param intFlowManager the flow manager
 * @param intConn the openflow switch connection
 * @param accConn the openflow switch connection
 * @param pi the packet-in
 * @param proto an openflow proto object
 * @param pkt the packet from the packet-in
 * @param flow the parsed flow from the packet
 * @param virtualDhcpMac the MAC address of the virtual DHCP server
 */
static void handleDHCPPktIn(bool v4,
                            Agent& agent,
                            IntFlowManager& intFlowManager,
                            PortMapper* intPortMapper,
                            PortMapper* accPortMapper,
                            SwitchConnection* intConn,
                            SwitchConnection* accConn,
                            struct ofputil_packet_in& pi,
                            ofputil_protocol& proto,
                            struct dp_packet* pkt,
                            struct flow& flow) {
    if (!intPortMapper) return;
    EndpointManager& epMgr = agent.getEndpointManager();

    MAC srcMac(flow.dl_src.ea);

    string iface;
    try {
        iface = intPortMapper->FindPort(pi.flow_metadata.flow.in_port.ofp_port);
    } catch (std::out_of_range&) {
        return;
    }

    unordered_set<ep_ptr> eps =
        findEpsForIface(epMgr, iface,
                       [&srcMac](const Endpoint& ep) {
                           const optional<MAC>& epMac = ep.getMAC();
                           if (epMac && srcMac == epMac.get()) {
                               return true;
                           }
                           const Endpoint::virt_ip_set& virtIps =
                               ep.getVirtualIPs();

                           for (const Endpoint::virt_ip_t& virt_ip : virtIps) {
                               if (virt_ip.first == srcMac) {
                                   return true;
                               }
                           }
                           return false;
                       });

    if (eps.size() == 0) {
        LOG(WARNING) << "No endpoint found for DHCP request from "
                     << srcMac << " on " << iface;
        return;
    }
    if (eps.size() > 1)
        LOG(WARNING) << "Multiple possible endpoints for DHCP request from "
                     << srcMac << " on " << iface;

    const shared_ptr<const Endpoint> ep = *eps.begin();

    if (v4)
        handleDHCPv4PktIn(agent, intFlowManager,
                          intPortMapper, accPortMapper, intConn, accConn,
                          ep, iface, pi, proto, pkt, flow);
    else
        handleDHCPv6PktIn(agent, intFlowManager,
                          intPortMapper, accPortMapper, intConn, accConn,
                          ep, pi, proto, pkt, flow);

}

static void handleVIPPktIn(bool v4,
                           Agent& agent,
                           PortMapper& intPortMapper,
                           struct ofputil_packet_in& pi,
                           struct flow& flow) {
    EndpointManager& epMgr = agent.getEndpointManager();
    NotifServer& notifServer = agent.getNotifServer();
    MAC srcMac(flow.dl_src.ea);
    address srcIp;

    if (v4) {
        // ARP reply
        if (flow.dl_type != htons(eth::type::ARP))
            return;

        srcIp = address_v4(ntohl(flow.nw_src));
    } else {
        // Neighbor advertisement
        if (flow.dl_type != htons(eth::type::IPV6) ||
            flow.nw_proto != 58 /* ICMPv6 */)
            return;

        address_v6::bytes_type bytes;
        std::memcpy(bytes.data(), &flow.nd_target, bytes.size());
        srcIp = address_v6(bytes);
    }

    string iface;
    try {
        iface = intPortMapper.FindPort(pi.flow_metadata.flow.in_port.ofp_port);
    } catch (std::out_of_range&) {
        return;
    }

    unordered_set<ep_ptr> eps =
        findEpsForIface(epMgr, iface,
                        [&srcMac, &srcIp](const Endpoint& ep) {
                            for (const Endpoint::virt_ip_t& vip :
                                     ep.getVirtualIPs()) {
                                network::cidr_t cidr;
                                if (network::cidr_from_string(vip.second, cidr)
                                    && srcMac == vip.first
                                    && network::cidr_contains(cidr, srcIp)) {
                                    return true;
                                }
                            }
                            return false;
                        });

    unordered_set<string> uuids;
    std::for_each(eps.begin(), eps.end(),
                  [&uuids](const ep_ptr& ep) {
                      uuids.insert(ep->getUUID());
                  });

    if (uuids.size() > 0) {
        LOG(DEBUG) << "Virtual IP ownership advertised for ("
                   << srcMac << ", " << srcIp << ")";
        notifServer.dispatchVirtualIp(uuids, srcMac, srcIp.to_string());
    }
}

static void handleICMPErrPktIn(bool v4,
                               Agent& agent,
                               IntFlowManager& intFlowManager,
                               PortMapper* intPortMapper,
                               PortMapper* accPortMapper,
                               SwitchConnection* intConn,
                               SwitchConnection* accConn,
                               struct ofputil_packet_in& pi,
                               ofputil_protocol& proto,
                               struct dp_packet* pkt) {
    uint32_t epgId = (uint32_t)pi.flow_metadata.flow.regs[0];
    optional<URI> egUri = agent.getPolicyManager().getGroupForVnid(epgId);

    if (v4) {
        size_t l4_size = dpp_l4_size(pkt);
        if (l4_size < (sizeof(struct icmphdr) + sizeof(struct iphdr)))
            return;

        char* pkt_data = (char*)dpp_data(pkt);
        OfpBuf b(ofpbuf_clone_data(pkt_data, dpp_size(pkt)));
        struct iphdr* outer =
            (struct iphdr*)((char*)dpp_l3(pkt));

        size_t inner_offset = (char*)dpp_l4(pkt)-pkt_data;
        struct icmphdr* inner_icmp =
            (struct icmphdr*)b.at_assert(inner_offset, sizeof(icmphdr));
        struct iphdr* inner_ip =
            (struct iphdr*)b.at_assert(inner_offset + sizeof(struct icmphdr),
                                       sizeof(iphdr));
        memcpy(&inner_ip->saddr, &outer->daddr, sizeof(inner_ip->saddr));

        // compute checksum
        memset(&inner_icmp->checksum, 0, sizeof(inner_icmp->checksum));
        uint32_t chksum = 0;
        packets::chksum_accum(chksum, (uint16_t*)inner_icmp, l4_size);
        uint16_t fchksum = packets::chksum_finalize(chksum);
        memcpy(&inner_icmp->checksum, &fchksum, sizeof(fchksum));

        uint32_t saddr;
        memcpy(&saddr, &inner_ip->saddr, sizeof(saddr));
        saddr = htonl(saddr);
        LOG(DEBUG) << "Translating ICMPv4 error packet for "
                   << boost::asio::ip::address_v4(saddr)
                   << " on " << pi.flow_metadata.flow.regs[7];

        send_packet_out(agent, intConn, accConn, intFlowManager,
                        intPortMapper, accPortMapper, egUri,
                        b, proto, pi.flow_metadata.flow.regs[7],
                        OFPP_IN_PORT);
    }

}

static void handleICMPEchoPktIn(bool v4,
                                Agent& agent,
                                IntFlowManager& intFlowManager,
                                PortMapper* intPortMapper,
                                PortMapper* accPortMapper,
                                SwitchConnection* intConn,
                                SwitchConnection* accConn,
                                struct ofputil_packet_in& pi,
                                ofputil_protocol& proto,
                                struct dp_packet* pkt) {
    uint32_t epgId = (uint32_t)pi.flow_metadata.flow.regs[0];
    optional<URI> egUri = agent.getPolicyManager().getGroupForVnid(epgId);

    size_t l4_size = dpp_l4_size(pkt);

    if (v4) {
        if (l4_size < sizeof(struct icmphdr))
            return;
    } else {
        if (l4_size < sizeof(struct icmp6_hdr))
            return;
    }

    char* pkt_data = (char*)dpp_data(pkt);
    OfpBuf b(ofpbuf_clone_data(pkt_data, dpp_size(pkt)));

    uint8_t* eth_hdr = (uint8_t*)b.at_assert(0, sizeof(eth::eth_header));

    size_t ip_offset = (char*)dpp_l3(pkt)-pkt_data;
    size_t icmp_offset = (char*)dpp_l4(pkt)-pkt_data;

    // swap source/dest MAC
    uint8_t mac[eth::ADDR_LEN];
    memcpy(&mac, eth_hdr, eth::ADDR_LEN);
    memcpy(eth_hdr, eth_hdr + eth::ADDR_LEN, eth::ADDR_LEN);
    memcpy(eth_hdr + eth::ADDR_LEN, mac, eth::ADDR_LEN);

    if (v4) {
        struct iphdr* ip =
            (struct iphdr*)b.at_assert(ip_offset, sizeof(iphdr));
        struct icmphdr* icmp =
            (struct icmphdr*)b.at_assert(icmp_offset, sizeof(icmphdr));

        // swap source/dest IP
        uint32_t saddr;
        uint32_t daddr;
        memcpy(&saddr, &ip->saddr, sizeof(saddr));
        memcpy(&daddr, &ip->daddr, sizeof(daddr));
        memcpy(&ip->saddr, &daddr, sizeof(daddr));
        memcpy(&ip->daddr, &saddr, sizeof(saddr));

        // set ICMP type, code, checksum
        memset(icmp, 0, 4);

        // compute checksum
        uint32_t chksum = 0;
        packets::chksum_accum(chksum, (uint16_t*)icmp, l4_size);
        uint16_t fchksum = packets::chksum_finalize(chksum);
        memcpy(&icmp->checksum, &fchksum, sizeof(fchksum));

        LOG(DEBUG) << "Replying to ICMPv4 echo request "
                   << boost::asio::ip::address_v4(ntohl(saddr)) << "->"
                   << boost::asio::ip::address_v4(ntohl(daddr))
                   << " on " << pi.flow_metadata.flow.in_port.ofp_port;
    } else {
        struct ip6_hdr* ip6 =
            (struct ip6_hdr*)b.at_assert(ip_offset, sizeof(struct ip6_hdr));
        struct icmp6_hdr* icmp =
            (struct icmp6_hdr*)b.at_assert(icmp_offset,
                                           sizeof(struct icmp6_hdr));

        // swap source/dest IP
        boost::asio::ip::address_v6::bytes_type saddr;
        boost::asio::ip::address_v6::bytes_type daddr;
        memcpy(saddr.data(), &ip6->ip6_src, saddr.size());
        memcpy(daddr.data(), &ip6->ip6_dst, daddr.size());
        memcpy(&ip6->ip6_src, daddr.data(), daddr.size());
        memcpy(&ip6->ip6_dst, saddr.data(), saddr.size());

        // set ICMP type, code, checksum
        memset(icmp, 0, 4);
        memset(icmp, 129, 1);

        // compute checksum
        uint32_t chksum = 0;
        // pseudoheader
        packets::chksum_accum(chksum, (uint16_t*)daddr.data(), daddr.size());
        packets::chksum_accum(chksum, (uint16_t*)saddr.data(), saddr.size());
        packets::chksum_accum(chksum, (uint16_t*)ip6 + 2, 2);
        chksum += (uint16_t)htons(58);
        // payload
        packets::chksum_accum(chksum, (uint16_t*)icmp, l4_size);
        uint16_t fchksum = packets::chksum_finalize(chksum);
        memcpy(&icmp->icmp6_cksum, &fchksum, sizeof(fchksum));

        LOG(DEBUG) << "Replying to ICMPv6 echo request "
                   << boost::asio::ip::address_v6(saddr) << "->"
                   << boost::asio::ip::address_v6(daddr)
                   << " on " << pi.flow_metadata.flow.in_port.ofp_port;
    }

    uint64_t metadata = ovs_ntohll(pi.flow_metadata.flow.metadata);
    if (((uint8_t*)&metadata)[6] == 1) {
        uint32_t vlan = pi.flow_metadata.flow.regs[0];
        send_packet_out(intConn, b, proto, OFPP_CONTROLLER,
                        pi.flow_metadata.flow.in_port.ofp_port,
                        pushVlanActions(vlan));
    } else {
        send_packet_out(agent, intConn, accConn, intFlowManager,
                        intPortMapper, accPortMapper, egUri,
                        b, proto, pi.flow_metadata.flow.regs[7],
                        OFPP_IN_PORT);
    }
}

/**
 * Dispatch packet-in messages to the appropriate handlers
 */
void PacketInHandler::Handle(SwitchConnection* conn,
                             int msgType, ofpbuf *msg,
                             struct ofputil_flow_removed* fentry) {
    assert(msgType == OFPTYPE_PACKET_IN);

    const struct ofp_header *oh = (ofp_header *)msg->data;
    struct ofputil_packet_in pi;
    uint32_t pi_buffer_id;

    enum ofperr err = ofputil_decode_packet_in(oh, false, NULL, NULL,
                                               &pi, NULL,
                                               &pi_buffer_id, NULL);
    if (err) {
        LOG(ERROR) << "Failed to decode packet-in: " << ovs_strerror(err);
        return;
    }

    if (pi.reason != OFPR_ACTION)
        return;

    DpPacketP pkt;
    struct flow flow;

    dp_packet_use_const(pkt.get(), pi.packet, pi.packet_len);
    flow_extract(pkt.get(), &flow);

    ofputil_protocol proto = ofputil_protocol_from_ofp_version
        ((ofp_version)conn->GetProtocolVersion());
    assert(ofputil_protocol_is_valid(proto));

    if (pi.cookie == flow::cookie::NEIGH_DISC)
        handleNDPktIn(agent, intFlowManager, conn, accSwConnection,
                      intPortMapper, accessPortMapper,
                      pi, proto, pkt.get(), flow);
    else if (pi.cookie == flow::cookie::DHCP_V4)
        handleDHCPPktIn(true, agent, intFlowManager, intPortMapper,
                        accessPortMapper, conn, accSwConnection,
                        pi, proto, pkt.get(), flow);
    else if (pi.cookie == flow::cookie::DHCP_V6)
        handleDHCPPktIn(false, agent, intFlowManager,
                        intPortMapper, accessPortMapper,
                        conn, accSwConnection, pi, proto, pkt.get(), flow);
    else if (pi.cookie == flow::cookie::VIRTUAL_IP_V4)
        handleVIPPktIn(true, agent, *intPortMapper, pi, flow);
    else if (pi.cookie == flow::cookie::VIRTUAL_IP_V6)
        handleVIPPktIn(false, agent, *intPortMapper, pi, flow);
    else if (pi.cookie == flow::cookie::ICMP_ERROR_V4)
        handleICMPErrPktIn(true, agent, intFlowManager,
                           intPortMapper, accessPortMapper,
                           conn, accSwConnection, pi, proto, pkt.get());
    else if (pi.cookie == flow::cookie::ICMP_ECHO_V4)
        handleICMPEchoPktIn(true, agent, intFlowManager,
                            intPortMapper, accessPortMapper,
                            conn, accSwConnection, pi, proto, pkt.get());
    else if (pi.cookie == flow::cookie::ICMP_ECHO_V6)
        handleICMPEchoPktIn(false, agent, intFlowManager,
                            intPortMapper, accessPortMapper,
                            conn, accSwConnection, pi, proto, pkt.get());
}

} /* namespace opflexagent */
