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
#include <boost/bind.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/placeholders.hpp>

#include <modelgbp/gbp/RoutingModeEnumT.hpp>

#include "AdvertManager.h"
#include "Packets.h"
#include "ActionBuilder.h"
#include "IntFlowManager.h"
#include "logging.h"
#include "arp.h"
#include "ovs-ofputil.h"

// OVS lib
#include <lib/util.h>

using std::string;
using boost::bind;
using boost::shared_ptr;
using boost::unordered_set;
using boost::asio::deadline_timer;
using boost::asio::placeholders::error;
using boost::asio::ip::address;
using boost::asio::ip::address_v6;
using boost::posix_time::milliseconds;
using boost::posix_time::seconds;
using boost::unique_lock;
using boost::mutex;
using boost::optional;
using opflex::modb::URI;

namespace ovsagent {

static const address_v6 ALL_NODES_IP(address_v6::from_string("ff02::1"));

AdvertManager::AdvertManager(Agent& agent_,
                             IntFlowManager& intFlowManager_)
    : urng(rng),
      all_ep_gen(urng, boost::random::uniform_int_distribution<>(300,600)),
      repeat_gen(urng, boost::random::uniform_int_distribution<>(3000,5000)),
      sendRouterAdv(false), sendEndpointAdv(EPADV_DISABLED),
      agent(agent_), intFlowManager(intFlowManager_),
      portMapper(NULL), switchConnection(NULL),
      ioService(&agent.getAgentIOService()),
      started(false), stopping(false) {

}

void AdvertManager::start() {
    if (started) return;
    started = true;

    if (sendRouterAdv) {
        routerAdvTimer.reset(new deadline_timer(*ioService));
        scheduleInitialRouterAdv();
    }
    if (sendEndpointAdv != EPADV_DISABLED) {
        allEndpointAdvTimer.reset(new deadline_timer(*ioService));
        endpointAdvTimer.reset(new deadline_timer(*ioService));
        scheduleInitialEndpointAdv();
    }
}

void AdvertManager::stop() {
    stopping = true;

    if (routerAdvTimer)
        routerAdvTimer->cancel();
    if (endpointAdvTimer)
        endpointAdvTimer->cancel();
    if (allEndpointAdvTimer)
        allEndpointAdvTimer->cancel();
}

void AdvertManager::scheduleInitialRouterAdv() {
    initialRouterAdvs = 0;
    if (routerAdvTimer) {
        routerAdvTimer->expires_from_now(milliseconds(250));
        routerAdvTimer->async_wait(bind(&AdvertManager::onRouterAdvTimer,
                                        this, error));
    }
}

void AdvertManager::scheduleInitialEndpointAdv(uint64_t delay) {
    if (allEndpointAdvTimer) {
        allEndpointAdvTimer->expires_from_now(milliseconds(delay));
        allEndpointAdvTimer->
            async_wait(bind(&AdvertManager::onAllEndpointAdvTimer,
                            this, error));
    }
}

void AdvertManager::doScheduleEpAdv(uint64_t time) {
    endpointAdvTimer->expires_from_now(milliseconds(time));
    endpointAdvTimer->
        async_wait(bind(&AdvertManager::onEndpointAdvTimer,
                        this, error));
}

void AdvertManager::scheduleEndpointAdv(const unordered_set<string>& uuids) {
    if (endpointAdvTimer) {
        unique_lock<mutex> guard(ep_mutex);
        BOOST_FOREACH(const string& uuid, uuids)
            pendingEps[uuid] = 5;

        doScheduleEpAdv();
    }
}

void AdvertManager::scheduleEndpointAdv(const std::string& uuid) {
    if (endpointAdvTimer) {
        unique_lock<mutex> guard(ep_mutex);
        pendingEps[uuid] = 5;

        doScheduleEpAdv();
    }
}

static int send_packet_out(IntFlowManager& intFlowManager,
                           SwitchConnection* conn,
                           ofpbuf* b,
                           unordered_set<uint32_t>& out_ports,
                           uint32_t vnid = 0,
                           const URI& egURI = URI::ROOT) {
    struct ofputil_packet_out po;
    po.buffer_id = UINT32_MAX;
    po.packet = b->data;
    po.packet_len = b->size;
    po.in_port = OFPP_CONTROLLER;

    uint32_t tunPort = intFlowManager.getTunnelPort();
    ActionBuilder ab;
    ab.reg(MFF_REG0, vnid);
    if (out_ports.find(tunPort) != out_ports.end()) {
        address tunDst = intFlowManager.getEPGTunnelDst(egURI);
        IntFlowManager::actionTunnelMetadata(ab,
                                             intFlowManager.getEncapType(),
                                             tunDst);
    }

    BOOST_FOREACH(uint32_t p, out_ports) {
        ab.output(p);
    }
    ab.build(&po);

    ofputil_protocol proto = ofputil_protocol_from_ofp_version
        ((ofp_version)conn->GetProtocolVersion());
    assert(ofputil_protocol_is_valid(proto));
    struct ofpbuf* message = ofputil_encode_packet_out(&po, proto);
    int error = conn->SendMessage(message);
    free(po.ofpacts);
    ofpbuf_delete(b);
    return error;
}

void AdvertManager::sendRouterAdvs() {
    LOG(DEBUG) << "Sending all router advertisements";

    EndpointManager& epMgr = agent.getEndpointManager();
    PolicyManager& polMgr = agent.getPolicyManager();

    PolicyManager::uri_set_t epgURIs;
    polMgr.getGroups(epgURIs);

    BOOST_FOREACH(const URI& epg, epgURIs) {
        unordered_set<string> eps;
        unordered_set<uint32_t> out_ports;
        epMgr.getEndpointsForGroup(epg, eps);
        BOOST_FOREACH(const string& uuid, eps) {
            shared_ptr<const Endpoint> ep = epMgr.getEndpoint(uuid);
            if (!ep) continue;

            const boost::optional<std::string>& iface =
                ep->getInterfaceName();
            if (!iface) continue;

            uint32_t port = portMapper->FindPort(iface.get());
            if (port != OFPP_NONE)
                out_ports.insert(port);
        }

        if (out_ports.size() == 0) continue;

        address_v6::bytes_type bytes = ALL_NODES_IP.to_bytes();
        struct ofpbuf* b =
            packets::compose_icmp6_router_ad(intFlowManager.getRouterMacAddr(),
                                             packets::MAC_ADDR_IPV6MULTICAST,
                                             (struct in6_addr*)bytes.data(),
                                             epg, polMgr);
        if (b == NULL) continue;

        int error = send_packet_out(intFlowManager, switchConnection,
                                    b, out_ports);
        if (error) {
            LOG(ERROR) << "Could not write packet-out: "
                       << ovs_strerror(error);
        } else {
            LOG(DEBUG) << "Sent router advertisement for " << epg;
        }
    }
}

void AdvertManager::onRouterAdvTimer(const boost::system::error_code& ec) {
    if (ec)
        return;
    if (!sendRouterAdv)
        return;
    if (!switchConnection)
        return;
    if (!portMapper)
        return;

    int nextInterval = 16;

    if (switchConnection->IsConnected()) {
        agent.getAgentIOService().dispatch(bind(&AdvertManager::sendRouterAdvs,
                                                this));

        if (initialRouterAdvs >= 3)
            nextInterval = 200;
        else
            initialRouterAdvs += 1;

    } else {
        initialRouterAdvs = 0;
    }

    if (!stopping) {
        routerAdvTimer->expires_from_now(seconds(nextInterval));
        routerAdvTimer->async_wait(bind(&AdvertManager::onRouterAdvTimer,
                                        this, error));
    }
}

static void doSendEpAdv(IntFlowManager& intFlowManager,
                        PolicyManager& policyManager,
                        SwitchConnection* switchConnection,
                        const string& ip, const uint8_t* epMac,
                        const uint8_t* routerMac,
                        const URI& egURI, uint32_t epgVnid,
                        unordered_set<uint32_t>& out_ports,
                        AdvertManager::EndpointAdvMode mode) {
    boost::system::error_code ec;
    address addr = address::from_string(ip, ec);
    if (ec) {
        LOG(ERROR) << "Invalid IP address: " << ip
                   << ": " << ec.message();
        return;
    }
    ofpbuf* b = NULL;

    boost::optional<address> routerIp;
    if (mode == AdvertManager::EPADV_ROUTER_REQUEST) {
        boost::optional<boost::shared_ptr<modelgbp::gbp::Subnet> > ipSubnet =
            policyManager.findSubnetForEp(egURI, addr);
        if (ipSubnet)
            routerIp =
                PolicyManager::getRouterIpForSubnet(*ipSubnet.get());

        if (!routerIp) {
            // fall back to gratuitous unicast if we have no gateway
            mode = AdvertManager::EPADV_GRATUITOUS_UNICAST;
        }
    }

    if (addr.is_v4()) {
        uint32_t addrv = addr.to_v4().to_ulong();
        if (mode == AdvertManager::EPADV_ROUTER_REQUEST) {
            // unicast spoofed ARP request for subnet gateway router
            uint32_t routerIpv = routerIp.get().to_v4().to_ulong();

            b = packets::compose_arp(arp::op::REQUEST,
                                     epMac, routerMac,
                                     epMac, packets::MAC_ADDR_ZERO,
                                     addrv, routerIpv);
        } else {
            // unicast or broadcast gratuitous arp
            const uint8_t* dstMac =
                (mode == AdvertManager::EPADV_GRATUITOUS_BROADCAST)
                ? packets::MAC_ADDR_BROADCAST : routerMac;

            b = packets::compose_arp(arp::op::REQUEST,
                                     epMac, dstMac,
                                     epMac, packets::MAC_ADDR_BROADCAST,
                                     addrv, addrv);
        }
    } else {
        address_v6::bytes_type abytes = addr.to_v6().to_bytes();
        struct in6_addr* addrv = (struct in6_addr*)abytes.data();
        if (mode == AdvertManager::EPADV_ROUTER_REQUEST) {
            // unicast spoofed neighbor solicitation for subnet
            // gateway router
            address_v6 routerSNIP =
                packets::construct_solicited_node_ip(routerIp.get().to_v6());
            address_v6::bytes_type dbytes = routerSNIP.to_bytes();
            struct in6_addr* dstv = (struct in6_addr*)dbytes.data();

            address_v6::bytes_type targetbytes =
                routerIp.get().to_v6().to_bytes();
            struct in6_addr* targetv = (struct in6_addr*)targetbytes.data();

            b = packets::compose_icmp6_neigh_solit(epMac, routerMac,
                                                   addrv, dstv, targetv);
        } else {
            // unicast or broadcast gratuitous neighbor advertisement
            const uint8_t* dstMac =
                (mode == AdvertManager::EPADV_GRATUITOUS_BROADCAST)
                ? packets::MAC_ADDR_IPV6MULTICAST : routerMac;
            address_v6::bytes_type rbytes =
                address_v6::from_string("ff02::1").to_bytes();
            struct in6_addr* allnodes = (struct in6_addr*)rbytes.data();

            b = packets::compose_icmp6_neigh_ad(0,
                                                epMac, dstMac,
                                                addrv, allnodes);
        }
    }
    if (b == NULL) return;

    int error = send_packet_out(intFlowManager, switchConnection,
                                b, out_ports, epgVnid, egURI);
    if (error) {
        LOG(ERROR) << "Could not write packet-out: "
                   << ovs_strerror(error);
    }
}

void AdvertManager::sendEndpointAdvs(const string& uuid) {
    uint32_t tunPort = intFlowManager.getTunnelPort();
    if (tunPort == OFPP_NONE) return;
    unordered_set<uint32_t> out_ports;
    out_ports.insert(tunPort);

    EndpointManager& epMgr = agent.getEndpointManager();
    PolicyManager& polMgr = agent.getPolicyManager();

    shared_ptr<const Endpoint> ep = epMgr.getEndpoint(uuid);
    if (!ep) return;
    if (!ep->getMAC()) return;

    optional<URI> epgURI = epMgr.getComputedEPG(uuid);
    if (!epgURI) return;
    optional<uint32_t> epgVnid = polMgr.getVnidForGroup(epgURI.get());
    if (!epgVnid) return;
    if (polMgr.getEffectiveRoutingMode(epgURI.get()) !=
            modelgbp::gbp::RoutingModeEnumT::CONST_ENABLED) {
        return;
    }

    uint8_t epMac[6];
    ep->getMAC().get().toUIntArray(epMac);
    const uint8_t* routerMac = intFlowManager.getRouterMacAddr();

    BOOST_FOREACH(const string& ip, ep->getIPs()) {
        LOG(DEBUG) << "Sending endpoint advertisement for "
                   << ep->getMAC().get() << " " << ip;

        doSendEpAdv(intFlowManager, polMgr, switchConnection,
                    ip, epMac, routerMac, epgURI.get(), epgVnid.get(),
                    out_ports, sendEndpointAdv);

    }

    BOOST_FOREACH (const Endpoint::IPAddressMapping& ipm,
                   ep->getIPAddressMappings()) {
        if (!ipm.getFloatingIP() || !ipm.getMappedIP() || !ipm.getEgURI())
            continue;
        // don't advertise endpoints if there's a next hop
        if (ipm.getNextHopIf())
            continue;

        optional<uint32_t> ipmVnid =
            polMgr.getVnidForGroup(ipm.getEgURI().get());
        if (!ipmVnid) continue;

        LOG(DEBUG) << "Sending endpoint advertisement for "
                   << ep->getMAC().get() << " " << ipm.getFloatingIP().get();

        doSendEpAdv(intFlowManager, polMgr, switchConnection,
                    ipm.getFloatingIP().get(), epMac,
                    routerMac, ipm.getEgURI().get(),
                    ipmVnid.get(), out_ports, sendEndpointAdv);
    }
}

void AdvertManager::sendAllEndpointAdvs() {
    LOG(DEBUG) << "Sending all endpoint advertisements";

    EndpointManager& epMgr = agent.getEndpointManager();
    PolicyManager& polMgr = agent.getPolicyManager();

    PolicyManager::uri_set_t epgURIs;
    polMgr.getGroups(epgURIs);
    BOOST_FOREACH(const URI& epg, epgURIs) {
        if (polMgr.getEffectiveRoutingMode(epg) !=
                modelgbp::gbp::RoutingModeEnumT::CONST_ENABLED) {
            continue;
        }
        unordered_set<string> eps;
        epMgr.getEndpointsForGroup(epg, eps);
        BOOST_FOREACH(const string& uuid, eps) {
            sendEndpointAdvs(uuid);
        }
    }
}

void AdvertManager::onAllEndpointAdvTimer(const boost::system::error_code& ec) {
    if (ec)
        return;
    if (sendEndpointAdv == EPADV_DISABLED)
        return;
    if (!switchConnection)
        return;
    if (!portMapper)
        return;

    if (switchConnection->IsConnected()) {
        agent.getAgentIOService()
            .dispatch(bind(&AdvertManager::sendAllEndpointAdvs, this));
    }

    if (!stopping) {
        allEndpointAdvTimer->expires_from_now(seconds(all_ep_gen()));
        allEndpointAdvTimer->
            async_wait(bind(&AdvertManager::onAllEndpointAdvTimer,
                            this, error));
    }
}

void AdvertManager::onEndpointAdvTimer(const boost::system::error_code& ec) {
    if (ec)
        return;
    if (sendEndpointAdv == EPADV_DISABLED)
        return;
    if (!switchConnection)
        return;
    if (!portMapper)
        return;

    {
        unique_lock<mutex> guard(ep_mutex);
        pending_ep_map_t::iterator it = pendingEps.begin();
        while (it != pendingEps.end()) {
            agent.getAgentIOService()
                .dispatch(bind(&AdvertManager::sendEndpointAdvs, this,
                               it->first));
            if (it->second <= 1) {
                it = pendingEps.erase(it);
            } else {
                it->second -= 1;
                it++;
            }
        }

        if (pendingEps.size() > 0)
            doScheduleEpAdv(repeat_gen());
    }
}

} /* namespace ovsagent */
