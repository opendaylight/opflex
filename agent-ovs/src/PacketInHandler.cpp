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

#include <boost/foreach.hpp>
#include <boost/system/error_code.hpp>

#include "PacketInHandler.h"
#include "Packets.h"
#include "ActionBuilder.h"
#include "logging.h"
#include "dhcp.h"
#include "udp.h"
#include "FlowManager.h"

using std::string;
using boost::asio::ip::address;
using boost::asio::ip::address_v6;
using boost::asio::ip::address_v4;
using boost::unordered_map;
using boost::unordered_set;
using boost::shared_ptr;
using boost::optional;
using opflex::modb::MAC;
using opflex::modb::URI;
using modelgbp::gbp::Subnet;

namespace ovsagent {

PacketInHandler::PacketInHandler(Agent& agent_, FlowManager& flowManager_)
    : agent(agent_), flowManager(flowManager_) {}


static bool writeLearnFlow(SwitchConnection *conn, 
                           ofputil_protocol& proto,
                           struct ofputil_packet_in& pi,
                           struct flow& flow,
                           bool stage2,
                           uint32_t tunPort,
                           FlowManager::EncapType encapType,
                           const address& tunDst) {
    bool dstKnown = (0 != pi.fmd.regs[7]);
    if (stage2 && !dstKnown) return false;

    struct ofputil_flow_mod fm;
    memset(&fm, 0, sizeof(fm));
    fm.buffer_id = UINT32_MAX;
    fm.table_id = FlowManager::LEARN_TABLE_ID;
    fm.priority = stage2 ? 150 : 100;
    fm.idle_timeout = 300;
    fm.command = OFPFC_ADD;
    fm.new_cookie = FlowManager::GetLearnEntryCookie();

    match_set_reg(&fm.match, 5 /* REG5 */, pi.fmd.regs[5]);
    if (stage2) {
        match_set_dl_dst(&fm.match, flow.dl_dst);
        match_set_dl_src(&fm.match, flow.dl_src);
    } else {
        match_set_dl_dst(&fm.match, flow.dl_src);
    }

    ActionBuilder ab;
    // Set destination epg == source epg
    ab.SetRegLoad(MFF_REG2, pi.fmd.regs[0]);
    // Set the output register
    uint32_t outport = stage2 ? pi.fmd.regs[7] : pi.fmd.in_port;
    if (outport == tunPort)
        FlowManager::SetActionTunnelMetadata(ab, encapType, tunDst);

    ab.SetRegLoad(MFF_REG7, outport);
    if (stage2) {
        ab.SetGotoTable(FlowManager::POL_TABLE_ID);
    } else {
        ab.SetOutputToPort(outport);
        ab.SetController();
    }
    ab.Build(&fm);

    struct ofpbuf * message = ofputil_encode_flow_mod(&fm, proto);
    int error = conn->SendMessage(message);
    free(fm.ofpacts);
    if (error) {
        LOG(ERROR) << "Could not write flow mod: " << ovs_strerror(error);
    }
    return true;
}


/**
 * Handle a packet-in for the learning table.  Perform MAC learning
 * for flood domains that require flooding for unknown unicast
 * traffic.  Note that this will only manage the reactive flows
 * associated with MAC learning; there are static flows to enabling
 * MAC learning elsewhere.  Any resulting flows or packet-out messages
 * are written directly to the connection.
 *
 * @param conn the openflow switch connection
 * @param pi the packet-in
 * @param proto an openflow proto object
 * @param pkt the packet from the packet-in
 * @param flow the parsed flow from the packet
 * @param tunPort the uplink tunnel port
 * @param encapType the type of encapsulation in use
 * @param tunDst the remote tunnel destination for encap types that
 * require it
 */
static void handleLearnPktIn(SwitchConnection *conn,
                             struct ofputil_packet_in& pi,
                             ofputil_protocol& proto,
                             struct ofpbuf& pkt,
                             struct flow& flow,
                             uint32_t tunPort,
                             FlowManager::EncapType encapType,
                             const address& tunDst) {
    writeLearnFlow(conn, proto, pi, flow, false, 
                   tunPort, encapType, tunDst);
    if (writeLearnFlow(conn, proto, pi, flow, true, 
                       tunPort, encapType, tunDst))
        return;

    {
        // install a forward flow to flood to the promiscuous ports in
        // the flood domain until we learn the correct reverse path
        struct ofputil_flow_mod fm;
        memset(&fm, 0, sizeof(fm));
        fm.buffer_id = pi.buffer_id;
        fm.table_id = FlowManager::LEARN_TABLE_ID;
        fm.priority = 50;
        fm.idle_timeout = 5;
        fm.hard_timeout = 60;
        fm.command = OFPFC_ADD;
        fm.new_cookie = FlowManager::GetLearnEntryCookie();

        match_set_in_port(&fm.match, pi.fmd.in_port);
        match_set_reg(&fm.match, 5 /* REG5 */, pi.fmd.regs[5]);
        match_set_dl_src(&fm.match, flow.dl_src);

        ActionBuilder ab;
        ab.SetGroup(FlowManager::getPromId(pi.fmd.regs[5]));
        ab.Build(&fm);

        struct ofpbuf* message = ofputil_encode_flow_mod(&fm, proto);
        int error = conn->SendMessage(message);
        free(fm.ofpacts);
        if (error) {
            LOG(ERROR) << "Could not write flow mod: " << ovs_strerror(error);
        }
    }

    if (pi.buffer_id == UINT32_MAX) {
        // Send packet out if needed
        struct ofputil_packet_out po;
        po.buffer_id = UINT32_MAX;
        po.packet = ofpbuf_data(&pkt);
        po.packet_len = ofpbuf_size(&pkt);
        po.in_port = pi.fmd.in_port;

        ActionBuilder ab;
        ab.SetGroup(FlowManager::getPromId(pi.fmd.regs[5]));
        ab.Build(&po);

        struct ofpbuf* message = ofputil_encode_packet_out(&po, proto);
        int error = conn->SendMessage(message);
        free(po.ofpacts);
        if (error) {
            LOG(ERROR) << "Could not write packet-out: " << ovs_strerror(error);
        }
    }
}

static void send_packet_out(SwitchConnection *conn,
                            struct ofpbuf* b,
                            ofputil_protocol& proto,
                            uint32_t in_port) {
    // send reply as packet-out
    struct ofputil_packet_out po;
    po.buffer_id = UINT32_MAX;
    po.packet = ofpbuf_data(b);
    po.packet_len = ofpbuf_size(b);
    po.in_port = in_port;

    ActionBuilder ab;
    ab.SetOutputToPort(OFPP_IN_PORT);
    ab.Build(&po);

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
 * @param conn the openflow switch connection
 * @param pi the packet-in
 * @param proto an openflow proto object
 * @param pkt the packet from the packet-in
 * @param flow the parsed flow from the packet
 * @param virtualRouterMac the MAC address of the virtual router
 */
static void handleNDPktIn(Agent& agent,
                          SwitchConnection *conn,
                          struct ofputil_packet_in& pi,
                          ofputil_protocol& proto,
                          struct ofpbuf& pkt,
                          struct flow& flow,
                          const uint8_t* virtualRouterMac) {
    uint32_t epgId = (uint32_t)pi.fmd.regs[0];
    PolicyManager& polMgr = agent.getPolicyManager();
    optional<URI> egUri = polMgr.getGroupForVnid(epgId);
    if (!egUri)
        return;

    if (flow.dl_type != htons(ETH_TYPE_IPV6) ||
        flow.nw_proto != 58 /* ICMPv6 */)
        return;

    size_t l4_size = ofpbuf_l4_size(&pkt);
    if (l4_size < sizeof(struct icmp6_hdr))
        return;
    struct icmp6_hdr* icmp = (struct icmp6_hdr*) ofpbuf_l4(&pkt);
    if (icmp->icmp6_code != 0)
        return;

    struct ofpbuf* b = NULL;

    if (icmp->icmp6_type == ND_NEIGHBOR_SOLICIT) {
        /* Neighbor solicitation */
        struct nd_neighbor_solicit* neigh_sol = 
            (struct nd_neighbor_solicit*) ofpbuf_l4(&pkt);
        if (l4_size < sizeof(*neigh_sol))
            return;

        LOG(DEBUG) << "Handling ICMPv6 neighbor solicitation";

        // we only get neighbor solicitation packets for our router
        // IP, so we just always reply with our router MAC to the
        // requested IP.
        b = packets::compose_icmp6_neigh_ad(ND_NA_FLAG_ROUTER | 
                                            ND_NA_FLAG_SOLICITED | 
                                            ND_NA_FLAG_OVERRIDE,
                                            virtualRouterMac,
                                            flow.dl_src,
                                            &neigh_sol->nd_ns_target,
                                            &flow.ipv6_src);

    } else if (icmp->icmp6_type == ND_ROUTER_SOLICIT) {
        /* Router solicitation */
        struct nd_router_solicit* router_sol = 
            (struct nd_router_solicit*) ofpbuf_l4(&pkt);
        if (l4_size < sizeof(*router_sol))
            return;

        LOG(DEBUG) << "Handling ICMPv6 router solicitation";

        b = packets::compose_icmp6_router_ad(virtualRouterMac,
                                             flow.dl_src,
                                             &flow.ipv6_src,
                                             egUri.get(),
                                             polMgr);
    }

    if (b)
        send_packet_out(conn, b, proto, pi.fmd.in_port);
}

static void handleDHCPv4PktIn(shared_ptr<const Endpoint>& ep,
                              Agent& agent,
                              SwitchConnection *conn,
                              struct ofputil_packet_in& pi,
                              ofputil_protocol& proto,
                              struct ofpbuf& pkt,
                              struct flow& flow,
                              const uint8_t* virtualDhcpMac) {
    using namespace dhcp;
    using namespace udp;

    size_t l4_size = ofpbuf_l4_size(&pkt);
    if (l4_size < (sizeof(struct udp_hdr) + sizeof(struct dhcp_hdr) + 1))
        return;
    struct dhcp_hdr* dhcp_pkt =
        (struct dhcp_hdr*) ((char*)ofpbuf_l4(&pkt) + sizeof(struct udp_hdr));
    if (dhcp_pkt->op != 1 ||
        dhcp_pkt->htype != 1 ||
        dhcp_pkt->hlen != 6 ||
        dhcp_pkt->cookie[0] != 99 ||
        dhcp_pkt->cookie[1] != 130 ||
        dhcp_pkt->cookie[2] != 83 ||
        dhcp_pkt->cookie[3] != 99)
        return;

    uint8_t message_type = 0;
    address requested_ip;

    char* cur = (char*)dhcp_pkt + sizeof(struct dhcp_hdr);
    size_t remaining = l4_size - sizeof(struct dhcp_hdr);
    while (remaining > 0) {
        struct dhcp_option_hdr* hdr = (struct dhcp_option_hdr*)cur;
        if (hdr->code == DHCP_OPTION_END)
            break;
        if (hdr->code == DHCP_OPTION_PAD) {
            cur += 1;
            remaining -= 1;
            continue;
        }

        if (remaining <= ((size_t)hdr->len + 2))
            break;
        switch (hdr->code) {
        case DHCP_OPTION_MESSAGE_TYPE:
            message_type = ((uint8_t*)cur)[2];
            break;
        case DHCP_OPTION_REQUESTED_IP:
            requested_ip = address_v4(ntohl(*((uint32_t*)((uint8_t*)cur + 2))));
        }

        cur += hdr->len + 2;
        remaining -= hdr->len + 2;
    }

    if (message_type != DHCP_MESSAGE_TYPE_DISCOVER &&
        message_type != DHCP_MESSAGE_TYPE_REQUEST)
        return;

    struct ofpbuf* b = NULL;
    boost::system::error_code ec;
    MAC srcMac(flow.dl_src);

    BOOST_FOREACH(const Endpoint::DHCPConfig& dhcpConfig,
                  ep->getDhcpConfig()) {
        optional<string> dhcpIpStr = dhcpConfig.getIpAddress();
        if (!dhcpIpStr) continue;
        address dhcpIp = address::from_string(dhcpIpStr.get(), ec);
        if (ec) continue;
        if (dhcpIp.is_v6()) continue;

        uint8_t prefixLen = dhcpConfig.getPrefixLen().get_value_or(32);

        uint8_t reply_type = DHCP_MESSAGE_TYPE_NAK;

        switch(message_type) {
        case DHCP_MESSAGE_TYPE_REQUEST:
            if (requested_ip == dhcpIp) {
                LOG(DEBUG) << "Handling DHCP REQUEST for IP "
                           << requested_ip << " from " << srcMac;
                reply_type = DHCP_MESSAGE_TYPE_ACK;
            } else {
                LOG(WARNING) << "Rejecting DHCP REQUEST for IP "
                             << requested_ip << " from " << srcMac;
            }

            break;
        case DHCP_MESSAGE_TYPE_DISCOVER:
            LOG(DEBUG) << "Handling DHCP DISCOVER" << " from " << srcMac;
            reply_type = DHCP_MESSAGE_TYPE_OFFER;
        default:
            break;
        }

        b = packets::compose_dhcpv4_reply(reply_type,
                                          dhcp_pkt->xid,
                                          virtualDhcpMac,
                                          flow.dl_src,
                                          dhcpIp.to_v4().to_ulong(),
                                          prefixLen,
                                          dhcpConfig.getRouters(),
                                          dhcpConfig.getDnsServers(),
                                          dhcpConfig.getDomain(),
                                          dhcpConfig.getStaticRoutes());

        // Only one IP address can come from DHCPv4
        break;
    }

    if (b)
        send_packet_out(conn, b, proto, pi.fmd.in_port);
}

static void handleDHCPv6PktIn(shared_ptr<const Endpoint>& ep,
                              Agent& agent,
                              SwitchConnection *conn,
                              struct ofputil_packet_in& pi,
                              ofputil_protocol& proto,
                              struct ofpbuf& pkt,
                              struct flow& flow,
                              const uint8_t* virtualDhcpMac) {
    using namespace dhcp;

}


/**
 * Handle a packet-in for DHCP messages messages.  The
 * reply is written as a packet-out to the connection
 *
 * @param v4 true if this is a DHCPv4 message, or false for DHCPv6
 * @param agent the agent object
 * @param conn the openflow switch connection
 * @param pi the packet-in
 * @param proto an openflow proto object
 * @param pkt the packet from the packet-in
 * @param flow the parsed flow from the packet
 * @param virtualDhcpMac the MAC address of the virtual DHCP server
 */
static void handleDHCPPktIn(bool v4,
                            Agent& agent,
                            SwitchConnection *conn,
                            struct ofputil_packet_in& pi,
                            ofputil_protocol& proto,
                            struct ofpbuf& pkt,
                            struct flow& flow,
                            const uint8_t* virtualDhcpMac) {
    uint32_t epgId = (uint32_t)pi.fmd.regs[0];
    PolicyManager& polMgr = agent.getPolicyManager();
    EndpointManager& epMgr = agent.getEndpointManager();
    optional<URI> egUri = polMgr.getGroupForVnid(epgId);
    if (!egUri)
        return;
    
    unordered_set<string> eps;
    epMgr.getEndpointsForGroup(egUri.get(), eps);
    boost::system::error_code ec;

    MAC srcMac(flow.dl_src);
    shared_ptr<const Endpoint> ep;
    BOOST_FOREACH(const string& epUuid, eps) {
        shared_ptr<const Endpoint> try_ep = epMgr.getEndpoint(epUuid);
        if (!try_ep) continue;
        const optional<MAC>& epMac = try_ep->getMAC();
        if (epMac && srcMac == epMac.get()) {
            ep = try_ep;
            break;
        }
    }

    if (!ep) return;
    if (v4)
        handleDHCPv4PktIn(ep, agent, conn, pi, proto,
                          pkt, flow, virtualDhcpMac);
    else
        handleDHCPv6PktIn(ep, agent, conn, pi, proto,
                          pkt, flow, virtualDhcpMac);

}


/**
 * Dispatch packet-in messages to the appropriate handlers
 */
void PacketInHandler::Handle(SwitchConnection *conn,
                             ofptype msgType, ofpbuf *msg) {
    assert(msgType == OFPTYPE_PACKET_IN);

    const struct ofp_header *oh = (ofp_header *)ofpbuf_data(msg);
    struct ofputil_packet_in pi;

    enum ofperr err = ofputil_decode_packet_in(&pi, oh);
    if (err) {
        LOG(ERROR) << "Failed to decode packet-in: " << ovs_strerror(err);
        return;
    }

    if (pi.reason != OFPR_ACTION)
        return;

    struct ofpbuf pkt;
    struct flow flow;
    ofpbuf_use_const(&pkt, pi.packet, pi.packet_len);
    flow_extract(&pkt, NULL, &flow);

    ofputil_protocol proto =
        ofputil_protocol_from_ofp_version(conn->GetProtocolVersion());
    assert(ofputil_protocol_is_valid(proto));

    if (pi.cookie == FlowManager::GetLearnEntryCookie() ||
        pi.cookie == FlowManager::GetProactiveLearnEntryCookie())
        handleLearnPktIn(conn, pi, proto, pkt, flow,
                         flowManager.GetTunnelPort(),
                         flowManager.GetEncapType(),
                         flowManager.GetTunnelDst());
    else if (pi.cookie == FlowManager::GetNDCookie())
        handleNDPktIn(agent, conn, pi, proto, pkt,
                      flow, flowManager.GetRouterMacAddr());
    else if (pi.cookie == FlowManager::GetDHCPCookie(true))
        handleDHCPPktIn(true, agent, conn, pi,
                        proto, pkt, flow, flowManager.GetDHCPMacAddr());
    else if (pi.cookie == FlowManager::GetDHCPCookie(false))
        handleDHCPPktIn(false, agent, conn, pi,
                        proto, pkt, flow, flowManager.GetDHCPMacAddr());
}

} /* namespace ovsagent */

