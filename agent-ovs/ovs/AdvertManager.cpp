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
#include <config.h>
#ifdef  __linux__
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <linux/if_ether.h>
#endif
#include "AdvertManager.h"
#include "Packets.h"
#include "ActionBuilder.h"
#include "IntFlowManager.h"
#include <opflexagent/logging.h>
#include "arp.h"

#include <boost/system/error_code.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/placeholders.hpp>

#include <modelgbp/gbp/RoutingModeEnumT.hpp>

#include "ovs-ofputil.h"

// OVS lib
#include <lib/util.h>

using std::string;
using std::shared_ptr;
using std::unordered_set;
using std::unique_lock;
using std::mutex;
using std::bind;
using std::placeholders::_1;
using std::placeholders::_2;
using boost::asio::deadline_timer;
using boost::asio::placeholders::error;
using boost::asio::ip::address;
using boost::asio::ip::address_v6;
using boost::posix_time::milliseconds;
using boost::posix_time::seconds;
using boost::optional;
using opflex::modb::URI;

namespace opflexagent {

static const address_v6 ALL_NODES_IP(address_v6::from_string("ff02::1"));

AdvertManager::AdvertManager(Agent& agent_,
                             IntFlowManager& intFlowManager_)
    : urng(rng()), all_ep_dis(300,600), repeat_dis(3000,5000),
      sendRouterAdv(false), sendEndpointAdv(EPADV_DISABLED),
      agent(agent_), intFlowManager(intFlowManager_),
      portMapper(NULL), switchConnection(NULL),
      ioService(&agent.getAgentIOService()),
      started(false), stopping(false) {
    //TunnelEpTimer has no dependencies and needs to be
    //initialized early
    tunnelEpAdvTimer.reset(new deadline_timer(*ioService));

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
    if(tunnelEpAdvTimer)
        tunnelEpAdvTimer->cancel();
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
        for (const string& uuid : uuids)
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

void AdvertManager::scheduleServiceAdv(const std::string& uuid) {
    if (endpointAdvTimer) {
        unique_lock<mutex> guard(ep_mutex);
        pendingServices[uuid] = 5;

        doScheduleEpAdv();
    }
}

static int send_packet_out(SwitchConnection* conn,
                           OfpBuf& b,
                           unordered_set<uint32_t>& out_ports,
                           IntFlowManager::EncapType encapType =
                           IntFlowManager::ENCAP_NONE,
                           uint32_t vnid = 0,
                           address tunDst = address()) {
    struct ofputil_packet_out po;
    po.buffer_id = UINT32_MAX;
    po.packet = b.data();
    po.packet_len = b.size();
    po.in_port = OFPP_CONTROLLER;

    ActionBuilder ab;
    if (encapType != IntFlowManager::ENCAP_NONE && vnid != 0) {
        ab.reg(MFF_REG0, vnid);
        IntFlowManager::actionTunnelMetadata(ab, encapType, tunDst);
    }
    for (uint32_t p : out_ports) {
        ab.output(p);
    }
    ab.build(&po);

    ofputil_protocol proto = ofputil_protocol_from_ofp_version
        ((ofp_version)conn->GetProtocolVersion());
    assert(ofputil_protocol_is_valid(proto));
    OfpBuf message(ofputil_encode_packet_out(&po, proto));
    int error = conn->SendMessage(message);
    free(po.ofpacts);
    return error;
}

void AdvertManager::sendRouterAdvs() {
    LOG(DEBUG) << "Sending all router advertisements";

    EndpointManager& epMgr = agent.getEndpointManager();
    PolicyManager& polMgr = agent.getPolicyManager();

    PolicyManager::uri_set_t epgURIs;
    polMgr.getGroups(epgURIs);

    for (const URI& epg : epgURIs) {
        unordered_set<string> eps;
        unordered_set<uint32_t> out_ports;
        epMgr.getEndpointsForGroup(epg, eps);
        for (const string& uuid : eps) {
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
        auto b =
            packets::compose_icmp6_router_ad(intFlowManager.getRouterMacAddr(),
                                             packets::MAC_ADDR_IPV6MULTICAST,
                                             (struct in6_addr*)bytes.data(),
                                             epg, polMgr);
        if (!b.get()) continue;

        int error = send_packet_out(switchConnection, b, out_ports);
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

static void doSendEpAdv(PolicyManager& policyManager,
                        SwitchConnection* switchConnection,
                        const string& ip, const uint8_t* epMac,
                        const uint8_t* routerMac,
                        const URI& egURI, uint32_t epgVnid,
                        unordered_set<uint32_t>& out_ports,
                        AdvertManager::EndpointAdvMode mode,
                        IntFlowManager::EncapType encapType,
                        address tunDst) {
    boost::system::error_code ec;
    address addr = address::from_string(ip, ec);
    if (ec) {
        LOG(ERROR) << "Invalid IP address: " << ip
                   << ": " << ec.message();
        return;
    }

    OfpBuf b((struct ofpbuf*)NULL);

    boost::optional<address> routerIp;
    if (mode == AdvertManager::EPADV_ROUTER_REQUEST) {
        boost::optional<std::shared_ptr<modelgbp::gbp::Subnet> > ipSubnet =
            policyManager.findSubnetForEp(egURI, addr);
        if (ipSubnet)
            routerIp = PolicyManager::getRouterIpForSubnet(*ipSubnet.get());

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
                network::construct_solicited_node_ip(routerIp.get().to_v6());
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
    if (!b.get()) return;

    int error = send_packet_out(switchConnection, b, out_ports,
                                encapType, epgVnid, tunDst);
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

    uint8_t epMac[6];
    ep->getMAC().get().toUIntArray(epMac);
    const uint8_t* routerMac = intFlowManager.getRouterMacAddr();

    for (const string& ip : ep->getIPs()) {
        LOG(DEBUG) << "Sending endpoint advertisement for "
                   << ep->getMAC().get() << " " << ip;

        doSendEpAdv(polMgr, switchConnection,
                    ip, epMac, routerMac, epgURI.get(), epgVnid.get(),
                    out_ports, sendEndpointAdv,
                    intFlowManager.getEncapType(),
                    intFlowManager.getEPGTunnelDst(epgURI.get()));

    }

    for (const Endpoint::IPAddressMapping& ipm : ep->getIPAddressMappings()) {
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

        doSendEpAdv(polMgr, switchConnection,
                    ipm.getFloatingIP().get(), epMac,
                    routerMac, ipm.getEgURI().get(),
                    ipmVnid.get(), out_ports, sendEndpointAdv,
                    intFlowManager.getEncapType(),
                    intFlowManager.getEPGTunnelDst(ipm.getEgURI().get()));
    }
}

void AdvertManager::sendAllEndpointAdvs() {
    LOG(DEBUG) << "Sending all endpoint advertisements";

    EndpointManager& epMgr = agent.getEndpointManager();
    PolicyManager& polMgr = agent.getPolicyManager();

    PolicyManager::uri_set_t epgURIs;
    polMgr.getGroups(epgURIs);
    for (const URI& epg : epgURIs) {
        unordered_set<string> eps;
        epMgr.getEndpointsForGroup(epg, eps);
        for (const string& uuid : eps) {
            sendEndpointAdvs(uuid);
        }
    }
}

struct ServiceAdvHash {
    size_t operator()(const Service& s) const noexcept {
        size_t v = 0;
        if (s.getServiceMAC())
            boost::hash_combine(v, s.getServiceMAC().get());
        if (s.getInterfaceName())
            boost::hash_combine(v, s.getInterfaceName().get());
        if (s.getIfaceVlan())
            boost::hash_combine(v, s.getIfaceVlan().get());
        if (s.getIfaceIP())
            boost::hash_combine(v, s.getIfaceIP().get());
        return v;
    }
};

struct ServiceAdvEqual {
    bool operator() (const Service& s1, const Service& s2) const noexcept {
        return (s1.getServiceMAC() == s2.getServiceMAC() &&
                s1.getInterfaceName() == s2.getInterfaceName() &&
                s1.getIfaceVlan() == s2.getIfaceVlan() &&
                s1.getIfaceIP() == s2.getIfaceIP());
    }
};

typedef std::unordered_set<Service, ServiceAdvHash, ServiceAdvEqual> adv_set_t;

static bool shouldSendAdv(const Service& s) {
    return ((s.getServiceMode() == Service::LOADBALANCER) &&
            s.getServiceMAC() && s.getIfaceIP() &&
            s.getInterfaceName());
}

void AdvertManager::sendAllServiceAdvs() {
    LOG(DEBUG) << "Sending all service advertisements";

    ServiceManager& svcMgr = agent.getServiceManager();
    PolicyManager& polMgr = agent.getPolicyManager();

    PolicyManager::uri_set_t rdURIs;
    polMgr.getRoutingDomains(rdURIs);
    for (const URI& rd : rdURIs) {
        unordered_set<string> svcs;
        svcMgr.getServicesByDomain(rd, svcs);

        adv_set_t advs;

        for (const string& uuid : svcs) {
            shared_ptr<const Service> s = svcMgr.getService(uuid);
            if (!s || !shouldSendAdv(*s)) {
                continue;
            }
            if (advs.find(*s) != advs.end()) {
                continue;
            }
            advs.insert(*s);

            sendServiceAdvs(uuid);
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
        agent.getAgentIOService()
            .dispatch(bind(&AdvertManager::sendAllServiceAdvs, this));
    }

    if (!stopping) {
        allEndpointAdvTimer->expires_from_now(seconds(all_ep_dis(urng)));
        allEndpointAdvTimer->
            async_wait(bind(&AdvertManager::onAllEndpointAdvTimer,
                            this, error));
    }
}

void AdvertManager::sendServiceAdvs(const string& uuid) {
    PolicyManager& polMgr = agent.getPolicyManager();
    ServiceManager& svcMgr = agent.getServiceManager();

    shared_ptr<const Service> svc = svcMgr.getService(uuid);

    if (!svc || !shouldSendAdv(*svc)) return;

    uint32_t port = portMapper->FindPort(svc->getInterfaceName().get());
    if (port == OFPP_NONE)
        return;

    unordered_set<uint32_t> out_ports {port};

    uint8_t svcMac[6];
    svc->getServiceMAC().get().toUIntArray(svcMac);
    const uint8_t* routerMac = intFlowManager.getRouterMacAddr();

    uint8_t vnid = 0;
    IntFlowManager::EncapType encapType = IntFlowManager::ENCAP_NONE;
    if (svc->getIfaceVlan()) {
        vnid = svc->getIfaceVlan().get();
        encapType = IntFlowManager::ENCAP_VLAN;
    }

    LOG(DEBUG) << "Sending service advertisement for "
               << svc->getServiceMAC().get() << " "
               << svc->getIfaceIP().get() << " on "
               << svc->getInterfaceName().get()
               << " (vlan " << unsigned(vnid) << ")";

    doSendEpAdv(polMgr, switchConnection, svc->getIfaceIP().get(),
                svcMac, routerMac, URI::ROOT, vnid,
                out_ports, sendEndpointAdv, encapType, address());
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

    unique_lock<mutex> guard(ep_mutex);
    {
        auto it = pendingEps.begin();
        while (it != pendingEps.end()) {
            agent.getAgentIOService()
                .post(bind(&AdvertManager::sendEndpointAdvs,
                           this, it->first));
            if (it->second <= 1) {
                it = pendingEps.erase(it);
            } else {
                it->second -= 1;
                it++;
            }
        }
    }
    {
        ServiceManager& svcMgr = agent.getServiceManager();
        adv_set_t advs;
        auto it = pendingServices.begin();
        while (it != pendingServices.end()) {
            shared_ptr<const Service> s = svcMgr.getService(it->first);
            if (!s || !shouldSendAdv(*s)) {
                it = pendingServices.erase(it);
                continue;
            }
            if (advs.find(*s) != advs.end()) {
                it->second -= 1;
                it++;
                continue;
            }
            advs.insert(*s);

            agent.getAgentIOService()
                .post(bind(&AdvertManager::sendServiceAdvs,
                           this, it->first));
            if (it->second <= 1) {
                it = pendingServices.erase(it);
            } else {
                it->second -= 1;
                it++;
            }
        }
    }

    if (pendingEps.size() > 0 || pendingServices.size() > 0)
        doScheduleEpAdv(repeat_dis(urng));
}

void AdvertManager::sendTunnelEpAdvs(const string& uuid) {
#ifdef __linux__
    const std::string tunnelIp  =
            intFlowManager.tunnelEpManager.getTerminationIp(uuid);
    const std::string tunnelMac =
            intFlowManager.tunnelEpManager.getTerminationMac(uuid);
    opflex::modb::MAC opMac(tunnelMac);
    uint8_t tunnelMacBytes[ETH_ALEN];
    opMac.toUIntArray(tunnelMacBytes);
    unsigned char buf[46];
    ifreq ifReq;
    struct sockaddr_ll sa_ll;
    int sockfd;
    memset(&ifReq, 0, sizeof(ifReq));
    memset(&sa_ll, 0, sizeof(sa_ll));
    memset(buf, 0, 46);
    struct arp::arp_hdr *arp_ptr = (struct arp::arp_hdr *) buf;
    sa_ll.sll_family = htons(AF_PACKET);
    sa_ll.sll_hatype = htons(1);
    sa_ll.sll_halen = htons(ETH_ALEN);
    string uplinkIface;
    intFlowManager.tunnelEpManager.getUplinkIface(uplinkIface);
    sa_ll.sll_ifindex = if_nametoindex(uplinkIface.c_str());
    if(sa_ll.sll_ifindex < 0) {
        LOG(ERROR) << "Failed to get ifindex by name " << uplinkIface <<
                ": " << sa_ll.sll_ifindex;
        return;
    }
    memset(sa_ll.sll_addr, 0xff, ETH_ALEN);
    arp_ptr->htype = htons(1);
    arp_ptr->ptype = htons(0x0800);
    arp_ptr->hlen = ETH_ALEN;
    arp_ptr->plen = 4;
    unsigned char *ptr = (unsigned char *)((unsigned char *)arp_ptr + sizeof(arp::arp_hdr));
    if(tunnelEndpointAdv == EndpointAdvMode::EPADV_GRATUITOUS_BROADCAST) {
        boost::system::error_code ec;
        address addr = address::from_string(tunnelIp, ec);
        if (ec || !addr.is_v4()) {
            LOG(ERROR) << "Invalid IPv4 address: " << tunnelIp;
            if(ec) {
                LOG(ERROR) << ": " << ec.message();
            }
            return;
        }
        uint32_t addrv = htonl(addr.to_v4().to_ulong());
        sockfd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ARP));
        if(sockfd < 0) {
            LOG(ERROR) << "Failed to create socket: " << sockfd;
            return;
        }
        sa_ll.sll_protocol = htons(ETH_P_ARP);

        arp_ptr->op = htons(opflexagent::arp::op::REQUEST);
        memcpy(ptr, tunnelMacBytes, ETH_ALEN);
        ptr+=ETH_ALEN;
        memcpy(ptr, &addrv, 4);
        ptr += 4;
        memset(ptr, 0xff, ETH_ALEN);
        ptr+=ETH_ALEN;
        memcpy(ptr, &addrv, 4);
    } else {
        sockfd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_RARP));
        if(sockfd < 0) {
            LOG(ERROR) << "Failed to create socket: " << sockfd;
            return;
        }
        sa_ll.sll_protocol = htons(ETH_P_RARP);

        arp_ptr->op = htons((opflexagent::arp::op::REVERSE_REQUEST));
        memcpy(ptr, tunnelMacBytes, ETH_ALEN);
        ptr += (ETH_ALEN + 4);
        memcpy(ptr, tunnelMacBytes, ETH_ALEN);
    }

    int error = sendto(sockfd, buf, 46 , htonl(SO_BROADCAST),
            (struct sockaddr*)&sa_ll, sizeof(struct sockaddr_ll));
    if (error < 0) {
        LOG(ERROR) << "Could not send tunnel advertisement: "
                   << error;
    } else {
       LOG(DEBUG) << "Sent advertisement for TunnelEp: " << tunnelMac << " " << tunnelIp;
    }
    close(sockfd);
#endif
}

void AdvertManager::onTunnelEpAdvTimer(const boost::system::error_code& ec) {
    if(ec)
        return;
    unique_lock<mutex> guard(tunnelep_mutex);
    {
        auto it = pendingTunnelEps.begin();
        while (it != pendingTunnelEps.end()) {
            agent.getAgentIOService()
                .post(bind(&AdvertManager::sendTunnelEpAdvs,
                           this, it->first));
            it++;
        }
    }
    if (pendingTunnelEps.size() > 0)
        doScheduleTunnelEpAdv(tunnelEpAdvInterval);
}

void AdvertManager::doScheduleTunnelEpAdv(uint64_t time) {
    tunnelEpAdvTimer->expires_from_now(seconds(time));
    tunnelEpAdvTimer->
            async_wait(bind(&AdvertManager::onTunnelEpAdvTimer,
                            this, error));
}

void AdvertManager::scheduleTunnelEpAdv(const std::string& uuid) {
    if (tunnelEpAdvTimer &&
            (tunnelEndpointAdv != EPADV_DISABLED)) {
#ifdef __linux__
        LOG(DEBUG) << "Scheduling Tunnel Ep advertisement";
        unique_lock<mutex> guard(tunnelep_mutex);
        pendingTunnelEps[uuid] = 5;
        doScheduleTunnelEpAdv();
#else
        LOG(ERROR) << "Tunnel advertisement not supported for non-linux platforms";
#endif
    }
}

} /* namespace opflexagent */
