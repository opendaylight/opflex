/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>
#include <sstream>
#include <boost/system/error_code.hpp>
#include <boost/algorithm/string/find_iterator.hpp>
#include <boost/algorithm/string/finder.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/ip/host_name.hpp>

#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/gbp/IntraGroupPolicyEnumT.hpp>
#include <modelgbp/gbp/UnknownFloodModeEnumT.hpp>
#include <modelgbp/gbp/BcastFloodModeEnumT.hpp>
#include <modelgbp/gbp/AddressResModeEnumT.hpp>
#include <modelgbp/gbp/RoutingModeEnumT.hpp>
#include <modelgbp/gbp/ConnTrackEnumT.hpp>
#include <modelgbp/l2/EtherTypeEnumT.hpp>
#include <modelgbp/l4/TcpFlagsEnumT.hpp>
#include <modelgbp/arp/OpcodeEnumT.hpp>

#include "logging.h"
#include "Endpoint.h"
#include "EndpointManager.h"
#include "VppManager.h"
#include <vom/om.hpp>
#include <vom/interface.hpp>
#include <vom/l2_binding.hpp>
#include <vom/l3_binding.hpp>
#include <vom/bridge_domain.hpp>
#include <vom/bridge_domain_entry.hpp>
#include <vom/bridge_domain_arp_entry.hpp>
#include <vom/interface.hpp>
#include <vom/dhcp_config.hpp>
#include <vom/acl_binding.hpp>
#include <vom/route_domain.hpp>
#include <vom/route.hpp>
#include <vom/neighbour.hpp>

using std::string;
using std::shared_ptr;
using std::unordered_set;
using std::bind;
using boost::optional;
using boost::asio::deadline_timer;
using boost::asio::ip::address;
using boost::asio::ip::address_v6;
using boost::asio::placeholders::error;
using opflex::modb::URI;
using opflex::modb::MAC;
using opflex::modb::class_id_t;
using modelgbp::gbpe::L24Classifier;
using modelgbp::l2::EtherTypeEnumT;

using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;

namespace ovsagent {

typedef EndpointListener::uri_set_t uri_set_t;

static const char* ID_NAMESPACES[] = {
    "floodDomain",
    "bridgeDomain",
    "routingDomain",
    "contract",
    "externalNetwork",
    "secGroup",
    "secGroupSet"
};

static const char* ID_NMSPC_FD      = ID_NAMESPACES[0];
static const char* ID_NMSPC_BD      = ID_NAMESPACES[1];
static const char* ID_NMSPC_RD      = ID_NAMESPACES[2];
static const char* ID_NMSPC_CON     = ID_NAMESPACES[3];
static const char* ID_NMSPC_EXTNET  = ID_NAMESPACES[4];
static const char* ID_NMSPC_SECGROUP     = ID_NAMESPACES[5];
static const char* ID_NMSPC_SECGROUP_SET = ID_NAMESPACES[6];

static string getSecGrpSetId(const uri_set_t& secGrps) {
    std::stringstream ss;
    bool notfirst = false;
    for (const URI& uri : secGrps) {
        if (notfirst) ss << ",";
        notfirst = true;
        ss << uri.toString();
    }
    return ss.str();
}

/**
 * An owner of the objects VPP learns during boot-up
 */
static const std::string BOOT_KEY = "__boot__";

VppManager::VppManager(Agent& agent_,
                       IdGenerator& idGen_) :
    agent(agent_),
    taskQueue(agent.getAgentIOService()),
    idGen(idGen_),
    stopping(false) {

    VOM::HW::init();
    VOM::OM::init();

    agent.getFramework().registerPeerStatusListener(this);
}

void VppManager::start() {
    for (size_t i = 0; i < sizeof(ID_NAMESPACES)/sizeof(char*); i++) {
        idGen.initNamespace(ID_NAMESPACES[i]);
    }
    initPlatformConfig();

    /*
     * make sure the first event in the task Q is the blocking
     * connection initiation to VPP ...
     */
    taskQueue.dispatch("init-connection",
                       bind(&VppManager::handleInitConnection, this));

    /**
     * DO BOOT
     */

    /**
     * ... followed by vpp boot dump
     */
    taskQueue.dispatch("boot-dump",
                       bind(&VppManager::handleBoot, this));

    /**
     * ... followed by uplink configuration
     */
    taskQueue.dispatch("uplink-configure",
                       bind(&VppManager::handleUplinkConfigure, this));

}

void VppManager::handleInitConnection()
{
    if (stopping) return;

    VOM::HW::connect();

    /**
     * We are insterested in getting interface events from VPP
     */
    std::shared_ptr<VOM::cmd> itf(new VOM::interface_cmds::events_cmd(*this));

    VOM::HW::enqueue(itf);
    m_cmds.push_back(itf);

    /**
     * We are insterested in getting DHCP events from VPP
     */
    std::shared_ptr<VOM::cmd> dc(new VOM::dhcp_config_cmds::events_cmd(*this));

    VOM::HW::enqueue(dc);
    m_cmds.push_back(dc);

    /**
     * Scehdule a timer to Poll for HW livensss
     */
    m_poll_timer.reset(new deadline_timer(agent.getAgentIOService()));
    m_poll_timer->expires_from_now(boost::posix_time::seconds(3));
    m_poll_timer->async_wait(bind(&VppManager::handleHWPollTimer, this, error));
}

void VppManager::handleUplinkConfigure()
{
    if (stopping) return;

    m_uplink.configure(boost::asio::ip::host_name());
}

void VppManager::handleSweepTimer(const boost::system::error_code& ec)
{
    if (stopping || ec) return;

    LOG(INFO) << "sweep boot data";

    /*
     * the sweep timer was not cancelled, continue with purging old state.
     */
    VOM::OM::sweep(BOOT_KEY);
}

void VppManager::handleHWPollTimer(const boost::system::error_code& ec)
{
    if (stopping || ec) return;

    if (!VOM::HW::poll())
    {
        /*
         * Lost connection to VPP; reconnect and then replay all the objects
         */
        VOM::HW::connect();
        VOM::OM::replay();
    }

    /*
     * re-scehdule a timer to Poll for HW liveness
     */
    m_poll_timer.reset(new deadline_timer(agent.getAgentIOService()));
    m_poll_timer->expires_from_now(boost::posix_time::seconds(3));
    m_poll_timer->async_wait(bind(&VppManager::handleHWPollTimer, this, error));
}

void VppManager::handleBoot()
{
    if (stopping) return;

    /**
     * Read the state from VPP
     */
    VOM::OM::populate(BOOT_KEY);
}

void VppManager::registerModbListeners() {
    // Initialize policy listeners
    agent.getEndpointManager().registerListener(this);
    agent.getServiceManager().registerListener(this);
    agent.getExtraConfigManager().registerListener(this);
    agent.getPolicyManager().registerListener(this);
}

void VppManager::stop() {
    stopping = true;

    agent.getEndpointManager().unregisterListener(this);
    agent.getServiceManager().unregisterListener(this);
    agent.getExtraConfigManager().unregisterListener(this);
    agent.getPolicyManager().unregisterListener(this);

    if (m_sweep_timer)
    {
        m_sweep_timer->cancel();
    }

    if (m_poll_timer)
    {
        m_poll_timer->cancel();
    }
}

void VppManager::setVirtualRouter(bool virtualRouterEnabled,
                                  bool routerAdv,
                                  const string& virtualRouterMac) {
    if (virtualRouterEnabled) {
        try {
            uint8_t routerMac[6];
            MAC(virtualRouterMac).toUIntArray(routerMac);
            m_vr = std::make_shared<VPP::VirtualRouter>(VPP::VirtualRouter(routerMac));
        } catch (std::invalid_argument) {
            LOG(ERROR) << "Invalid virtual router MAC: " << virtualRouterMac;
        }
    }
}

void VppManager::endpointUpdated(const std::string& uuid) {
    if (stopping) return;

    taskQueue.dispatch(uuid,
                       bind(&VppManager::handleEndpointUpdate, this, uuid));
}

void VppManager::serviceUpdated(const std::string& uuid) {
    if (stopping) return;

    LOG(INFO) << "Service Update Not supported ";
}

void VppManager::rdConfigUpdated(const opflex::modb::URI& rdURI) {
    domainUpdated(RoutingDomain::CLASS_ID, rdURI);
}

void VppManager::egDomainUpdated(const opflex::modb::URI& egURI) {
    if (stopping) return;

    taskQueue.dispatch(egURI.toString(),
                       bind(&VppManager::handleEndpointGroupDomainUpdate,
                            this, egURI));
}

void VppManager::domainUpdated(class_id_t cid, const URI& domURI) {
    if (stopping) return;

    taskQueue.dispatch(domURI.toString(),
                       bind(&VppManager::handleDomainUpdate,
                            this, cid, domURI));
}

void VppManager::secGroupSetUpdated(const EndpointListener::uri_set_t& secGrps) {
    if (stopping) return;
    taskQueue.dispatch("setSecGrp:",
                       std::bind(&VppManager::handleSecGrpSetUpdate,
                                 this, secGrps));
}

void VppManager::secGroupUpdated(const opflex::modb::URI& uri) {
    if (stopping) return;
    taskQueue.dispatch("secGrp:",
                       std::bind(&VppManager::handleSecGrpUpdate,
                                 this, uri));
}

void VppManager::contractUpdated(const opflex::modb::URI& contractURI) {
    if (stopping) return;
    taskQueue.dispatch(contractURI.toString(),
                       bind(&VppManager::handleContractUpdate,
                            this, contractURI));
}

void VppManager::handle_interface_event(VOM::interface_cmds::events_cmd *e)
{
    if (stopping) return;
    taskQueue.dispatch("InterfaceEvent",
                       bind(&VppManager::handleInterfaceEvent,
                            this, e));
}

void VppManager::handle_interface_stat(VOM::interface_cmds::stats_cmd *e)
{
    if (stopping) return;
    taskQueue.dispatch("InterfaceStat",
                       bind(&VppManager::handleInterfaceStat,
                            this, e));
}

void VppManager::handle_dhcp_event(VOM::dhcp_config_cmds::events_cmd *e)
{
    if (stopping) return;
    taskQueue.dispatch("dhcp-config-event",
                       bind(&VppManager::handleDhcpEvent,
                            this, e));
}

void VppManager::configUpdated(const opflex::modb::URI& configURI) {
    if (stopping) return;
    agent.getAgentIOService()
        .dispatch(bind(&VppManager::handleConfigUpdate, this, configURI));
}

void VppManager::portStatusUpdate(const string& portName,
                                  uint32_t portNo, bool fromDesc) {
    if (stopping) return;
    agent.getAgentIOService()
        .dispatch(bind(&VppManager::handlePortStatusUpdate, this,
                       portName, portNo));
}

void VppManager::peerStatusUpdated(const std::string&, int,
                                   PeerStatus peerStatus) {
    if (stopping) return;
}

bool VppManager::getGroupForwardingInfo(const URI& epgURI, uint32_t& vnid,
                                        optional<URI>& rdURI,
                                        uint32_t& rdId,
                                        optional<URI>& bdURI,
                                        uint32_t& bdId,
                                        optional<URI>& fdURI,
                                        uint32_t& fdId) {
    PolicyManager& polMgr = agent.getPolicyManager();
    optional<uint32_t> epgVnid = polMgr.getVnidForGroup(epgURI);
    if (!epgVnid) {
        return false;
    }
    vnid = epgVnid.get();

    optional<shared_ptr<RoutingDomain> > epgRd = polMgr.getRDForGroup(epgURI);
    optional<shared_ptr<BridgeDomain> > epgBd = polMgr.getBDForGroup(epgURI);
    optional<shared_ptr<FloodDomain> > epgFd = polMgr.getFDForGroup(epgURI);
    if (!epgRd && !epgBd && !epgFd) {
        return false;
    }

    bdId = 0;
    if (epgBd) {
        bdURI = epgBd.get()->getURI();
        bdId = getId(BridgeDomain::CLASS_ID, bdURI.get());
    }
    fdId = 0;
    if (epgFd) {
        fdURI = epgFd.get()->getURI();
        fdId = getId(FloodDomain::CLASS_ID, fdURI.get());
    }
    rdId = 0;
    if (epgRd) {
        rdURI = epgRd.get()->getURI();
        rdId = getId(RoutingDomain::CLASS_ID, rdURI.get());
    }
    return true;
}

void VppManager::handleEndpointUpdate(const string& uuid) {

    LOG(DEBUG) << "stub: Updating endpoint " << uuid;

    EndpointManager& epMgr = agent.getEndpointManager();
    shared_ptr<const Endpoint> epWrapper = epMgr.getEndpoint(uuid);

    if (!epWrapper) {
        /*
         * remove everything related to this endpoint
         */
        VOM::OM::remove(uuid);
        return;
    }

    optional<URI> epgURI = epMgr.getComputedEPG(uuid);
    if (!epgURI) {      // can't do much without EPG
        return;
    }

    /*
     * This is an update to all the state related to this endpoint.
     * At the end of processing we want all the state realted to this endpint,
     * that we don't touch here, gone.
     */
    VOM::OM::mark_n_sweep ms(uuid);

    const Endpoint& endPoint = *epWrapper.get();
    const optional<string>& vppInterfaceName = endPoint.getAccessInterface();
    const uri_set_t& secGrps = endPoint.getSecurityGroups();
    const std::string secGrpId = getSecGrpSetId(secGrps);
    int rv;

    uint32_t epgVnid, rdId, bdId, fgrpId;
    optional<URI> fgrpURI, bdURI, rdURI;
    if (!getGroupForwardingInfo(epgURI.get(), epgVnid, rdURI,
                                rdId, bdURI, bdId, fgrpURI, fgrpId)) {
        return;
    }

    /*
     * the route-domain the endpoint is in
     */
    route_domain rd(rdId);
    VOM::OM::write(uuid, rd);

    /*
     * We want a veth interface - admin up
     */
    VOM::interface itf(vppInterfaceName.get(),
                       VOM::interface::type_t::AFPACKET,
                       VOM::interface::admin_state_t::UP,
                       rd);
    VOM::OM::write(uuid, itf);

    /*
     * If the interface is not created then we can do no more
     */
    if (handle_t::INVALID == itf.handle())
      return;

    /**
     * We are interested in getting interface stats from VPP
     *
     * UNDER DEVELOPEMENT
     *
     std::shared_ptr<VOM::cmd> itfstats(new VOM::interface_cmds::stats_cmd(*this, std::vector<VOM::handle_t>{itf.handle()}));
     VOM::HW::enqueue(itfstats);
     m_cmds.push_back(itfstats);
    */

    if (agent.getEndpointManager().secGrpSetEmpty(secGrps)) {
        VOM::OM::remove(secGrpId);
    } else {
        VOM::ACL::l3_list::rules_t in_rules, out_rules;
        buildSecGrpSetUpdate(secGrps, secGrpId, in_rules, out_rules);

        optional<Endpoint::DHCPv4Config> v4c = endPoint.getDHCPv4Config();
        if (v4c)
            allowDhcpRequest(in_rules, EtherTypeEnumT::CONST_IPV4);

        optional<Endpoint::DHCPv6Config> v6c = endPoint.getDHCPv6Config();
        if (v6c)
            allowDhcpRequest(in_rules, EtherTypeEnumT::CONST_IPV6);

        if (!in_rules.empty()) {
            VOM::ACL::l3_list in_acl(secGrpId + "in", in_rules);
            VOM::OM::write(secGrpId, in_acl);

            VOM::ACL::l3_binding in_binding(direction_t::INPUT, itf, in_acl);
            VOM::OM::write(uuid, in_binding);
        }
        if (!out_rules.empty()) {
            VOM::ACL::l3_list out_acl(secGrpId + "out", out_rules);
            VOM::OM::write(secGrpId, out_acl);

            VOM::ACL::l3_binding out_binding(direction_t::OUTPUT, itf, out_acl);
            VOM::OM::write(uuid, out_binding);
        }
    }

    uint8_t macAddr[6];
    bool hasMac = endPoint.getMAC() != boost::none;

    if (hasMac)
        endPoint.getMAC().get().toUIntArray(macAddr);

    /* check and parse the IP-addresses */
    boost::system::error_code ec;

    std::vector<address> ipAddresses;
    for (const string& ipStr : endPoint.getIPs()) {
        address addr = address::from_string(ipStr, ec);
        if (ec) {
            LOG(WARNING) << "Invalid endpoint IP: "
                         << ipStr << ": " << ec.message();
        } else {
            ipAddresses.push_back(addr);
        }
    }

    if (hasMac) {
        address_v6 linkLocalIp(network::construct_link_local_ip_addr(macAddr));
        if (endPoint.getIPs().find(linkLocalIp.to_string()) ==
            endPoint.getIPs().end())
            ipAddresses.push_back(linkLocalIp);
    }

    VOM::ACL::l2_list::rules_t rules;
    if (itf.handle().value()) {
        if (endPoint.isPromiscuousMode()) {
            VOM::ACL::l2_rule rulev6(50,
                                     VOM::ACL::action_t::PERMIT,
                                     VOM::route::prefix_t::ZEROv6,
                                     macAddr,
                                     VOM::mac_address_t::ZERO);

            VOM::ACL::l2_rule rulev4(51,
                                     VOM::ACL::action_t::PERMIT,
                                     VOM::route::prefix_t::ZERO,
                                     macAddr,
                                     VOM::mac_address_t::ZERO);
            rules.insert(rulev4);
            rules.insert(rulev6);
        } else if (hasMac) {
            VOM::ACL::l2_rule rulev6(20,
                                     VOM::ACL::action_t::PERMIT,
                                     VOM::route::prefix_t::ZEROv6,
                                     macAddr,
                                     VOM::mac_address_t::ONE);

            VOM::ACL::l2_rule rulev4(21,
                                     VOM::ACL::action_t::PERMIT,
                                     VOM::route::prefix_t::ZERO,
                                     macAddr,
                                     VOM::mac_address_t::ONE);
            rules.insert(rulev4);
            rules.insert(rulev6);

            for (const address& ipAddr : ipAddresses) {
                // Allow IPv4/IPv6 packets from port with EP IP address
                VOM::route::prefix_t pfx(ipAddr, ipAddr.is_v4() ? 32 : 128);
                if (ipAddr.is_v6()) {
                    VOM::ACL::l2_rule rule(30,
                                           VOM::ACL::action_t::PERMIT,
                                           pfx,
                                           macAddr,
                                           VOM::mac_address_t::ONE);
                    rules.insert(rule);
                } else {
                    VOM::ACL::l2_rule rule(31,
                                           VOM::ACL::action_t::PERMIT,
                                           pfx,
                                           macAddr,
                                           VOM::mac_address_t::ONE);
                    rules.insert(rule);
                }
            }
        }

        for (const Endpoint::virt_ip_t& vip : endPoint.getVirtualIPs()) {
            network::cidr_t vip_cidr;
            if (!network::cidr_from_string(vip.second, vip_cidr)) {
                LOG(WARNING) << "Invalid endpoint VIP (CIDR): " << vip.second;
                continue;
            }
            uint8_t vmac[6];
            vip.first.toUIntArray(vmac);

            for (const address& ipAddr : ipAddresses) {
                if (!network::cidr_contains(vip_cidr, ipAddr)) {
                    continue;
                }
                VOM::route::prefix_t pfx(ipAddr, ipAddr.is_v4() ? 32 : 128);
                if (ipAddr.is_v6()) {
                    VOM::ACL::l2_rule rule(60,
                                           VOM::ACL::action_t::PERMIT,
                                           pfx,
                                           vmac,
                                           VOM::mac_address_t::ONE);
                    rules.insert(rule);
                } else {
                    VOM::ACL::l2_rule rule(61,
                                           VOM::ACL::action_t::PERMIT,
                                           pfx,
                                           vmac,
                                           VOM::mac_address_t::ONE);
                    rules.insert(rule);
                }
            }
        }

        VOM::ACL::l2_list acl(uuid, rules);
        VOM::OM::write(uuid, acl);

        VOM::ACL::l2_binding binding(VOM::direction_t::INPUT, itf, acl);
        VOM::OM::write(uuid, binding);
    }

    optional<shared_ptr<FloodDomain> > fd =
        agent.getPolicyManager().getFDForGroup(epgURI.get());

    uint8_t arpMode = AddressResModeEnumT::CONST_UNICAST;
    uint8_t ndMode = AddressResModeEnumT::CONST_UNICAST;
    uint8_t unkFloodMode = UnknownFloodModeEnumT::CONST_DROP;
    uint8_t bcastFloodMode = BcastFloodModeEnumT::CONST_NORMAL;
    if (fd) {
        // alagalah Irrespective of flooding scope (epg vs. flood-domain), the
        // properties of the flood-domain object decide how flooding
        // is done.

        arpMode = fd.get()
            ->getArpMode(AddressResModeEnumT::CONST_UNICAST);
        ndMode = fd.get()
            ->getNeighborDiscMode(AddressResModeEnumT::CONST_UNICAST);
        unkFloodMode = fd.get()
            ->getUnknownFloodMode(UnknownFloodModeEnumT::CONST_DROP);
        bcastFloodMode = fd.get()
            ->getBcastFloodMode(BcastFloodModeEnumT::CONST_NORMAL);

        VOM::bridge_domain bd(fgrpId, VOM::bridge_domain::learning_mode_t::OFF);
        VOM::OM::write(uuid, bd);

        VOM::l2_binding l2itf(itf, bd);
        VOM::OM::write(uuid, l2itf);

        if (hasMac)
        {
            /*
             * An entry in the BD's L2 FIB to forward traffic to the end-point
             */
            VOM::bridge_domain_entry be(bd, {macAddr}, itf);

            if (VOM::rc_t::OK != VOM::OM::write(uuid, be)) {
                LOG(ERROR) << "bridge-domain-entry: "
                           << vppInterfaceName.get();
                return;
            }

            /*
             * An entry in the BD's ARP termination table to reply to
             * ARP request for this end-point from other local end-points
             */
            for (const address& ipAddr : ipAddresses) {
                VOM::bridge_domain_arp_entry bae(bd, {macAddr}, ipAddr);

                if (VOM::rc_t::OK != VOM::OM::write(uuid, bae)) {
                    LOG(ERROR) << "bridge-domain-arp-entry: "
                               << vppInterfaceName.get();
                    return;
                }

                /*
                 * Add an L3 rewrite route to the end point. This will match packets
                 * from locally attached EPs in different subnets.
                 */
                VOM::route::path ep_path(ipAddr, itf);
                VOM::route::ip_route ep_route(rd, ipAddr);
                ep_route.add(ep_path);
                VOM::OM::write(uuid, ep_route);

                /*
                 * Add a neighbour entry
                 */
                VOM::neighbour ne(itf, {macAddr}, ipAddr);
                VOM::OM::write(uuid, ne);
            }
        }
    }

    /*
     * That's all folks ... destructor of mark_n_sweep calls the
     * sweep for the stale state
     */
}

void VppManager::handleEndpointGroupDomainUpdate(const URI& epgURI)
{
    LOG(DEBUG) << "Updating endpoint-group " << epgURI;

    const string& epg_uuid = epgURI.toString();
    PolicyManager &pm = agent.getPolicyManager();

    if (!agent.getPolicyManager().groupExists(epgURI))
    {
        VOM::OM::remove(epg_uuid);
        return;
    }
    uint32_t epgVnid, rdId, bdId, fgrpId;
    optional<URI> fgrpURI, bdURI, rdURI;
    if (!getGroupForwardingInfo(epgURI, epgVnid, rdURI,
                                rdId, bdURI, bdId, fgrpURI, fgrpId)) {
        return;
    }

    /*
     * Mark all of this EPG's state stale.
     */
    VOM::OM::mark(epg_uuid);

    /*
     * Construct the BridgeDomain
     */
    VOM::bridge_domain bd(fgrpId, VOM::bridge_domain::learning_mode_t::OFF);

    VOM::OM::write(epg_uuid, bd);

    /*
     * Construct the encap-link
     */
    std::shared_ptr<VOM::interface> encap_link(m_uplink.mk_interface(epg_uuid, epgVnid));

    /*
     * Add the encap-link to the BD
     *
     * If the encap link is a VLAN, then set the pop VTR operation on the
     * link so that the VLAN tag is correctly pop/pushed on rx/tx resp.
     */
    VOM::l2_binding l2(*encap_link, bd);
    if (VOM::interface::type_t::VXLAN != encap_link->type()) {
        l2.set(l2_binding::l2_vtr_op_t::L2_VTR_POP_1, epgVnid);
    }
    VOM::OM::write(epg_uuid, l2);

    /*
     * Add the BVIs to the BD
     */
    optional<shared_ptr<RoutingDomain>> epgRd = pm.getRDForGroup(epgURI);

    VOM::route_domain rd(rdId);
    VOM::OM::write(epg_uuid, rd);

    updateBVIs(epgURI, bd, rd, encap_link);

    /*
     * Sweep the remaining EPG's state
     */
    VOM::OM::sweep(epg_uuid);
}

void VppManager::updateBVIs(const URI& epgURI,
                            VOM::bridge_domain &bd,
                            const VOM::route_domain &rd,
                            std::shared_ptr<VOM::interface> encap_link)
{
    LOG(DEBUG) << "Updating BVIs";

    const string& epg_uuid = epgURI.toString();
    PolicyManager::subnet_vector_t subnets;
    agent.getPolicyManager().getSubnetsForGroup(epgURI, subnets);

    /*
     * Create a BVI interface for the EPG and add it to the bridge-domain
     */
    VOM::interface bvi("bvi-" + std::to_string(bd.id()),
                       VOM::interface::type_t::BVI,
                       VOM::interface::admin_state_t::UP,
                       rd);
    if (m_vr)
    {
        /*
         * Set the BVI's MAC address to the Virtual Router
         * address, so packets destined to the VR are handled
         * by layer 3.
         */
        bvi.set(m_vr->mac());
    }
    VOM::OM::write(epg_uuid, bvi);

    VOM::l2_binding l2(bvi, bd);
    VOM::OM::write(epg_uuid, l2);

    for (shared_ptr<Subnet>& sn : subnets)
    {
        optional<address> routerIp =
            PolicyManager::getRouterIpForSubnet(*sn);

        if (routerIp)
        {
            boost::asio::ip::address raddr = routerIp.get();
            /*
             * - apply the host prefix on the BVI
             * - add an entry into the L2 FIB for it.
             * - add an entry into the ARP Table for it.
             */
            VOM::route::prefix_t host_pfx(raddr);
            VOM::l3_binding l3(bvi, host_pfx);
            VOM::OM::write(epg_uuid, l3);

            VOM::bridge_domain_entry be(bd, bvi.l2_address().to_mac(), bvi);
            VOM::OM::write(epg_uuid, be);

            VOM::bridge_domain_arp_entry bae(bd,
                                             bvi.l2_address().to_mac(),
                                             raddr);
            VOM::OM::write(epg_uuid, bae);

            /*
             * add the subnet as a DVR route, so all other EPs will be
             * L3-bridged to the uplink
             */
            VOM::route::prefix_t subnet_pfx(raddr, sn->getPrefixLen().get());
            VOM::route::path dvr_path(*encap_link,
                                      VOM::nh_proto_t::ETHERNET);
            VOM::route::ip_route subnet_route(rd, subnet_pfx);
            subnet_route.add(dvr_path);
            VOM::OM::write(epg_uuid, subnet_route);
        }
    }
}

void VppManager::handleRoutingDomainUpdate(const URI& rdURI) {
    optional<shared_ptr<RoutingDomain > > rd =
        RoutingDomain::resolve(agent.getFramework(), rdURI);

    if (!rd) {
        LOG(DEBUG) << "Cleaning up for RD: " << rdURI;
        idGen.erase(getIdNamespace(RoutingDomain::CLASS_ID), rdURI.toString());
        return;
    }
    LOG(DEBUG) << "Updating routing domain " << rdURI;
}

void VppManager::handleDomainUpdate(class_id_t cid, const URI& domURI) {

    switch (cid) {
    case RoutingDomain::CLASS_ID:
        handleRoutingDomainUpdate(domURI);
        break;
    case Subnet::CLASS_ID:
        if (!Subnet::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for Subnet: " << domURI;
        }
        break;
    case BridgeDomain::CLASS_ID:
        if (!BridgeDomain::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for BD: " << domURI;
            idGen.erase(getIdNamespace(cid), domURI.toString());
        }
        break;
    case FloodDomain::CLASS_ID:
        if (!FloodDomain::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for FD: " << domURI;
            idGen.erase(getIdNamespace(cid), domURI.toString());
        }
        break;
    case FloodContext::CLASS_ID:
        if (!FloodContext::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for FloodContext: " << domURI;
        }
        break;
    case L3ExternalNetwork::CLASS_ID:
        if (!L3ExternalNetwork::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for L3ExtNet: " << domURI;
            idGen.erase(getIdNamespace(cid), domURI.toString());
        }
        break;
    }
    LOG(DEBUG) << "Updating domain " << domURI;
}

void VppManager::handleInterfaceEvent(VOM::interface_cmds::events_cmd *e)
{
    LOG(DEBUG) << "Interface Event: " << *e;

    std::lock_guard<VOM::interface_cmds::events_cmd> lg(*e);

    for (auto &msg : *e)
    {
        auto &payload = msg.get_payload();

        VOM::handle_t handle(payload.sw_if_index);
        std::shared_ptr<VOM::interface> sp = VOM::interface::find(handle);

        if (sp)
        {
            VOM::interface::oper_state_t oper_state =
                VOM::interface::oper_state_t::from_int(payload.link_up_down);

            LOG(DEBUG) << "Interface Event: " << sp->to_string()
                       << " state: " << oper_state.to_string();

            sp->set(oper_state);
        }
    }

    e->flush();
}

void VppManager::handleInterfaceStat(VOM::interface_cmds::stats_cmd *e)
{
    LOG(DEBUG) << "Interface Stat: " << *e;

    std::lock_guard<VOM::interface_cmds::stats_cmd> lg(*e);

    for (auto &msg : *e)
    {
        auto &payload = msg.get_payload();

        for (int i=0; i < payload.count; ++i) {
            auto &data = payload.data[i];

            VOM::handle_t handle(data.sw_if_index);
            std::shared_ptr<VOM::interface> sp = VOM::interface::find(handle);
            LOG(INFO) << "Interface Stat: " << sp->to_string()
                      << " stat rx_packets: " << data.rx_packets
                      << " stat rx_bytes: " << data.rx_bytes
                      << " stat tx_packets: " << data.tx_packets
                      << " stat tx_bytes: " << data.tx_bytes;
        }
    }

    e->flush();
}

void VppManager::handleDhcpEvent(VOM::dhcp_config_cmds::events_cmd *e)
{
    LOG(INFO) << "DHCP Event: " << *e;
    m_uplink.handle_dhcp_event(e);
}

void VppManager::handleContractUpdate(const opflex::modb::URI& contractURI) {
    LOG(DEBUG) << "Updating contract " << contractURI;

    const string& contractId = contractURI.toString();
    PolicyManager& polMgr = agent.getPolicyManager();
    if (!polMgr.contractExists(contractURI)) {  // Contract removed
        idGen.erase(getIdNamespace(Contract::CLASS_ID), contractURI.toString());
        return;
    }
}

void VppManager::initPlatformConfig() {

    using namespace modelgbp::platform;

    optional<shared_ptr<Config> > config =
        Config::resolve(agent.getFramework(),
                        agent.getPolicyManager().getOpflexDomain());
}

void VppManager::handleConfigUpdate(const opflex::modb::URI& configURI) {
    LOG(DEBUG) << "Updating platform config " << configURI;

    initPlatformConfig();

    /**
     * Now that we are known to be opflex connected,
     * Scehdule a timer to sweep the state we read when we first connected
     * to VPP.
     */
    m_sweep_timer.reset(new deadline_timer(agent.getAgentIOService()));
    m_sweep_timer->expires_from_now(boost::posix_time::seconds(30));
    m_sweep_timer->async_wait(bind(&VppManager::handleSweepTimer, this, error));
}

void VppManager::handlePortStatusUpdate(const string& portName,
                                        uint32_t) {
    LOG(DEBUG) << "Port-status update for " << portName;
}

typedef std::function<bool(opflex::ofcore::OFFramework&,
                           const string&,
                           const string&)> IdCb;

static const IdCb ID_NAMESPACE_CB[] =
{IdGenerator::uriIdGarbageCb<FloodDomain>,
 IdGenerator::uriIdGarbageCb<BridgeDomain>,
 IdGenerator::uriIdGarbageCb<RoutingDomain>,
 IdGenerator::uriIdGarbageCb<Contract>,
 IdGenerator::uriIdGarbageCb<L3ExternalNetwork>};


const char * VppManager::getIdNamespace(class_id_t cid) {
    const char *nmspc = NULL;
    switch (cid) {
    case RoutingDomain::CLASS_ID:   nmspc = ID_NMSPC_RD; break;
    case BridgeDomain::CLASS_ID:    nmspc = ID_NMSPC_BD; break;
    case FloodDomain::CLASS_ID:     nmspc = ID_NMSPC_FD; break;
    case Contract::CLASS_ID:        nmspc = ID_NMSPC_CON; break;
    case L3ExternalNetwork::CLASS_ID: nmspc = ID_NMSPC_EXTNET; break;
    default:
        assert(false);
    }
    return nmspc;
}

uint32_t VppManager::getId(class_id_t cid, const URI& uri) {
    return idGen.getId(getIdNamespace(cid), uri.toString());
}

VPP::Uplink &VppManager::uplink()
{
    return m_uplink;
}

void VppManager::handleSecGrpUpdate(const opflex::modb::URI& uri) {
    unordered_set<uri_set_t> secGrpSets;
    agent.getEndpointManager().getSecGrpSetsForSecGrp(uri, secGrpSets);
    for (const uri_set_t& secGrpSet : secGrpSets)
        secGroupSetUpdated(secGrpSet);
}

void VppManager::allowDhcpRequest(VOM::ACL::l3_list::rules_t& in_rules,
				  uint16_t etherType) {

    VOM::ACL::action_t act = VOM::ACL::action_t::PERMIT;

    if (etherType == EtherTypeEnumT::CONST_IPV4) {
        route::prefix_t pfx = route::prefix_t::ZERO;

        VOM::ACL::l3_rule rule(200, act, pfx, pfx);

        rule.set_proto(17);
        rule.set_src_from_port(68);
        rule.set_src_to_port(68);
        rule.set_dst_from_port(67);
        rule.set_dst_to_port(67);

        in_rules.insert(rule);
    } else {
        route::prefix_t pfx = route::prefix_t::ZEROv6;

        VOM::ACL::l3_rule rule(200, act, pfx, pfx);

        rule.set_proto(17);
        rule.set_src_from_port(546);
        rule.set_src_to_port(546);
        rule.set_dst_from_port(547);
        rule.set_dst_to_port(547);

        in_rules.insert(rule);
    }
}

void setParamUpdate(L24Classifier& cls, VOM::ACL::l3_rule& rule) {

    using modelgbp::l4::TcpFlagsEnumT;

    if (cls.isArpOpcSet()) {
        rule.set_proto(cls.getArpOpc().get());
    }

    if (cls.isProtSet()) {
        rule.set_proto(cls.getProt(0));

        if (cls.isSFromPortSet()) {
            rule.set_src_from_port(cls.getSFromPort(0));
        }

        if (cls.isSToPortSet()) {
            rule.set_src_to_port(cls.getSToPort(0));
        }

        if (cls.isDFromPortSet()) {
            rule.set_dst_from_port(cls.getDFromPort(0));
        }

        if (cls.isDToPortSet()) {
            rule.set_dst_to_port(cls.getDToPort(0));
        }

        if (6 == cls.getProt(0) &&  cls.isTcpFlagsSet()) {
            rule.set_tcp_flags_mask(cls.getTcpFlags(TcpFlagsEnumT::CONST_UNSPECIFIED));
            rule.set_tcp_flags_value(cls.getTcpFlags(TcpFlagsEnumT::CONST_UNSPECIFIED));
        }
    }
}

void VppManager::buildSecGrpSetUpdate(const uri_set_t& secGrps,
                                      const std::string& secGrpId,
                                      VOM::ACL::l3_list::rules_t& in_rules,
                                      VOM::ACL::l3_list::rules_t& out_rules) {
    LOG(DEBUG) << "Updating security group set";

    if (agent.getEndpointManager().secGrpSetEmpty(secGrps)) {
        VOM::OM::remove(secGrpId);
        return;
    }

    for (const opflex::modb::URI& secGrp : secGrps) {
        PolicyManager::rule_list_t rules;
        agent.getPolicyManager().getSecGroupRules(secGrp, rules);

        for (shared_ptr<PolicyRule>& pc : rules) {
            uint8_t dir = pc->getDirection();
            const shared_ptr<L24Classifier>& cls = pc->getL24Classifier();
            uint32_t priority = pc->getPriority();
            uint16_t etherType = cls->getEtherT(EtherTypeEnumT::CONST_UNSPECIFIED);
            VOM::ACL::action_t act = VOM::ACL::action_t::from_bool(pc->getAllow(),
                                                                   cls->getConnectionTracking(ConnTrackEnumT::CONST_NORMAL));

            if (!pc->getRemoteSubnets().empty()) {
                boost::optional<const network::subnets_t&> remoteSubs;
                remoteSubs = pc->getRemoteSubnets();
                for (const network::subnet_t& sub : remoteSubs.get()) {
                    bool is_v6 = boost::asio::ip::address::from_string(sub.first).is_v6();
                    route::prefix_t ip(sub.first, sub.second);
                    route::prefix_t ip2(route::prefix_t::ZERO);

                    if (etherType == EtherTypeEnumT::CONST_IPV4 ||
                        etherType == EtherTypeEnumT::CONST_ARP || !is_v6) {
                        ;
                    }

                    if (etherType == EtherTypeEnumT::CONST_IPV6 && is_v6) {
                        ip2 = route::prefix_t::ZEROv6;
                    }

                    if (dir == DirectionEnumT::CONST_BIDIRECTIONAL ||
                        dir == DirectionEnumT::CONST_IN) {
                        VOM::ACL::l3_rule rule(priority, act, ip, ip2);
                        setParamUpdate(*cls, rule);
                        in_rules.insert(rule);
                    }
                    if (dir == DirectionEnumT::CONST_BIDIRECTIONAL ||
                        dir == DirectionEnumT::CONST_OUT) {
                        VOM::ACL::l3_rule rule(priority, act, ip2, ip);
                        setParamUpdate(*cls, rule);
                        out_rules.insert(rule);
                    }
                }
            } else {
                route::prefix_t srcIp(route::prefix_t::ZERO);
                route::prefix_t dstIp(route::prefix_t::ZERO);

                if (etherType == EtherTypeEnumT::CONST_IPV6) {
                    srcIp = route::prefix_t::ZEROv6;
                    dstIp = route::prefix_t::ZEROv6;
                }

                VOM::ACL::l3_rule rule(priority, act, srcIp, dstIp);
                setParamUpdate(*cls, rule);
                if (dir == DirectionEnumT::CONST_BIDIRECTIONAL ||
                    dir == DirectionEnumT::CONST_IN) {
                    in_rules.insert(rule);
                }
                if (dir == DirectionEnumT::CONST_BIDIRECTIONAL ||
                    dir == DirectionEnumT::CONST_OUT) {
                    out_rules.insert(rule);
                }
            }
        }
    }
}

void VppManager::handleSecGrpSetUpdate(const uri_set_t& secGrps) {

    LOG(DEBUG) << "Updating security group set";
    VOM::ACL::l3_list::rules_t in_rules, out_rules;
    const std::string secGrpId = getSecGrpSetId(secGrps);
    std::shared_ptr<VOM::ACL::l3_list> in_acl, out_acl;

    buildSecGrpSetUpdate(secGrps, secGrpId, in_rules, out_rules);

    if (in_rules.empty() && out_rules.empty())
        return;

    if (!in_rules.empty()) {
        VOM::ACL::l3_list inAcl(secGrpId + "in", in_rules);
        in_acl = inAcl.singular();
        VOM::OM::write(secGrpId, *in_acl);
    }

    if (!out_rules.empty()) {
        VOM::ACL::l3_list outAcl(secGrpId + "out", out_rules);
        out_acl = outAcl.singular();
        VOM::OM::write(secGrpId, *out_acl);
    }

    EndpointManager& epMgr = agent.getEndpointManager();
    std::unordered_set<std::string> eps;
    epMgr.getEndpointsForSecGrps(secGrps, eps);

    for (const std::string& uuid : eps) {
        const Endpoint& endPoint = *epMgr.getEndpoint(uuid).get();
        const optional<string>& vppInterfaceName = endPoint.getAccessInterface();

        if (!vppInterfaceName)
            continue;

        std::shared_ptr<VOM::interface> itf = VOM::interface::find(vppInterfaceName.get());

        if (!itf)
            continue;

        if (!in_rules.empty()) {
            VOM::ACL::l3_binding in_binding(direction_t::INPUT, *itf, *in_acl);
            VOM::OM::write(uuid, in_binding);
        }
        if (!out_rules.empty()) {
            VOM::ACL::l3_binding out_binding(direction_t::OUTPUT, *itf, *out_acl);
            VOM::OM::write(uuid, out_binding);
        }
    }
}

} // namespace ovsagent
