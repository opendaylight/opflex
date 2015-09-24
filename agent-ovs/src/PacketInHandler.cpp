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
using std::vector;
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

PacketInHandler::PacketInHandler(Agent& agent_,
                                 FlowManager& flowManager_)
    : agent(agent_), flowManager(flowManager_),
      portMapper(NULL), flowReader(NULL), switchConnection(NULL) {}


bool PacketInHandler::writeLearnFlow(SwitchConnection *conn,
                                     ofputil_protocol& proto,
                                     struct ofputil_packet_in& pi,
                                     struct flow& flow,
                                     bool stage2) {
    uint32_t tunPort = flowManager.GetTunnelPort();
    FlowManager::EncapType encapType = flowManager.GetEncapType();
    const address& tunDst = flowManager.GetTunnelDst();

    bool dstKnown = (0 != pi.flow_metadata.flow.regs[7]);
    if (stage2 && !dstKnown) return false;

    uint8_t* flowMac = NULL;

    struct ofputil_flow_mod fm;
    memset(&fm, 0, sizeof(fm));
    fm.buffer_id = UINT32_MAX;
    fm.table_id = FlowManager::LEARN_TABLE_ID;
    fm.priority = stage2 ? 150 : 100;
    fm.idle_timeout = 300;
    fm.command = OFPFC_ADD;
    fm.new_cookie = FlowManager::GetLearnEntryCookie();

    match_set_reg(&fm.match, 5 /* REG5 */, pi.flow_metadata.flow.regs[5]);
    if (stage2) {
        match_set_in_port(&fm.match, pi.flow_metadata.flow.in_port.ofp_port);
        flowMac = flow.dl_dst;
        match_set_dl_dst(&fm.match, flow.dl_dst);
        match_set_dl_src(&fm.match, flow.dl_src);
    } else {
        flowMac = flow.dl_src;
        match_set_dl_dst(&fm.match, flow.dl_src);
    }

    ActionBuilder ab;
    // Set destination epg == source epg
    ab.SetRegLoad(MFF_REG2, pi.flow_metadata.flow.regs[0]);
    // Set the output register
    uint32_t outport = stage2
        ? pi.flow_metadata.flow.regs[7]
        : pi.flow_metadata.flow.in_port.ofp_port;
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

    if (flowReader) {
        FlowReader::FlowCb dstCb =
            boost::bind(&PacketInHandler::dstFlowCb, this,
                        _1, MAC(flowMac), outport,
                        pi.flow_metadata.flow.regs[5]);
        match fmatch;
        memset(&fmatch, 0, sizeof(fmatch));
        match_set_reg(&fmatch, 5 /* REG5 */, pi.flow_metadata.flow.regs[5]);
        match_set_dl_dst(&fmatch, flowMac);

        flowReader->getFlows(FlowManager::LEARN_TABLE_ID,
                             &fmatch, dstCb);
    }

    return true;
}

static uint32_t getOutputRegValue(const FlowEntryPtr& fe) {
    const struct ofpact* a;
    OFPACT_FOR_EACH (a, fe->entry->ofpacts, fe->entry->ofpacts_len) {
        if (a->type == OFPACT_SET_FIELD) {
            struct ofpact_set_field* sf = ofpact_get_SET_FIELD(a);
            const struct mf_field *mf = sf->field;
            if (mf->id == MFF_REG7) {
                return ntohl(sf->value.be32);
                break;
            }
        }
    }
    return OFPP_NONE;
}

static void removeLearnFlow(SwitchConnection* conn, const FlowEntryPtr& fe) {
    ofputil_protocol proto =
        ofputil_protocol_from_ofp_version(conn->GetProtocolVersion());
    assert(ofputil_protocol_is_valid(proto));

    struct ofputil_flow_mod fm;
    memset(&fm, 0, sizeof(fm));
    fm.table_id = FlowManager::LEARN_TABLE_ID;
    fm.command = OFPFC_DELETE_STRICT;
    fm.priority = fe->entry->priority;
    fm.cookie = FlowManager::GetLearnEntryCookie();
    memcpy(&fm.match, &fe->entry->match, sizeof(fe->entry->match));

    fm.idle_timeout = fe->entry->idle_timeout;
    fm.hard_timeout = fe->entry->hard_timeout;
    fm.buffer_id = UINT32_MAX;
    fm.out_port = OFPP_ANY;
    fm.out_group = OFPG_ANY;
    fm.flags = (ofputil_flow_mod_flags)0;
    fm.delete_reason = OFPRR_DELETE;

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
    BOOST_FOREACH(const FlowEntryPtr& fe, flows) {
        if (fe->entry->cookie == FlowManager::GetLearnEntryCookie()) {
            uint32_t port = getOutputRegValue(fe);
            uint32_t flowFgrpId = fe->entry->match.flow.regs[5];
            MAC flowDstMac(fe->entry->match.flow.dl_dst);
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
    BOOST_FOREACH(const FlowEntryPtr& fe, flows) {
        if (fe->entry->cookie == FlowManager::GetLearnEntryCookie() &&
            !reconcileReactiveFlow(fe)) {
            removeLearnFlow(switchConnection, fe);
        }
    }
}

bool PacketInHandler::reconcileReactiveFlow(const FlowEntryPtr& fe) {
    EndpointManager& epMgr = agent.getEndpointManager();

    if (!portMapper) return true;
    if (fe->entry->cookie != FlowManager::GetLearnEntryCookie())
        return false;   // non-reactive entries must be reconciled

    uint32_t port = getOutputRegValue(fe);
    if (port == flowManager.GetTunnelPort())
        return true;

    MAC dstMac(fe->entry->match.flow.dl_dst);

    try {
        const string& portName = portMapper->FindPort(port);
        unordered_set<string> eps;
        epMgr.getEndpointsByIface(portName, eps);
        BOOST_FOREACH(const string& uuid, eps) {
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

void PacketInHandler::portStatusUpdate(const string& portName,
                                       uint32_t portNo,
                                       bool fromDesc) {
    if (fromDesc) return;
    if (flowReader && portMapper) {
        FlowReader::FlowCb cb =
            boost::bind(&PacketInHandler::anyFlowCb, this, _1);
        flowReader->getFlows(FlowManager::LEARN_TABLE_ID, cb);
    }
}


void PacketInHandler::handleLearnPktIn(SwitchConnection *conn,
                                       struct ofputil_packet_in& pi,
                                       ofputil_protocol& proto,
                                       struct dp_packet& pkt,
                                       struct flow& flow) {
    writeLearnFlow(conn, proto, pi, flow, false);
    if (writeLearnFlow(conn, proto, pi, flow, true))
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

        match_set_in_port(&fm.match, pi.flow_metadata.flow.in_port.ofp_port);
        match_set_reg(&fm.match, 5 /* REG5 */, pi.flow_metadata.flow.regs[5]);
        match_set_dl_src(&fm.match, flow.dl_src);

        ActionBuilder ab;
        ab.SetGroup(FlowManager::getPromId(pi.flow_metadata.flow.regs[5]));
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
        po.packet = dp_packet_data(&pkt);
        po.packet_len = dp_packet_size(&pkt);
        po.in_port = pi.flow_metadata.flow.in_port.ofp_port;

        ActionBuilder ab;
        ab.SetRegLoad(MFF_REG0, pi.flow_metadata.flow.regs[0]);
        ab.SetGroup(FlowManager::getPromId(pi.flow_metadata.flow.regs[5]));
        ab.Build(&po);

        struct ofpbuf* message = ofputil_encode_packet_out(&po, proto);
        int error = conn->SendMessage(message);
        free(po.ofpacts);
        if (error) {
            LOG(ERROR) << "Could not write packet-out: " << ovs_strerror(error);
        }
    }
}

static void send_packet_out(FlowManager& flowManager,
                            SwitchConnection *conn,
                            struct ofpbuf* b,
                            ofputil_protocol& proto,
                            uint32_t in_port,
                            uint32_t out_port = OFPP_IN_PORT) {
    // send reply as packet-out
    struct ofputil_packet_out po;
    po.buffer_id = UINT32_MAX;
    po.packet = b->data;
    po.packet_len = b->size;
    po.in_port = in_port;

    uint32_t tunPort = flowManager.GetTunnelPort();

    ActionBuilder ab;
    if (out_port == tunPort || (in_port == tunPort && out_port == OFPP_IN_PORT))
        FlowManager::SetActionTunnelMetadata(ab, flowManager.GetEncapType(),
                                             flowManager.GetTunnelDst());
    ab.SetOutputToPort(out_port);
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
 * @param flowManager the flow manager
 * @param conn the openflow switch connection
 * @param pi the packet-in
 * @param proto an openflow proto object
 * @param pkt the packet from the packet-in
 * @param flow the parsed flow from the packet
 */
static void handleNDPktIn(Agent& agent,
                          FlowManager& flowManager,
                          SwitchConnection *conn,
                          struct ofputil_packet_in& pi,
                          ofputil_protocol& proto,
                          struct dp_packet& pkt,
                          struct flow& flow) {
    uint32_t epgId = (uint32_t)pi.flow_metadata.flow.regs[0];
    PolicyManager& polMgr = agent.getPolicyManager();
    optional<URI> egUri = polMgr.getGroupForVnid(epgId);
    if (!egUri)
        return;

    if (flow.dl_type != htons(ETH_TYPE_IPV6) ||
        flow.nw_proto != 58 /* ICMPv6 */)
        return;

    size_t l4_size = dp_packet_l4_size(&pkt);
    if (l4_size < sizeof(struct icmp6_hdr))
        return;
    struct icmp6_hdr* icmp = (struct icmp6_hdr*) dp_packet_l4(&pkt);
    if (icmp->icmp6_code != 0)
        return;

    struct ofpbuf* b = NULL;

    const uint8_t* mac = flowManager.GetRouterMacAddr();
    bool router = true;
    // Use the MAC address from the metadata if available
    uint64_t metadata = ntohll(pi.flow_metadata.flow.metadata);
    if (((uint8_t*)&metadata)[7] == 1) {
        mac = (uint8_t*)&metadata;
        router = mac[6] == 1;
    }

    if (icmp->icmp6_type == ND_NEIGHBOR_SOLICIT) {
        /* Neighbor solicitation */
        struct nd_neighbor_solicit* neigh_sol =
            (struct nd_neighbor_solicit*) dp_packet_l4(&pkt);
        if (l4_size < sizeof(*neigh_sol))
            return;

        LOG(DEBUG) << "Handling ICMPv6 neighbor solicitation";
        uint32_t flags = ND_NA_FLAG_SOLICITED | ND_NA_FLAG_OVERRIDE;
        if (router) flags |= ND_NA_FLAG_ROUTER;

        b = packets::compose_icmp6_neigh_ad(flags,
                                            mac,
                                            flow.dl_src,
                                            &neigh_sol->nd_ns_target,
                                            &flow.ipv6_src);

    } else if (icmp->icmp6_type == ND_ROUTER_SOLICIT) {
        /* Router solicitation */
        struct nd_router_solicit* router_sol =
            (struct nd_router_solicit*) dp_packet_l4(&pkt);
        if (l4_size < sizeof(*router_sol))
            return;

        LOG(DEBUG) << "Handling ICMPv6 router solicitation";

        b = packets::compose_icmp6_router_ad(mac,
                                             flow.dl_src,
                                             &flow.ipv6_src,
                                             egUri.get(),
                                             polMgr);
    }

    if (b)
        send_packet_out(flowManager, conn, b, proto,
                        pi.flow_metadata.flow.in_port.ofp_port);
}

static void handleDHCPv4PktIn(shared_ptr<const Endpoint>& ep,
                              FlowManager& flowManager,
                              SwitchConnection *conn,
                              struct ofputil_packet_in& pi,
                              ofputil_protocol& proto,
                              struct dp_packet& pkt,
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

    size_t l4_size = dp_packet_l4_size(&pkt);
    if (l4_size < (sizeof(struct udp_hdr) + sizeof(struct dhcp_hdr) + 1))
        return;
    struct dhcp_hdr* dhcp_pkt =
        (struct dhcp_hdr*) ((char*)dp_packet_l4(&pkt) + sizeof(struct udp_hdr));
    if (dhcp_pkt->op != 1 ||
        dhcp_pkt->htype != 1 ||
        dhcp_pkt->hlen != 6 ||
        dhcp_pkt->cookie[0] != 99 ||
        dhcp_pkt->cookie[1] != 130 ||
        dhcp_pkt->cookie[2] != 83 ||
        dhcp_pkt->cookie[3] != 99)
        return;

    uint8_t message_type = 0;
    address_v4 requested_ip;

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
    MAC srcMac(flow.dl_src);

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
                         << requested_ip << " from " << srcMac;
        }
        break;
    case message_type::DISCOVER:
        LOG(DEBUG) << "Handling DHCP DISCOVER from " << srcMac;
        reply_type = message_type::OFFER;
        break;
    default:
        LOG(DEBUG) << "Ignoring unexpected DHCP message of type "
                   << message_type << " from " << srcMac;
        return;
    }

    b = packets::compose_dhcpv4_reply(reply_type,
                                      dhcp_pkt->xid,
                                      flowManager.GetDHCPMacAddr(),
                                      flow.dl_src,
                                      dhcpIp.to_ulong(),
                                      prefixLen,
                                      v4c.get().getRouters(),
                                      v4c.get().getDnsServers(),
                                      v4c.get().getDomain(),
                                      v4c.get().getStaticRoutes());

    if (b)
        send_packet_out(flowManager, conn, b, proto,
                        pi.flow_metadata.flow.in_port.ofp_port);
}

static void handleDHCPv6PktIn(shared_ptr<const Endpoint>& ep,
                              FlowManager& flowManager,
                              SwitchConnection *conn,
                              struct ofputil_packet_in& pi,
                              ofputil_protocol& proto,
                              struct dp_packet& pkt,
                              struct flow& flow) {
    using namespace dhcp6;
    using namespace udp;

    const optional<Endpoint::DHCPv6Config>& v6c = ep->getDHCPv6Config();
    if (!v6c) return;

    const unordered_set<string>& addresses = ep->getIPs();

    boost::system::error_code ec;
    vector<address_v6> v6addresses;
    BOOST_FOREACH(const string& addrStr, addresses) {
        address_v6 addr_v6 = address_v6::from_string(addrStr, ec);
        if (ec) continue;
        v6addresses.push_back(addr_v6);
    }

    size_t l4_size = dp_packet_l4_size(&pkt);
    if (l4_size < (sizeof(struct udp_hdr) + sizeof(struct dhcp6_hdr) + 1))
        return;
    struct dhcp6_hdr* dhcp_pkt =
        (struct dhcp6_hdr*)((char*)dp_packet_l4(&pkt) + sizeof(struct udp_hdr));

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
    MAC srcMac(flow.dl_src);

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
                                      flowManager.GetDHCPMacAddr(),
                                      flow.dl_src,
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
        send_packet_out(flowManager, conn, b, proto,
                        pi.flow_metadata.flow.in_port.ofp_port);
}


/**
 * Handle a packet-in for DHCP messages.  The reply is written as a
 * packet-out to the connection
 *
 * @param v4 true if this is a DHCPv4 message, or false for DHCPv6
 * @param agent the agent object
 * @param flowManager the flow manager
 * @param conn the openflow switch connection
 * @param pi the packet-in
 * @param proto an openflow proto object
 * @param pkt the packet from the packet-in
 * @param flow the parsed flow from the packet
 * @param virtualDhcpMac the MAC address of the virtual DHCP server
 */
static void handleDHCPPktIn(bool v4,
                            Agent& agent,
                            FlowManager& flowManager,
                            SwitchConnection *conn,
                            struct ofputil_packet_in& pi,
                            ofputil_protocol& proto,
                            struct dp_packet& pkt,
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
        handleDHCPv4PktIn(ep, flowManager, conn, pi, proto, pkt, flow);
    else
        handleDHCPv6PktIn(ep, flowManager, conn, pi, proto, pkt, flow);

}

static void handleVIPPktIn(bool v4,
                           Agent& agent,
                           FlowManager& flowManager,
                           SwitchConnection *conn,
                           struct ofputil_packet_in& pi,
                           ofputil_protocol& proto,
                           struct dp_packet& pkt,
                           struct flow& flow) {
    EndpointManager& epMgr = agent.getEndpointManager();
    NotifServer& notifServer = agent.getNotifServer();
    MAC srcMac(flow.dl_src);
    address srcIp;

    if (v4) {
        // ARP reply
        if (flow.dl_type != htons(ETH_TYPE_ARP))
            return;

        srcIp = address_v4(ntohl(flow.nw_src));
    } else {
        // Neighbor advertisement
        if (flow.dl_type != htons(ETH_TYPE_IPV6) ||
            flow.nw_proto != 58 /* ICMPv6 */)
            return;

        address_v6::bytes_type bytes;
        std::memcpy(bytes.data(), &flow.nd_target, bytes.size());
        srcIp = address_v6(bytes);
    }

    try {
        const std::string& iface =
            flowManager.GetPortMapper()->
            FindPort(pi.flow_metadata.flow.in_port.ofp_port);

        unordered_set<string> uuids;

        unordered_set<string> try_uuids;
        epMgr.getEndpointsByIface(iface, try_uuids);

        boost::system::error_code ec;

        BOOST_FOREACH(const string& epUuid, try_uuids) {
            shared_ptr<const Endpoint> try_ep = epMgr.getEndpoint(epUuid);
            if (!try_ep) continue;

            BOOST_FOREACH(const Endpoint::virt_ip_t& vip,
                          try_ep->getVirtualIPs()) {
                address addr = address::from_string(vip.second, ec);
                if (ec) continue;
                if (srcMac == vip.first && srcIp == addr) {
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

/**
 * Dispatch packet-in messages to the appropriate handlers
 */
void PacketInHandler::Handle(SwitchConnection *conn,
                             ofptype msgType, ofpbuf *msg) {
    assert(msgType == OFPTYPE_PACKET_IN);

    const struct ofp_header *oh = (ofp_header *)msg->data;
    struct ofputil_packet_in pi;

    enum ofperr err = ofputil_decode_packet_in(&pi, oh);
    if (err) {
        LOG(ERROR) << "Failed to decode packet-in: " << ovs_strerror(err);
        return;
    }

    if (pi.reason != OFPR_ACTION)
        return;

    struct dp_packet pkt;
    struct flow flow;
    dp_packet_use_const(&pkt, pi.packet, pi.packet_len);
    flow_extract(&pkt, &flow);

    ofputil_protocol proto =
        ofputil_protocol_from_ofp_version(conn->GetProtocolVersion());
    assert(ofputil_protocol_is_valid(proto));

    if (pi.cookie == FlowManager::GetLearnEntryCookie() ||
        pi.cookie == FlowManager::GetProactiveLearnEntryCookie())
        handleLearnPktIn(conn, pi, proto, pkt, flow);
    else if (pi.cookie == FlowManager::GetNDCookie())
        handleNDPktIn(agent, flowManager, conn, pi, proto, pkt, flow);
    else if (pi.cookie == FlowManager::GetDHCPCookie(true))
        handleDHCPPktIn(true, agent, flowManager, conn, pi, proto, pkt, flow);
    else if (pi.cookie == FlowManager::GetDHCPCookie(false))
        handleDHCPPktIn(false, agent, flowManager, conn, pi, proto, pkt, flow);
    else if (pi.cookie == FlowManager::GetVIPCookie(true))
        handleVIPPktIn(true, agent, flowManager, conn, pi, proto, pkt, flow);
    else if (pi.cookie == FlowManager::GetVIPCookie(false))
        handleVIPPktIn(false, agent, flowManager, conn, pi, proto, pkt, flow);
}

} /* namespace ovsagent */
