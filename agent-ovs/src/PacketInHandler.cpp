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
#include "logging.h"
#include "dhcp.h"
#include "udp.h"
#include "eth.h"
#include "IntFlowManager.h"
#include "FlowConstants.h"
#include "ovs-shim.h"
#include "ovs-ofputil.h"

// OVS lib
#include <lib/util.h>

using std::string;
using std::vector;
using std::unordered_map;
using std::unordered_set;
using std::shared_ptr;
using boost::asio::ip::address;
using boost::asio::ip::address_v6;
using boost::asio::ip::address_v4;
using boost::optional;
using opflex::modb::MAC;
using opflex::modb::URI;
using modelgbp::gbp::Subnet;

namespace ovsagent {

PacketInHandler::PacketInHandler(Agent& agent_,
                                 IntFlowManager& intFlowManager_)
    : agent(agent_), intFlowManager(intFlowManager_),
      portMapper(NULL), flowReader(NULL), switchConnection(NULL) {}


bool PacketInHandler::writeLearnFlow(SwitchConnection *conn,
                                     int proto,
                                     struct ofputil_packet_in& pi,
                                     struct flow& flow,
                                     bool stage2) {
    uint32_t tunPort = intFlowManager.getTunnelPort();

    bool dstKnown = (0 != pi.flow_metadata.flow.regs[7]);
    bool broadcast = (flow.dl_dst.ea[0] & 0x01) != 0;
    if (stage2 && (!dstKnown || broadcast)) return false;

    uint8_t* flowMac = NULL;

    struct ofputil_flow_mod fm;
    memset(&fm, 0, sizeof(fm));
    fm.buffer_id = UINT32_MAX;
    fm.table_id = IntFlowManager::LEARN_TABLE_ID;
    fm.priority = stage2 ? 150 : 100;
    fm.idle_timeout = 300;
    fm.command = OFPFC_ADD;
    fm.new_cookie = flow::cookie::LEARN;

    match_set_reg(&fm.match, 5 /* REG5 */, pi.flow_metadata.flow.regs[5]);
    if (stage2) {
        match_set_in_port(&fm.match, pi.flow_metadata.flow.in_port.ofp_port);
        flowMac = flow.dl_dst.ea;
        match_set_dl_dst(&fm.match, flow.dl_dst);
        match_set_dl_src(&fm.match, flow.dl_src);
    } else {
        flowMac = flow.dl_src.ea;
        match_set_dl_dst(&fm.match, flow.dl_src);
    }

    // Set the output register
    uint32_t outport = stage2
        ? pi.flow_metadata.flow.regs[7]
        : pi.flow_metadata.flow.in_port.ofp_port;

    ActionBuilder ab;

    // Set destination epg == source epg
    ab.reg(MFF_REG2, pi.flow_metadata.flow.regs[0]);
    ab.reg(MFF_REG7, outport);
    if (stage2) {
        if (outport == tunPort) {
            ab.metadata(flow::meta::out::TUNNEL, flow::meta::out::MASK);
        }
        ab.go(IntFlowManager::POL_TABLE_ID);
    } else {
        ab.output(outport);
        ab.controller();
    }
    ab.build(&fm);

    struct ofpbuf * message =
        ofputil_encode_flow_mod(&fm, (ofputil_protocol)proto);
    int error = conn->SendMessage(message);
    free(fm.ofpacts);
    if (error) {
        LOG(ERROR) << "Could not write flow mod: " << ovs_strerror(error);
    }

    if (flowReader && !stage2) {
        FlowReader::FlowCb dstCb =
            boost::bind(&PacketInHandler::dstFlowCb, this,
                        _1, MAC(flowMac), outport,
                        pi.flow_metadata.flow.regs[5]);
        match fmatch;
        memset(&fmatch, 0, sizeof(fmatch));
        match_set_reg(&fmatch, 5 /* REG5 */, pi.flow_metadata.flow.regs[5]);
        match_set_dl_dst(&fmatch, flow.dl_src);

        flowReader->getFlows(IntFlowManager::LEARN_TABLE_ID,
                             &fmatch, dstCb);
    }

    return true;
}

static uint32_t getOutputRegValue(const FlowEntryPtr& fe) {
    return get_output_reg_value(fe->entry->ofpacts, fe->entry->ofpacts_len);
}

static void removeLearnFlow(SwitchConnection* conn, const FlowEntryPtr& fe) {
    ofputil_protocol proto = ofputil_protocol_from_ofp_version
        ((ofp_version)conn->GetProtocolVersion());
    assert(ofputil_protocol_is_valid(proto));

    struct ofputil_flow_mod fm;
    memset(&fm, 0, sizeof(fm));
    fm.table_id = IntFlowManager::LEARN_TABLE_ID;
    fm.command = OFPFC_DELETE_STRICT;
    fm.priority = fe->entry->priority;
    fm.cookie = flow::cookie::LEARN;
    memcpy(&fm.match, &fe->entry->match, sizeof(fe->entry->match));

    fm.idle_timeout = fe->entry->idle_timeout;
    fm.hard_timeout = fe->entry->hard_timeout;
    fm.buffer_id = UINT32_MAX;
    fm.out_port = OFPP_ANY;
    fm.out_group = OFPG_ANY;
    fm.flags = (ofputil_flow_mod_flags)0;

    struct ofpbuf * message = ofputil_encode_flow_mod(&fm, proto);
    int error = conn->SendMessage(message);
    if (error) {
        LOG(ERROR) << "Could not delete flow mod: " << ovs_strerror(error);
    }
}

void PacketInHandler::dstFlowCb(const FlowEntryList& flows,
                                const MAC& dstMac, uint32_t outPort,
                                uint32_t fgrpId) {
    if (!switchConnection) return;
    for (const FlowEntryPtr& fe : flows) {
        if (fe->entry->cookie == flow::cookie::LEARN) {
            uint32_t port = getOutputRegValue(fe);
            uint32_t flowFgrpId = fe->entry->match.flow.regs[5];
            MAC flowDstMac(fe->entry->match.flow.dl_dst.ea);
            if (flowFgrpId == fgrpId && flowDstMac == dstMac &&
                port != outPort) {
                LOG(DEBUG) << "Removing stale learn flow with dst "
                           << flowDstMac
                           << " on port " << port;
                removeLearnFlow(switchConnection, fe);
            }
        }
    }
}

void PacketInHandler::anyFlowCb(const FlowEntryList& flows) {
    if (!switchConnection) return;
    for (const FlowEntryPtr& fe : flows) {
        if (fe->entry->cookie == flow::cookie::LEARN &&
            !reconcileReactiveFlow(fe)) {
            removeLearnFlow(switchConnection, fe);
        }
    }
}

bool PacketInHandler::reconcileReactiveFlow(const FlowEntryPtr& fe) {
    EndpointManager& epMgr = agent.getEndpointManager();

    if (!portMapper) return true;
    if (fe->entry->cookie != flow::cookie::LEARN)
        return false;   // non-reactive entries must be reconciled

    uint32_t port = getOutputRegValue(fe);
    if (port == intFlowManager.getTunnelPort())
        return true;

    MAC dstMac(fe->entry->match.flow.dl_dst.ea);

    try {
        const string& portName = portMapper->FindPort(port);
        unordered_set<string> eps;
        epMgr.getEndpointsByIface(portName, eps);
        for (const string& uuid : eps) {
            shared_ptr<const Endpoint> ep = epMgr.getEndpoint(uuid);
            if (!ep) continue;
            if (ep->isPromiscuousMode()) return true;

            optional<MAC> mac = ep->getMAC();
            if (!mac) continue;
            if (mac == dstMac) return true;
        }
    } catch (std::out_of_range) {
        // fall through
    }

    LOG(DEBUG) << "Removing stale learn flow with dst " << dstMac
               << " on port " << port;
    return false;
}

void PacketInHandler::portStatusUpdate(const string&, uint32_t,
                                       bool fromDesc) {
    if (fromDesc) return;
    if (flowReader && portMapper) {
        FlowReader::FlowCb cb =
            boost::bind(&PacketInHandler::anyFlowCb, this, _1);
        flowReader->getFlows(IntFlowManager::LEARN_TABLE_ID, cb);
    }
}


void PacketInHandler::handleLearnPktIn(SwitchConnection *conn,
                                       struct ofputil_packet_in& pi,
                                       uint32_t pi_buffer_id,
                                       int proto,
                                       const struct dp_packet* pkt,
                                       struct flow& flow) {
    writeLearnFlow(conn, proto, pi, flow, false);
    if (writeLearnFlow(conn, proto, pi, flow, true))
        return;

    uint32_t epgId = (uint32_t)pi.flow_metadata.flow.regs[0];
    PolicyManager& polMgr = agent.getPolicyManager();
    optional<URI> egUri = polMgr.getGroupForVnid(epgId);
    const address tunDst = egUri
        ? intFlowManager.getEPGTunnelDst(egUri.get())
        : intFlowManager.getTunnelDst();

    bool broadcast = (flow.dl_dst.ea[0] & 0x01) != 0;
    uint32_t groupId = broadcast
        ? pi.flow_metadata.flow.regs[5]
        : IntFlowManager::getPromId(pi.flow_metadata.flow.regs[5]);
    {
        // install a forward flow to flood to the promiscuous ports in
        // the flood domain until we learn the correct reverse path
        struct ofputil_flow_mod fm;
        memset(&fm, 0, sizeof(fm));
        fm.buffer_id = pi_buffer_id;
        fm.table_id = IntFlowManager::LEARN_TABLE_ID;
        fm.priority = 50;
        fm.idle_timeout = 5;
        fm.hard_timeout = 60;
        fm.command = OFPFC_ADD;
        fm.new_cookie = flow::cookie::LEARN;

        match_set_in_port(&fm.match, pi.flow_metadata.flow.in_port.ofp_port);
        match_set_reg(&fm.match, 5 /* REG5 */, pi.flow_metadata.flow.regs[5]);
        match_set_dl_src(&fm.match, flow.dl_src);

        ActionBuilder ab;
        ab.reg(MFF_REG7, tunDst.to_v4().to_ulong());
        ab.group(groupId);
        ab.build(&fm);

        struct ofpbuf* message = ofputil_encode_flow_mod
            (&fm, (ofputil_protocol)proto);
        int error = conn->SendMessage(message);
        free(fm.ofpacts);
        if (error) {
            LOG(ERROR) << "Could not write flow mod: " << ovs_strerror(error);
        }
    }

    if (pi_buffer_id == UINT32_MAX) {
        // Send packet out if needed
        struct ofputil_packet_out po;
        po.buffer_id = UINT32_MAX;
        po.packet = dpp_data(pkt);
        po.packet_len = dpp_size(pkt);
        po.in_port = pi.flow_metadata.flow.in_port.ofp_port;

        ActionBuilder ab;
        ab.reg(MFF_REG0, pi.flow_metadata.flow.regs[0]);
        ab.reg(MFF_REG7, tunDst.to_v4().to_ulong());
        ab.group(groupId);
        ab.build(&po);

        struct ofpbuf* message =
            ofputil_encode_packet_out(&po, (ofputil_protocol)proto);
        int error = conn->SendMessage(message);
        free(po.ofpacts);
        if (error) {
            LOG(ERROR) << "Could not write packet-out: " << ovs_strerror(error);
        }
    }
}

static void send_packet_out(IntFlowManager& intFlowManager,
                            SwitchConnection *conn,
                            struct ofpbuf* b,
                            ofputil_protocol& proto,
                            uint32_t in_port,
                            uint32_t out_port = OFPP_IN_PORT,
                            const URI& egURI = URI::ROOT) {
    // send reply as packet-out
    struct ofputil_packet_out po;
    po.buffer_id = UINT32_MAX;
    po.packet = b->data;
    po.packet_len = b->size;
    po.in_port = in_port;

    uint32_t tunPort = intFlowManager.getTunnelPort();

    ActionBuilder ab;
    if (out_port == tunPort ||
        (in_port == tunPort && out_port == OFPP_IN_PORT)) {
        address tunDst = intFlowManager.getEPGTunnelDst(egURI);
        IntFlowManager::actionTunnelMetadata(ab,
                                             intFlowManager.getEncapType(),
                                             tunDst);
    }
    ab.output(out_port);
    ab.build(&po);

    struct ofpbuf* message = ofputil_encode_packet_out(&po, proto);
    int error = conn->SendMessage(message);
    free(po.ofpacts);
    if (error) {
        LOG(ERROR) << "Could not write packet-out: " << ovs_strerror(error);
    }

    ofpbuf_delete(b);
}


/**
 * Handle a packet-in for router neighbor discovery messages.  The
 * reply is written as a packet-out to the connection
 *
 * @param agent the agent object
 * @param intFlowManager the flow manager
 * @param conn the openflow switch connection
 * @param pi the packet-in
 * @param proto an openflow proto object
 * @param pkt the packet from the packet-in
 * @param flow the parsed flow from the packet
 */
static void handleNDPktIn(Agent& agent,
                          IntFlowManager& intFlowManager,
                          SwitchConnection *conn,
                          struct ofputil_packet_in& pi,
                          ofputil_protocol& proto,
                          const struct dp_packet* pkt,
                          struct flow& flow) {
    uint32_t epgId = (uint32_t)pi.flow_metadata.flow.regs[0];
    PolicyManager& polMgr = agent.getPolicyManager();
    optional<URI> egUri = polMgr.getGroupForVnid(epgId);
    if (!egUri)
        return;

    if (flow.dl_type != htons(eth::type::IPV6) ||
        flow.nw_proto != 58 /* ICMPv6 */)
        return;

    size_t l4_size = dpp_l4_size(pkt);
    if (l4_size < sizeof(struct icmp6_hdr))
        return;
    struct icmp6_hdr* icmp = (struct icmp6_hdr*) dpp_l4(pkt);
    if (icmp->icmp6_code != 0)
        return;

    struct ofpbuf* b = NULL;

    const uint8_t* mac = intFlowManager.getRouterMacAddr();
    bool router = true;
    // Use the MAC address from the metadata if available
    uint64_t metadata = ovs_ntohll(pi.flow_metadata.flow.metadata);
    if (((uint8_t*)&metadata)[7] == 1) {
        mac = (uint8_t*)&metadata;
        router = mac[6] == 1;
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

    } else if (icmp->icmp6_type == ND_ROUTER_SOLICIT) {
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
    }

    if (b)
        send_packet_out(intFlowManager, conn, b, proto,
                        pi.flow_metadata.flow.in_port.ofp_port,
                        OFPP_IN_PORT, egUri.get());
}

static void handleDHCPv4PktIn(shared_ptr<const Endpoint>& ep,
                              IntFlowManager& intFlowManager,
                              PortMapper* portMapper,
                              SwitchConnection *conn,
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
            requested_ip = address_v4(ntohl(*((uint32_t*)((uint8_t*)cur + 2))));
        }

        cur += hdr->len + 2;
        remaining -= hdr->len + 2;
    }

    struct ofpbuf* b = NULL;
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
            string iface = "[unknown]";
            try {
                if (portMapper)
                    iface =
                        portMapper->FindPort(pi.flow_metadata.flow
                                             .in_port.ofp_port);
            } catch (std::out_of_range) {}

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

    b = packets::compose_dhcpv4_reply(reply_type,
                                      dhcp_pkt->xid,
                                      intFlowManager.getDHCPMacAddr(),
                                      flow.dl_src.ea,
                                      dhcpIp.to_ulong(),
                                      prefixLen,
                                      v4c.get().getServerIp(),
                                      v4c.get().getRouters(),
                                      v4c.get().getDnsServers(),
                                      v4c.get().getDomain(),
                                      v4c.get().getStaticRoutes(),
                                      v4c.get().getInterfaceMtu());

    if (b)
        send_packet_out(intFlowManager, conn, b, proto,
                        pi.flow_metadata.flow.in_port.ofp_port);
}

static void handleDHCPv6PktIn(shared_ptr<const Endpoint>& ep,
                              IntFlowManager& intFlowManager,
                              SwitchConnection *conn,
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

    struct ofpbuf* b = NULL;
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

    b = packets::compose_dhcpv6_reply(reply_type,
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
                                      rapid_commit);

    if (b)
        send_packet_out(intFlowManager, conn, b, proto,
                        pi.flow_metadata.flow.in_port.ofp_port);
}


/**
 * Handle a packet-in for DHCP messages.  The reply is written as a
 * packet-out to the connection
 *
 * @param v4 true if this is a DHCPv4 message, or false for DHCPv6
 * @param agent the agent object
 * @param intFlowManager the flow manager
 * @param conn the openflow switch connection
 * @param pi the packet-in
 * @param proto an openflow proto object
 * @param pkt the packet from the packet-in
 * @param flow the parsed flow from the packet
 * @param virtualDhcpMac the MAC address of the virtual DHCP server
 */
static void handleDHCPPktIn(bool v4,
                            Agent& agent,
                            IntFlowManager& intFlowManager,
                            PortMapper* portMapper,
                            SwitchConnection *conn,
                            struct ofputil_packet_in& pi,
                            ofputil_protocol& proto,
                            struct dp_packet* pkt,
                            struct flow& flow) {
    uint32_t epgId = (uint32_t)pi.flow_metadata.flow.regs[0];
    PolicyManager& polMgr = agent.getPolicyManager();
    EndpointManager& epMgr = agent.getEndpointManager();
    optional<URI> egUri = polMgr.getGroupForVnid(epgId);
    if (!egUri)
        return;

    unordered_set<string> eps;
    epMgr.getEndpointsForGroup(egUri.get(), eps);
    boost::system::error_code ec;

    MAC srcMac(flow.dl_src.ea);
    shared_ptr<const Endpoint> ep;
    for (const string& epUuid : eps) {
        shared_ptr<const Endpoint> try_ep = epMgr.getEndpoint(epUuid);
        if (!try_ep) continue;
        const optional<MAC>& epMac = try_ep->getMAC();
        if (epMac && srcMac == epMac.get()) {
            ep = try_ep;
            break;
        }
        const Endpoint::virt_ip_set& virtIps =
            try_ep->getVirtualIPs();

        for (const Endpoint::virt_ip_t& virt_ip : virtIps) {
            if (virt_ip.first == srcMac) {
                ep = try_ep;
                goto ep_found;
            }
        }
    }

 ep_found:

    if (!ep) return;
    if (v4)
        handleDHCPv4PktIn(ep, intFlowManager, portMapper,
                          conn, pi, proto, pkt, flow);
    else
        handleDHCPv6PktIn(ep, intFlowManager, conn, pi, proto, pkt, flow);

}

static void handleVIPPktIn(bool v4,
                           Agent& agent,
                           PortMapper& portMapper,
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

    try {
        const std::string& iface =
            portMapper.FindPort(pi.flow_metadata.flow.in_port.ofp_port);

        unordered_set<string> uuids;

        unordered_set<string> try_uuids;
        epMgr.getEndpointsByIface(iface, try_uuids);

        boost::system::error_code ec;

        for (const string& epUuid : try_uuids) {
            shared_ptr<const Endpoint> try_ep = epMgr.getEndpoint(epUuid);
            if (!try_ep) continue;

            for (const Endpoint::virt_ip_t& vip : try_ep->getVirtualIPs()) {
                packets::cidr_t cidr;
                if (!packets::cidr_from_string(vip.second, cidr)) continue;
                if (srcMac == vip.first &&
                    packets::cidr_contains(cidr, srcIp)) {
                    uuids.insert(epUuid);
                    break;
                }
            }
        }

        if (uuids.size() > 0) {
            LOG(DEBUG) << "Virtual IP ownership advertised for ("
                       << srcMac << ", " << srcIp << ")";
            notifServer.dispatchVirtualIp(uuids, srcMac, srcIp.to_string());
        }

    } catch (std::out_of_range) {
        // Ignore announcement coming from stale port
    }
}

static void handleICMPErrPktIn(bool v4,
                               Agent& agent,
                               IntFlowManager& intFlowManager,
                               SwitchConnection *conn,
                               struct ofputil_packet_in& pi,
                               ofputil_protocol& proto,
                               struct dp_packet* pkt) {
    struct ofpbuf* b = NULL;

    uint32_t epgId = (uint32_t)pi.flow_metadata.flow.regs[0];
    optional<URI> egUri = agent.getPolicyManager().getGroupForVnid(epgId);

    if (v4) {
        size_t l4_size = dpp_l4_size(pkt);
        if (l4_size < (sizeof(struct icmphdr) + sizeof(struct iphdr)))
            return;

        char* pkt_data = (char*)dpp_data(pkt);
        b = ofpbuf_clone_data(pkt_data, dpp_size(pkt));
        struct iphdr* outer =
            (struct iphdr*)((char*)dpp_l3(pkt));

        size_t inner_offset = (char*)dpp_l4(pkt)-pkt_data;
        struct icmphdr* inner_icmp =
            (struct icmphdr*)(ofpbuf_at_assert(b, inner_offset,
                                               sizeof(icmphdr)));
        struct iphdr* inner_ip =
            (struct iphdr*)(ofpbuf_at_assert(b, inner_offset +
                                             sizeof(struct icmphdr),
                                             sizeof(iphdr)));
        inner_ip->saddr = outer->daddr;

        // compute checksum
        inner_icmp->checksum = 0;
        uint32_t chksum = 0;
        packets::chksum_accum(chksum, (uint16_t*)inner_icmp, l4_size);
        inner_icmp->checksum = packets::chksum_finalize(chksum);

        LOG(DEBUG) << "Translating ICMPv4 error packet for "
                   << boost::asio::ip::address_v4(htonl(inner_ip->saddr))
                   << " on " << pi.flow_metadata.flow.regs[7];
    }

    if (b)
        send_packet_out(intFlowManager, conn, b, proto,
                        pi.flow_metadata.flow.regs[7],
                        OFPP_IN_PORT, egUri ? egUri.get() : URI::ROOT);
}

/**
 * Dispatch packet-in messages to the appropriate handlers
 */
void PacketInHandler::Handle(SwitchConnection *conn,
                             int msgType, ofpbuf *msg) {
    assert(msgType == OFPTYPE_PACKET_IN);

    const struct ofp_header *oh = (ofp_header *)msg->data;
    struct ofputil_packet_in pi;
    uint32_t pi_buffer_id;

    enum ofperr err = ofputil_decode_packet_in(oh, false, &pi, NULL,
                                               &pi_buffer_id, NULL);
    if (err) {
        LOG(ERROR) << "Failed to decode packet-in: " << ovs_strerror(err);
        return;
    }

    if (pi.reason != OFPR_ACTION)
        return;

    DpPacketP pkt;
    struct flow flow;

    dp_packet_use_const(pkt.ptr, pi.packet, pi.packet_len);
    flow_extract(pkt.ptr, &flow);

    ofputil_protocol proto = ofputil_protocol_from_ofp_version
        ((ofp_version)conn->GetProtocolVersion());
    assert(ofputil_protocol_is_valid(proto));

    if (pi.cookie == flow::cookie::LEARN ||
        pi.cookie == flow::cookie::PROACTIVE_LEARN)
        handleLearnPktIn(conn, pi, pi_buffer_id, proto, pkt.ptr, flow);
    else if (pi.cookie == flow::cookie::NEIGH_DISC)
        handleNDPktIn(agent, intFlowManager, conn, pi, proto, pkt.ptr, flow);
    else if (pi.cookie == flow::cookie::DHCP_V4)
        handleDHCPPktIn(true, agent, intFlowManager, portMapper,
                        conn, pi, proto, pkt.ptr, flow);
    else if (pi.cookie == flow::cookie::DHCP_V6)
        handleDHCPPktIn(false, agent, intFlowManager, portMapper,
                        conn, pi, proto, pkt.ptr, flow);
    else if (pi.cookie == flow::cookie::VIRTUAL_IP_V4)
        handleVIPPktIn(true, agent, *portMapper, pi, flow);
    else if (pi.cookie == flow::cookie::VIRTUAL_IP_V6)
        handleVIPPktIn(false, agent, *portMapper, pi, flow);
    else if (pi.cookie == flow::cookie::ICMP_ERROR_V4)
        handleICMPErrPktIn(true, agent, intFlowManager, conn,
                           pi, proto, pkt.ptr);
}

} /* namespace ovsagent */
