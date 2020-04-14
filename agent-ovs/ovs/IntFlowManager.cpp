/*
 * Copyright (c) 2014-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <boost/system/error_code.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>

#include <netinet/icmp6.h>

#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/gbp/IntraGroupPolicyEnumT.hpp>
#include <modelgbp/gbp/UnknownFloodModeEnumT.hpp>
#include <modelgbp/gbp/BcastFloodModeEnumT.hpp>
#include <modelgbp/gbp/AddressResModeEnumT.hpp>
#include <modelgbp/gbp/RoutingModeEnumT.hpp>
#include <modelgbp/platform/RemoteInventoryTypeEnumT.hpp>
#include <modelgbp/observer/DropLogModeEnumT.hpp>
#include <modelgbp/gbp/EnforcementPreferenceTypeEnumT.hpp>
#include <modelgbp/gbpe/SvcToEpCounter.hpp>
#include <modelgbp/gbpe/EpToSvcCounter.hpp>
#include <modelgbp/observer/SvcStatUniverse.hpp>
#include <modelgbp/fault/SeverityEnumT.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/algorithm/string.hpp>

#include <opflexagent/logging.h>
#include <opflexagent/Endpoint.h>
#include <opflexagent/EndpointManager.h>
#include <opflexagent/Faults.h>
#include <opflexagent/Network.h>

#include "SwitchConnection.h"
#include "IntFlowManager.h"
#include "TableState.h"
#include "PacketInHandler.h"
#include "CtZoneManager.h"
#include "Packets.h"
#include "FlowUtils.h"
#include "FlowConstants.h"
#include "FlowBuilder.h"
#include "RangeMask.h"

#include "arp.h"
#include "eth.h"
#include "ovs-ofputil.h"
#include "ovs-shim.h"

#include <openvswitch/list.h>
#include <arpa/inet.h>

using std::string;
using std::vector;
using std::ostringstream;
using std::shared_ptr;
using std::unordered_set;
using std::unordered_map;
using std::pair;
using boost::optional;
using boost::uuids::to_string;
using boost::uuids::basic_random_generator;
using boost::asio::deadline_timer;
using boost::asio::ip::address;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;
using std::chrono::milliseconds;
using std::unique_lock;
using std::mutex;
using opflex::modb::URI;
using opflex::modb::MAC;
using opflex::modb::class_id_t;
using modelgbp::observer::SvcStatUniverse;

namespace pt = boost::property_tree;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;

std::random_device rng;
std::mt19937 urng(rng());
basic_random_generator<std::mt19937> uuidGen(urng);

namespace opflexagent {

static const char* ID_NAMESPACES[] =
    {"floodDomain", "bridgeDomain", "routingDomain",
     "externalNetwork", "l24classifierRule",
     "svcstats", "service"};

static const char* ID_NMSPC_FD            = ID_NAMESPACES[0];
static const char* ID_NMSPC_BD            = ID_NAMESPACES[1];
static const char* ID_NMSPC_RD            = ID_NAMESPACES[2];
static const char* ID_NMSPC_EXTNET        = ID_NAMESPACES[3];
static const char* ID_NMSPC_L24CLASS_RULE = ID_NAMESPACES[4];
static const char* ID_NMSPC_SVCSTATS      = ID_NAMESPACES[5];
static const char* ID_NMSPC_SERVICE       = ID_NAMESPACES[6];



void IntFlowManager::populateTableDescriptionMap(
        SwitchManager::TableDescriptionMap &fwdTblDescr) {
    // Populate descriptions of flow tables
#define TABLE_DESC(table_id, table_name, drop_reason) \
        fwdTblDescr.insert( \
                    std::make_pair(table_id, \
                            std::make_pair(table_name, drop_reason)));
    TABLE_DESC(SEC_TABLE_ID, "PORT_SECURITY_TABLE",
            "Port security policy missing/incorrect")
    TABLE_DESC(SRC_TABLE_ID, "SOURCE_TABLE",
            "Source policy group derivation missing/incorrect")
    TABLE_DESC(SNAT_REV_TABLE_ID, "SNAT_REV_TABLE",
            "Reverse SNAT policy missing/incorrect")
    TABLE_DESC(SERVICE_REV_TABLE_ID, "SERVICE_REV_TABLE",
            "Service source policy missing/incorrect")
    TABLE_DESC(BRIDGE_TABLE_ID, "BRIDGE_TABLE", "MAC lookup failed")
    TABLE_DESC(SERVICE_NEXTHOP_TABLE_ID, "SERVICE_NEXTHOP_TABLE",
            "Service destination policy missing/incorrect")
    TABLE_DESC(ROUTE_TABLE_ID, "ROUTE_TABLE", "Route lookup failed")
    TABLE_DESC(SNAT_TABLE_ID, "SNAT_TABLE", "SNAT policy missing/incorrect")
    TABLE_DESC(NAT_IN_TABLE_ID, "NAT_IN_TABLE",
            "NAT ingress port policy missing/incorrect")
    TABLE_DESC(LEARN_TABLE_ID, "LEARN_TABLE", "Learn table drop")
    TABLE_DESC(SERVICE_DST_TABLE_ID, "SERVICE_DST_TABLE",
            "Service destination missing/incorrect")
    TABLE_DESC(POL_TABLE_ID, "POL_TABLE", "Contract missing/incorrect")
    TABLE_DESC(STATS_TABLE_ID, "STATS_TABLE", "Stats Table drop")
    TABLE_DESC(OUT_TABLE_ID, "OUT_TABLE",
            "Derived output port missing/incorrect")
#undef TABLE_DESC
}

IntFlowManager::IntFlowManager(Agent& agent_,
                               SwitchManager& switchManager_,
                               IdGenerator& idGen_,
                               CtZoneManager& ctZoneManager_,
                               PacketInHandler& pktInHandler_,
                               TunnelEpManager& tunnelEpManager_) :
    agent(agent_), switchManager(switchManager_), idGen(idGen_),
    ctZoneManager(ctZoneManager_), pktInHandler(pktInHandler_),
    tunnelEpManager(tunnelEpManager_),
#ifdef HAVE_PROMETHEUS_SUPPORT
    prometheusManager(agent.getPrometheusManager()),
#endif
    taskQueue(agent.getAgentIOService()), encapType(ENCAP_NONE),
    floodScope(FLOOD_DOMAIN), tunnelPortStr("4789"),
    virtualRouterEnabled(false), routerAdv(false),
    virtualDHCPEnabled(false), conntrackEnabled(false), dropLogRemotePort(0),
    serviceStatsFlowDisabled(false),
    advertManager(agent, *this), isSyncing(false), stopping(false) {
    // set up flow tables
    switchManager.setMaxFlowTables(NUM_FLOW_TABLES);
    SwitchManager::TableDescriptionMap fwdTblDescr;
    populateTableDescriptionMap(fwdTblDescr);
    switchManager.setForwardingTableList(fwdTblDescr);

    memset(routerMac, 0, sizeof(routerMac));
    memset(dhcpMac, 0, sizeof(dhcpMac));
    tunnelDst = address::from_string("127.0.0.1");

    agent.getFramework().registerPeerStatusListener(this);
}

void IntFlowManager::start(bool serviceStatsFlowDisabled_) {
    LOG(DEBUG) << "Starting IntFlowManager"
               << " serviceStatsFlowDisabled: " << serviceStatsFlowDisabled_;
    serviceStatsFlowDisabled = serviceStatsFlowDisabled_;

    // set up port mapper
    switchManager.getPortMapper().registerPortStatusListener(this);
    advertManager.setPortMapper(&switchManager.getPortMapper());

    // Register connection handlers
    SwitchConnection* conn = switchManager.getConnection();
    advertManager.registerConnection(conn);

    for (size_t i = 0; i < sizeof(ID_NAMESPACES)/sizeof(char*); i++) {
        idGen.initNamespace(ID_NAMESPACES[i]);
    }

    initPlatformConfig();
    createStaticFlows();
}

void IntFlowManager::registerModbListeners() {
    // Initialize policy listeners
    agent.getEndpointManager().registerListener(this);
    agent.getServiceManager().registerListener(this);
    agent.getExtraConfigManager().registerListener(this);
    agent.getLearningBridgeManager().registerListener(this);
    agent.getPolicyManager().registerListener(this);
    agent.getSnatManager().registerListener(this);
    tunnelEpManager.registerListener(this);
}

void IntFlowManager::stop() {
    LOG(DEBUG) << "Stopping IntFlowManager";
    stopping = true;

    agent.getEndpointManager().unregisterListener(this);
    agent.getServiceManager().unregisterListener(this);
    agent.getExtraConfigManager().unregisterListener(this);
    agent.getLearningBridgeManager().unregisterListener(this);
    agent.getPolicyManager().unregisterListener(this);
    agent.getSnatManager().unregisterListener(this);
    tunnelEpManager.unregisterListener(this);

    advertManager.stop();
    switchManager.getPortMapper().unregisterPortStatusListener(this);
}

void IntFlowManager::setEncapType(EncapType encapType) {
    this->encapType = encapType;
}

void IntFlowManager::setEncapIface(const string& encapIf) {
    if (encapIf.empty()) {
        LOG(ERROR) << "Ignoring empty encapsulation interface name";
        return;
    }
    encapIface = encapIf;
}

void IntFlowManager::setUplinkIface(const string& uplinkIf) {
    if (uplinkIf.empty()) {
        LOG(ERROR) << "Ignoring empty uplink interface name";
        return;
    }
    uplinkIface = uplinkIf;
}

uint32_t IntFlowManager::getUplinkPort() {
    return switchManager.getPortMapper().FindPort(uplinkIface);
}

void IntFlowManager::setFloodScope(FloodScope fscope) {
    floodScope = fscope;
}

uint32_t IntFlowManager::getTunnelPort() {
    return switchManager.getPortMapper().FindPort(encapIface);
}

void IntFlowManager::setTunnel(const string& tunnelRemoteIp,
                               uint16_t tunnelRemotePort) {
    boost::system::error_code ec;
    address tunDst = address::from_string(tunnelRemoteIp, ec);
    if (ec) {
        LOG(ERROR) << "Invalid tunnel destination IP: "
                   << tunnelRemoteIp << ": " << ec.message();
    } else if (tunDst.is_v6()) {
        LOG(ERROR) << "IPv6 tunnel destinations are not supported";
    } else {
        tunnelDst = tunDst;
    }

    ostringstream ss;
    ss << tunnelRemotePort;
    tunnelPortStr = ss.str();
}

void IntFlowManager::setDropLog(const string& dropLogPort, const string& dropLogRemoteIp,
        const uint16_t _dropLogRemotePort) {
    dropLogIface = dropLogPort;
    boost::system::error_code ec;
    address tunDst = address::from_string(dropLogRemoteIp, ec);
    if (ec) {
        LOG(ERROR) << "Invalid drop-log tunnel destination IP: "
                   << dropLogRemoteIp << ": " << ec.message();
    } else if (tunDst.is_v6()) {
        LOG(ERROR) << "IPv6 drop-log tunnel destinations are not supported";
    } else {
        dropLogDst = tunDst;
        LOG(INFO) << "DropLog port set to " << dropLogPort
                   << " tunnel destination: " << dropLogRemoteIp
                   << ":" <<_dropLogRemotePort;
    }
    dropLogRemotePort = _dropLogRemotePort;
}

void IntFlowManager::setVirtualRouter(bool virtualRouterEnabled,
                                      bool routerAdv,
                                      const string& virtualRouterMac) {
    this->virtualRouterEnabled = virtualRouterEnabled;
    this->routerAdv = routerAdv;
    try {
        MAC(virtualRouterMac).toUIntArray(routerMac);
    } catch (std::invalid_argument&) {
        LOG(ERROR) << "Invalid virtual router MAC: " << virtualRouterMac;
    }
    advertManager.enableRouterAdv(virtualRouterEnabled && routerAdv);
}

void IntFlowManager::setVirtualDHCP(bool dhcpEnabled,
                                    const string& mac) {
    this->virtualDHCPEnabled = dhcpEnabled;
    try {
        MAC(mac).toUIntArray(dhcpMac);
    } catch (std::invalid_argument&) {
        LOG(ERROR) << "Invalid virtual DHCP server MAC: " << mac;
    }
}

void IntFlowManager::setEndpointAdv(AdvertManager::EndpointAdvMode mode,
        AdvertManager::EndpointAdvMode tunnelMode,
        uint64_t tunnelAdvIntvl) {
    if (mode != AdvertManager::EPADV_DISABLED)
        advertManager.enableEndpointAdv(mode);
    advertManager.enableTunnelEndpointAdv(tunnelMode, tunnelAdvIntvl);
}

void IntFlowManager::setMulticastGroupFile(const std::string& mcastGroupFile) {
    this->mcastGroupFile = mcastGroupFile;
}

void IntFlowManager::enableConnTrack() {
    conntrackEnabled = true;
}

address IntFlowManager::getEPGTunnelDst(const URI& epgURI) {
    if (encapType != IntFlowManager::ENCAP_VXLAN &&
        encapType != IntFlowManager::ENCAP_IVXLAN)
        return address();

    optional<string> epgMcastIp =
        agent.getPolicyManager().getMulticastIPForGroup(epgURI);
    if (epgMcastIp) {
        boost::system::error_code ec;
        address ip = address::from_string(epgMcastIp.get(), ec);
        if (ec || !ip.is_v4() || ! ip.is_multicast()) {
            LOG(WARNING) << "Ignoring invalid/unsupported group multicast "
                "IP: " << epgMcastIp.get();
            return getTunnelDst();
        }
        return ip;
    } else
        return getTunnelDst();
}

void IntFlowManager::endpointUpdated(const std::string& uuid) {
    if (stopping) return;

    if(tunnelEpManager.isTunnelEp(uuid)){
        advertManager.scheduleTunnelEpAdv(uuid);
        return;
    }
    advertManager.scheduleEndpointAdv(uuid);
    taskQueue.dispatch(uuid, [=]() { handleEndpointUpdate(uuid); });
}

void IntFlowManager::localExternalDomainUpdated(const opflex::modb::URI& egURI) {
    if (stopping) return;
    taskQueue.dispatch(egURI.toString(), [=]() { handleLocalExternalDomainUpdated(egURI); });
}

void IntFlowManager::remoteEndpointUpdated(const string& uuid) {
    if (stopping) return;
    taskQueue.dispatch(uuid,
                       [=](){ handleRemoteEndpointUpdate(uuid); });
}

void IntFlowManager::serviceUpdated(const std::string& uuid) {
    if (stopping) return;

    advertManager.scheduleServiceAdv(uuid);
    taskQueue.dispatch(uuid, [=]() { handleServiceUpdate(uuid); });
}

void IntFlowManager::rdConfigUpdated(const opflex::modb::URI& rdURI) {
    domainUpdated(RoutingDomain::CLASS_ID, rdURI);
}

void IntFlowManager::packetDropLogConfigUpdated(const opflex::modb::URI& dropLogCfgURI) {
    using modelgbp::observer::DropLogConfig;
    using modelgbp::observer::DropLogModeEnumT;
    FlowEntryList dropLogFlows;
    optional<shared_ptr<DropLogConfig>> dropLogCfg =
            DropLogConfig::resolve(agent.getFramework(), dropLogCfgURI);
    if(!dropLogCfg) {
        FlowBuilder().priority(2)
                .action().go(IntFlowManager::SEC_TABLE_ID)
                .parent().build(dropLogFlows);
        switchManager.writeFlow("DropLogConfig", DROP_LOG_TABLE_ID, dropLogFlows);
        LOG(INFO) << "Defaulting to droplog disabled";
        return;
    }
    if(dropLogCfg.get()->getDropLogEnable(0) != 0) {
        if(dropLogCfg.get()->getDropLogMode(
                    DropLogModeEnumT::CONST_UNFILTERED_DROP_LOG) ==
           DropLogModeEnumT::CONST_UNFILTERED_DROP_LOG) {
            FlowBuilder().priority(2)
                    .action()
                    .metadata(flow::meta::DROP_LOG,
                              flow::meta::DROP_LOG)
                    .go(IntFlowManager::SEC_TABLE_ID)
                    .parent().build(dropLogFlows);
            LOG(INFO) << "Droplog mode set to unfiltered";
        } else {
            switchManager.clearFlows("DropLogConfig", DROP_LOG_TABLE_ID);
            LOG(INFO) << "Droplog mode set to filtered";
            return;
        }
    } else {
        LOG(INFO) << "Droplog disabled";
        FlowBuilder().priority(2)
                .action()
                .go(IntFlowManager::SEC_TABLE_ID)
                .parent().build(dropLogFlows);
    }
    switchManager.writeFlow("DropLogConfig", DROP_LOG_TABLE_ID, dropLogFlows);
}

void IntFlowManager::packetDropFlowConfigUpdated(const opflex::modb::URI& dropFlowCfgURI) {
    using modelgbp::observer::DropFlowConfig;
    optional<shared_ptr<DropFlowConfig>> dropFlowCfg =
            DropFlowConfig::resolve(agent.getFramework(), dropFlowCfgURI);
    if(!dropFlowCfg) {
        switchManager.clearFlows(dropFlowCfgURI.toString(), DROP_LOG_TABLE_ID);
        return;
    }
    FlowEntryList dropLogFlows;
    FlowBuilder fb;
    fb.priority(1);
    if(dropFlowCfg.get()->isEthTypeSet()) {
        fb.ethType(dropFlowCfg.get()->getEthType(0));
    }
    if(dropFlowCfg.get()->isInnerSrcAddressSet()) {
        address addr = address::from_string(
            dropFlowCfg.get()->getInnerSrcAddress(""));
        fb.ipSrc(addr);
    }
    if(dropFlowCfg.get()->isInnerDstAddressSet()) {
        address addr = address::from_string(
                dropFlowCfg.get()->getInnerDstAddress(""));
        fb.ipDst(addr);
    }
    if(dropFlowCfg.get()->isOuterSrcAddressSet()) {
        address addr = address::from_string(
                dropFlowCfg.get()->getOuterSrcAddress(""));
        fb.outerIpSrc(addr);
    }
    if(dropFlowCfg.get()->isOuterDstAddressSet()) {
        address addr = address::from_string(
                dropFlowCfg.get()->getOuterDstAddress(""));
        fb.outerIpDst(addr);
    }
    if(dropFlowCfg.get()->isTunnelIdSet()) {
        fb.tunId(dropFlowCfg.get()->getTunnelId(0));
    }
    if(dropFlowCfg.get()->isIpProtoSet()) {
        fb.proto(dropFlowCfg.get()->getIpProto(0));
    }
    if(dropFlowCfg.get()->isSrcPortSet()) {
        fb.tpSrc(dropFlowCfg.get()->getSrcPort(0));
    }
    if(dropFlowCfg.get()->isDstPortSet()) {
        fb.tpDst(dropFlowCfg.get()->getDstPort(0));
    }
    fb.action().metadata(flow::meta::DROP_LOG, flow::meta::DROP_LOG)
            .go(IntFlowManager::SEC_TABLE_ID).parent().build(dropLogFlows);
    switchManager.writeFlow(dropFlowCfgURI.toString(), DROP_LOG_TABLE_ID,
            dropLogFlows);
}

void IntFlowManager::lbIfaceUpdated(const std::string& uuid) {
    if (stopping) return;

    taskQueue.dispatch(uuid,
                       [=]() { handleLearningBridgeIfaceUpdate(uuid); });
}

void IntFlowManager::lbVlanUpdated(LearningBridgeIface::vlan_range_t vlan) {
    if (stopping) return;

    taskQueue.dispatch(boost::lexical_cast<string>(vlan),
                       [=]() { handleLearningBridgeVlanUpdate(vlan); });
}

void IntFlowManager::egDomainUpdated(const opflex::modb::URI& egURI) {
    if (stopping) return;

    taskQueue.dispatch(egURI.toString(),
                       [=]() { handleEndpointGroupDomainUpdate(egURI); });
}

void IntFlowManager::domainUpdated(class_id_t cid, const URI& domURI) {
    if (stopping) return;

    taskQueue.dispatch(domURI.toString(),
                       [=]() { handleDomainUpdate(cid, domURI); });
}

void IntFlowManager::contractUpdated(const opflex::modb::URI& contractURI) {
    if (stopping) return;
    taskQueue.dispatch(contractURI.toString(),
                       [=]() { handleContractUpdate(contractURI); });
}

void IntFlowManager::configUpdated(const opflex::modb::URI& configURI) {
    if (stopping) return;
    optional<shared_ptr<modelgbp::platform::Config>> config_opt =
        modelgbp::platform::Config::resolve(agent.getFramework(), configURI);
    if (config_opt) {
        boost::optional<const uint8_t> configEncapType =
            config_opt.get()->getEncapType();
        if (configEncapType && configEncapType.get() != encapType) {
            LOG(INFO) << "fault raised for encapType from fabric doesn't match "
                         "agent config";
            auto fu = modelgbp::fault::Universe::resolve(agent.getFramework());
            opflex::modb::Mutator mutator(agent.getFramework(),
                                          "policyelement");
            auto fi = fu.get()->addFaultInstance(to_string(uuidGen()));
            fi->setDescription(
                "encapType from fabric doesn't match agent config");
            fi->setSeverity(modelgbp::fault::SeverityEnumT::CONST_CRITICAL);
            fi->setAffectedObject(configURI.toString());
            fi->setFaultCode(opflexagent::FaultCodes::ENCAP_MISMATCH);
            mutator.commit();
        }
    }
    switchManager.enableSync();
    agent.getAgentIOService()
        .dispatch([=]() { handleConfigUpdate(configURI); });
}

void IntFlowManager::portStatusUpdate(const string& portName,
                                      uint32_t portNo, bool fromDesc) {
    if (stopping) return;
    agent.getAgentIOService()
        .dispatch([=]() {
                handlePortStatusUpdate(portName, portNo);
            });
}

void IntFlowManager::snatUpdated(const std::string& uuid) {
    if (stopping) return;
    taskQueue.dispatch(uuid, [=]() { handleSnatUpdate(uuid); });
}

void IntFlowManager::peerStatusUpdated(const std::string&, int,
                                       PeerStatus peerStatus) {
    if (stopping || isSyncing) return;
    if (peerStatus == PeerStatusListener::READY) {
        advertManager.scheduleInitialEndpointAdv();
    }
}

bool IntFlowManager::getGroupForwardingInfo(const URI& epgURI, uint32_t& vnid,
                                            optional<URI>& rdURI,
                                            uint32_t& rdId,
                                            optional<URI>& bdURI,
                                            uint32_t& bdId,
                                            optional<URI>& fdURI,
                                            uint32_t& fdId) {
    EndpointManager &epMgr = agent.getEndpointManager();

    if(epMgr.localExternalDomainExists(epgURI)) {
        string bdStr, fdStr;
        /* Ext encap id is a vlan id which falls in the same namespace as
         * the vnid for the flow tables and it could clash with a real EPG.
         * So we add a >24 bit value prefix to it.
         */
        optional<uint32_t> epgVnid = ((1<< 30) + epMgr.getExtEncapId(epgURI));
        if (!epgVnid) {
            return false;
        }
        vnid = epgVnid.get();
        bdStr = "extbd:" + epgURI.toString();
        bdURI = URI(bdStr);
        bdId = getId(BridgeDomain::CLASS_ID, bdURI.get());

        fdStr = "extfd:" + epgURI.toString();
        fdURI = URI(fdStr);
        fdId = getId(FloodDomain::CLASS_ID, fdURI.get());

        rdId = 0;
    } else {
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
        if (epgFd) {    // FD present -> flooding is desired
            if (floodScope == ENDPOINT_GROUP) {
                fdURI = epgURI;
            } else  {
                fdURI = epgFd.get()->getURI();
            }
            fdId = getId(FloodDomain::CLASS_ID, fdURI.get());
        }
        rdId = 0;
        if (epgRd) {
            rdURI = epgRd.get()->getURI();
            rdId = getId(RoutingDomain::CLASS_ID, rdURI.get());
        }
    }
    return true;
}

// Match helper functions
static FlowBuilder& matchEpg(FlowBuilder& fb,
                             IntFlowManager::EncapType encapType,
                             uint32_t epgId) {
    switch (encapType) {
    case IntFlowManager::ENCAP_VLAN:
        fb.vlan(0xfff & epgId);
        break;
    case IntFlowManager::ENCAP_VXLAN:
    case IntFlowManager::ENCAP_IVXLAN:
    default:
        fb.tunId(epgId);
        break;
    }
    return fb;
}

static FlowBuilder& matchDestDom(FlowBuilder& fb, uint32_t bdId,
                                 uint32_t l3Id) {
    if (bdId != 0)
        fb.reg(4, bdId);
    if (l3Id != 0)
        fb.reg(6, l3Id);
    return fb;
}

static FlowBuilder& matchDestArp(FlowBuilder& fb, const address& ip,
                                 uint32_t bdId, uint32_t l3Id,
                                 uint8_t prefixLen = 32) {
    fb.arpDst(ip, prefixLen)
        .proto(arp::op::REQUEST)
        .ethDst(packets::MAC_ADDR_BROADCAST);
    return matchDestDom(fb, bdId, l3Id);
}

static FlowBuilder& matchDestNd(FlowBuilder& fb, const address* ip,
                                uint32_t bdId, uint32_t rdId,
                                uint8_t type = ND_NEIGHBOR_SOLICIT) {
    matchDestDom(fb, bdId, rdId)
        .ethType(eth::type::IPV6)
        .proto(58)
        .tpSrc(type)
        .tpDst(0)
        .ethDst(packets::MAC_ADDR_MULTICAST, packets::MAC_ADDR_MULTICAST);

    if (ip) {
        fb.ndTarget(type, *ip);
    }
    return fb;
}

static FlowBuilder& matchFd(FlowBuilder& fb,
                            uint32_t fgrpId, bool broadcast,
                            uint8_t* dstMac = NULL) {
    fb.reg(5, fgrpId);
    if (dstMac)
        fb.ethDst(dstMac);
    else if (broadcast)
        fb.ethDst(packets::MAC_ADDR_MULTICAST, packets::MAC_ADDR_MULTICAST);
    return fb;
}

static FlowBuilder& matchSubnet(FlowBuilder& fb, uint32_t rdId,
                                uint16_t prioBase,
                                address& ip, uint8_t prefixLen, bool src) {
    fb.priority(prioBase + prefixLen)
        .reg(6, rdId);
    if (src) fb.ipSrc(ip, prefixLen);
    else fb.ipDst(ip, prefixLen);
    return fb;
}

static FlowBuilder& matchIcmpEchoReq(FlowBuilder& fb, bool v4) {
    return fb.ethType(v4 ? eth::type::IP : eth::type::IPV6)
        .proto(v4 ? 1 : 58)
        .tpSrc(v4 ? 8: 128)
        .tpDst(0);
}

// Action helper functions
static FlowBuilder& actionSource(FlowBuilder& fb, uint32_t epgId, uint32_t bdId,
                                 uint32_t fgrpId,  uint32_t l3Id,
                                 uint8_t nextTable
                                 = IntFlowManager::SERVICE_REV_TABLE_ID,
                                 IntFlowManager::EncapType encapType
                                 = IntFlowManager::ENCAP_NONE,
                                 bool setPolicyApplied = false)
{
    if (encapType == IntFlowManager::ENCAP_VLAN) {
        fb.action().popVlan();
    }

    fb.action()
        .reg(MFF_REG0, epgId)
        .reg(MFF_REG4, bdId)
        .reg(MFF_REG5, fgrpId)
        .reg(MFF_REG6, l3Id);
    if (setPolicyApplied) {
        fb.action().metadata(flow::meta::POLICY_APPLIED,
                             flow::meta::POLICY_APPLIED);
    }
    fb.action().go(nextTable);
    return fb;
}

void IntFlowManager::actionTunnelMetadata(ActionBuilder& ab,
                                          IntFlowManager::EncapType type,
                                          const optional<address>& tunDst) {
    switch (type) {
    case IntFlowManager::ENCAP_VLAN:
        ab.pushVlan();
        ab.regMove(MFF_REG0, MFF_VLAN_VID);
        break;
    case IntFlowManager::ENCAP_VXLAN:
        ab.regMove(MFF_REG0, MFF_TUN_ID);
        if (tunDst) {
            if (tunDst->is_v4()) {
                ab.reg(MFF_TUN_DST, tunDst->to_v4().to_ulong());
            } else {
                // this should be unreachable
                LOG(WARNING) << "Ipv6 tunnel destination unsupported";
            }
        } else {
            ab.regMove(MFF_REG7, MFF_TUN_DST);
        }
        // fall through
    case IntFlowManager::ENCAP_IVXLAN:
        break;
    default:
        break;
    }
}

static FlowBuilder& actionDestEpArp(FlowBuilder& fb,
                                    uint32_t epgId, uint32_t port,
                                    const uint8_t* dstMac) {
    fb.action().reg(MFF_REG2, epgId)
        .reg(MFF_REG7, port)
        .ethDst(dstMac)
        .go(IntFlowManager::POL_TABLE_ID);
    return fb;
}

static FlowBuilder& actionOutputToEPGTunnel(FlowBuilder& fb) {
    fb.action()
        .metadata(flow::meta::out::TUNNEL, flow::meta::out::MASK)
        .go(IntFlowManager::STATS_TABLE_ID);
    return fb;
}

static FlowBuilder& actionArpReply(FlowBuilder& fb, const uint8_t *mac,
                                   const address& ip,
                                   IntFlowManager::EncapType type
                                   = IntFlowManager::ENCAP_NONE) {
    fb.action()
        .regMove(MFF_ETH_SRC, MFF_ETH_DST)
        .reg(MFF_ETH_SRC, mac)
        .reg16(MFF_ARP_OP, arp::op::REPLY)
        .regMove(MFF_ARP_SHA, MFF_ARP_THA)
        .reg(MFF_ARP_SHA, mac)
        .regMove(MFF_ARP_SPA, MFF_ARP_TPA)
        .reg(MFF_ARP_SPA, ip.to_v4().to_ulong());
    switch (type) {
    case IntFlowManager::ENCAP_VLAN:
        fb.action()
            .pushVlan()
            .regMove(MFF_REG0, MFF_VLAN_VID);
        break;
    case IntFlowManager::ENCAP_VXLAN:
    case IntFlowManager::ENCAP_IVXLAN:
        fb.action()
            .regMove(MFF_TUN_SRC, MFF_TUN_DST);
        break;
    default:
        break;
    }
    fb.action().output(OFPP_IN_PORT);
    return fb;
}

static FlowBuilder& actionRevNatDest(FlowBuilder& fb, uint32_t epgVnid,
                                     uint32_t bdId, uint32_t fgrpId,
                                     uint32_t rdId, uint32_t ofPort) {
    fb.action()
        .reg(MFF_REG2, epgVnid)
        .reg(MFF_REG4, bdId)
        .reg(MFF_REG5, fgrpId)
        .reg(MFF_REG6, rdId)
        .reg(MFF_REG7, ofPort)
        .metadata(flow::meta::ROUTED, flow::meta::ROUTED)
        .go(IntFlowManager::NAT_IN_TABLE_ID);
    return fb;
}

static FlowBuilder& actionController(FlowBuilder& fb, uint32_t epgId = 0,
                                     uint64_t metadata = 0) {
    if (epgId != 0)
        fb.action().reg(MFF_REG0, epgId);
    if (metadata)
        fb.action().reg64(MFF_METADATA, metadata);
    fb.action().controller();
    return fb;
}

static FlowBuilder& actionSecAllow(FlowBuilder& fb) {
    fb.action().go(IntFlowManager::SRC_TABLE_ID);
    return fb;
}

// Flow creation helpers

static void flowsRevNatICMP(FlowEntryList& el, bool v4, uint8_t type) {
    FlowBuilder fb;
    fb.priority(10)
        .cookie(v4 ? flow::cookie::ICMP_ERROR_V4 : flow::cookie::ICMP_ERROR_V6)
        .metadata(flow::meta::out::REV_NAT, flow::meta::out::MASK)
        .action().controller();

    if (v4) {
        fb.ethType(eth::type::IP).proto(1 /* ICMP */).tpSrc(type);
    } else {
        fb.ethType(eth::type::IPV6).proto(58 /* ICMP */).tpSrc(type);
    }
    fb.build(el);
}

static void flowsProxyDiscovery(FlowEntryList& el,
                                uint16_t priority,
                                const address& ipAddr,
                                const uint8_t* macAddr,
                                uint32_t epgVnid, uint32_t rdId,
                                uint32_t bdId,
                                bool router,
                                const uint8_t* matchSourceMac,
                                uint32_t tunPort,
                                IntFlowManager::EncapType encapType,
                                bool directDelivery = false,
                                uint32_t dropInPort = OFPP_NONE) {
    if (ipAddr.is_v4()) {
        if (tunPort != OFPP_NONE &&
            encapType != IntFlowManager::ENCAP_NONE) {
            FlowBuilder proxyArpTun;
            if (matchSourceMac)
                proxyArpTun.ethSrc(matchSourceMac);
            matchDestArp(proxyArpTun.priority(priority+1).inPort(tunPort),
                         ipAddr, bdId, rdId);
            actionArpReply(proxyArpTun, macAddr, ipAddr,
                           encapType)
                .build(el);
        }
        {
            FlowBuilder proxyArp;
            // Match on inPort to ignore this arp (prio + 1)
            if (dropInPort != OFPP_NONE)
                proxyArp.priority(priority+1).inPort(dropInPort);
            else
                proxyArp.priority(priority);

            if (matchSourceMac)
                proxyArp.ethSrc(matchSourceMac);
            matchDestArp(proxyArp, ipAddr, bdId, rdId);

            // Drop the arp request on this inPort
            if (dropInPort != OFPP_NONE)
                proxyArp.build(el);
            else
                actionArpReply(proxyArp, macAddr, ipAddr)
                    .build(el);
        }
    } else {
        // pass MAC address in flow metadata
        uint64_t metadata = 0;
        memcpy(&metadata, macAddr, 6);
        ((uint8_t*)&metadata)[7] = 1;
        if (router)
            ((uint8_t*)&metadata)[7] = 3;
        if (directDelivery)
            ((uint8_t*)&metadata)[6] = 1;

        FlowBuilder proxyND;
        if (matchSourceMac)
            proxyND.ethSrc(matchSourceMac);
        matchDestNd(proxyND.priority(priority).cookie(flow::cookie::NEIGH_DISC),
                    &ipAddr, bdId, rdId);
        actionController(proxyND, epgVnid, metadata);
        proxyND.build(el);
    }
}

static void flowsProxyDiscovery(IntFlowManager& flowMgr,
                                FlowEntryList& el,
                                uint16_t priority,
                                const address& ipAddr,
                                const uint8_t* macAddr,
                                uint32_t epgVnid, uint32_t rdId,
                                uint32_t bdId,
                                bool router = false,
                                const uint8_t* matchSourceMac = NULL) {
    flowsProxyDiscovery(el, priority, ipAddr, macAddr, epgVnid, rdId,
                        bdId, router, matchSourceMac, flowMgr.getTunnelPort(),
                        (epgVnid != 0)
                        ? flowMgr.getEncapType() : IntFlowManager::ENCAP_NONE);
}

static void flowsProxyICMP(FlowEntryList& el,
                           uint16_t priority,
                           const address& ipAddr,
                           uint32_t bdId,
                           uint32_t l3Id) {
    FlowBuilder fb;
    bool v4 = ipAddr.is_v4();
    matchIcmpEchoReq(fb, v4).priority(priority)
        .ipDst(ipAddr)
        .cookie(v4 ? flow::cookie::ICMP_ECHO_V4 : flow::cookie::ICMP_ECHO_V6);
    matchDestDom(fb, bdId, l3Id);
    actionController(fb, 0, ovs_htonll(0x100));
    fb.build(el);
}

static void flowsIpm(IntFlowManager& flowMgr,
                     FlowEntryList& elSrc, FlowEntryList& elBridgeDst,
                     FlowEntryList& elRouteDst,FlowEntryList& elOutput,
                     const uint8_t* macAddr, uint32_t ofPort,
                     uint32_t epgVnid, uint32_t rdId,
                     uint32_t bdId, uint32_t fgrpId,
                     uint32_t fepgVnid, uint32_t frdId,
                     uint32_t fbdId, uint32_t ffdId,
                     address& mappedIp, address& floatingIp,
                     uint32_t nextHopPort, const uint8_t* nextHopMac) {
    const uint8_t* effNextHopMac =
        nextHopMac ? nextHopMac : flowMgr.getRouterMacAddr();

    if (!floatingIp.is_unspecified()) {
        {
            // floating IP destination within the same EPG
            // Set up reverse DNAT
            FlowBuilder ipmRoute;
            matchDestDom(ipmRoute.priority(452)
                         .ipDst(floatingIp).reg(0, fepgVnid),
                         0, frdId)
                .action()
                .ethSrc(flowMgr.getRouterMacAddr()).ethDst(macAddr)
                .ipDst(mappedIp).decTtl();

            actionRevNatDest(ipmRoute, epgVnid, bdId,
                             fgrpId, rdId, ofPort);
            ipmRoute.build(elRouteDst);
        }
        {
            // Floating IP destination across EPGs
            // Apply policy for source EPG -> floating IP EPG
            // then resubmit with source EPG set to floating
            // IP EPG
            FlowBuilder ipmResub;
            matchDestDom(ipmResub.priority(450).ipDst(floatingIp),
                         0, frdId);
            ipmResub.action()
                .reg(MFF_REG2, fepgVnid)
                .reg(MFF_REG7, fepgVnid)
                .metadata(flow::meta::out::RESUBMIT_DST,
                          flow::meta::out::MASK)
                .go(IntFlowManager::POL_TABLE_ID);
            ipmResub.build(elRouteDst);
        }
        // Reply to discovery requests for the floating IP
        // address
        flowsProxyDiscovery(flowMgr, elBridgeDst, 20, floatingIp, macAddr,
                            fepgVnid, frdId, fbdId);
    }
    {
        // Apply NAT action in output table
        FlowBuilder ipmNatOut;
        ipmNatOut.priority(10)
            .metadata(flow::meta::out::NAT, flow::meta::out::MASK)
            .reg(6, rdId)
            .reg(7, fepgVnid)
            .ipSrc(mappedIp);
        ActionBuilder& ab = ipmNatOut.action();
        ab.ethSrc(macAddr).ethDst(effNextHopMac);
        if (!floatingIp.is_unspecified()) {
            ab.ipSrc(floatingIp);
        }
        ab.decTtl();

        if (nextHopPort == OFPP_NONE) {
            ab.reg(MFF_REG0, fepgVnid)
                .reg(MFF_REG4, fbdId)
                .reg(MFF_REG5, ffdId)
                .reg(MFF_REG6, frdId)
                .reg(MFF_REG7, (uint32_t)0)
                .reg64(MFF_METADATA, flow::meta::ROUTED)
                .resubmit(OFPP_IN_PORT, IntFlowManager::BRIDGE_TABLE_ID);
        } else {
            ab.reg(MFF_PKT_MARK, rdId)
                .output(nextHopPort);
        }
        ipmNatOut.build(elOutput);
    }

    // Handle traffic returning from a next hop interface
    if (nextHopPort != OFPP_NONE) {
        if (!floatingIp.is_unspecified()) {
            // reverse traffic from next hop interface where we
            // delivered with a DNAT to a floating IP.  We assume that
            // the destination IP is unique for a given next hop
            // interface.
            FlowBuilder ipmNextHopRev;
            ipmNextHopRev.priority(201).inPort(nextHopPort)
                .ethSrc(effNextHopMac).ipDst(floatingIp)
                .action()
                .ethSrc(flowMgr.getRouterMacAddr()).ethDst(macAddr)
                .ipDst(mappedIp).decTtl();

            actionRevNatDest(ipmNextHopRev, epgVnid, bdId,
                             fgrpId, rdId, ofPort);
            ipmNextHopRev.build(elSrc);
        }
        {
            // Reverse traffic from next hop interface where we
            // delivered with an SKB mark.  The SKB mark must be set
            // to the routing domain for the mapped destination IP
            // address
            FlowBuilder ipmNextHopRevMark;
            ipmNextHopRevMark.priority(200).inPort(nextHopPort)
                .ethSrc(effNextHopMac).mark(rdId).ipDst(mappedIp);
            if (nextHopMac)
                ipmNextHopRevMark.action().ethSrc(flowMgr.getRouterMacAddr());
            actionRevNatDest(ipmNextHopRevMark, epgVnid, bdId,
                             fgrpId, rdId, ofPort);
            ipmNextHopRevMark.build(elSrc);
        }
    }
}

static void flowsEndpointPortSec(FlowEntryList& elPortSec,
                                 const Endpoint& endPoint,
                                 uint32_t ofPort,
                                 bool hasMac,
                                 uint8_t* macAddr,
                                 const std::vector<address>& ipAddresses) {
    if (ofPort == OFPP_NONE)
        return;

    if (endPoint.isPromiscuousMode()) {
        // allow all packets from port
        actionSecAllow(FlowBuilder().priority(50).inPort(ofPort))
            .build(elPortSec);
    } else if (hasMac) {
        // allow L2 packets from port with EP MAC address
        actionSecAllow(FlowBuilder().priority(20)
                       .inPort(ofPort).ethSrc(macAddr))
            .build(elPortSec);
        if(endPoint.isExternal()) {
            actionSecAllow(FlowBuilder().priority(30)
                           .inPort(ofPort).ethSrc(macAddr)
                           .ethType(eth::type::IP))
                           .build(elPortSec);
            actionSecAllow(FlowBuilder().priority(30)
                           .inPort(ofPort).ethSrc(macAddr)
                           .ethType(eth::type::IPV6))
                           .build(elPortSec);
        }
        for (const address& ipAddr : ipAddresses) {
            if(!endPoint.isExternal()) {
                // Allow IPv4/IPv6 packets from port with EP IP address
                actionSecAllow(FlowBuilder().priority(30)
                               .inPort(ofPort).ethSrc(macAddr)
                               .ipSrc(ipAddr))
                    .build(elPortSec);
            }
            if (ipAddr.is_v4()) {
                // Allow ARP with correct source address
                actionSecAllow(FlowBuilder().priority(40)
                               .inPort(ofPort).ethSrc(macAddr)
                               .arpSrc(ipAddr))
                    .build(elPortSec);
            } else {
                // Allow neighbor advertisements with correct
                // source address
                actionSecAllow(FlowBuilder().priority(40)
                               .inPort(ofPort).ethSrc(macAddr)
                               .ndTarget(ND_NEIGHBOR_ADVERT, ipAddr))
                    .build(elPortSec);
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

        // Handle ARP/ND from "active" virtual IPs normally, that is
        // without generating a packet-in
        for (const address& ipAddr : ipAddresses) {
            if (!network::cidr_contains(vip_cidr, ipAddr)) {
                continue;
            }
            FlowBuilder active_vip;
            active_vip.priority(61).inPort(ofPort).ethSrc(vmac);
            if (ipAddr.is_v4()) {
                active_vip.arpSrc(ipAddr);
            } else {
                active_vip.ndTarget(ND_NEIGHBOR_ADVERT, ipAddr);
            }
            actionSecAllow(active_vip).build(elPortSec);
        }

        FlowBuilder vf;
        vf.priority(60).inPort(ofPort).ethSrc(vmac);

        if (vip_cidr.first.is_v4()) {
            vf.cookie(flow::cookie::VIRTUAL_IP_V4)
                .arpSrc(vip_cidr.first, vip_cidr.second);
        } else {
            vf.cookie(flow::cookie::VIRTUAL_IP_V6)
                .ndTarget(ND_NEIGHBOR_ADVERT,
                          vip_cidr.first, vip_cidr.second);
        }
        // AAP mode active-active skip controller for grat arp
        if (!endPoint.isAapModeAA())
            vf.action().controller();
        actionSecAllow(vf).build(elPortSec);

        // AAP mode active-active allow IPv4/IPv6 packets from
        // port with virtual ip address and endpoint macaddr
        if (hasMac && endPoint.isAapModeAA()) {
            actionSecAllow(FlowBuilder().priority(30)
                           .inPort(ofPort).ethSrc(vmac)
                           .ipSrc(vip_cidr.first, vip_cidr.second))
                .build(elPortSec);
        }
    }
}

static void flowsVirtualDhcp(FlowEntryList& elSrc, uint32_t ofPort,
                             uint8_t* macAddr, bool v4) {
    FlowBuilder fb;
    flowutils::match_dhcp_req(fb, v4);
    actionController(fb);
    fb.priority(35)
        .cookie(v4 ? flow::cookie::DHCP_V4 : flow::cookie::DHCP_V6)
        .inPort(ofPort)
        .ethSrc(macAddr)
        .build(elSrc);
}

static void flowsEndpointDHCPSource(IntFlowManager& flowMgr,
                                    FlowEntryList& elPortSec,
                                    FlowEntryList& elBridgeDst,
                                    const Endpoint& endPoint,
                                    uint32_t ofPort,
                                    bool hasMac,
                                    uint8_t* macAddr,
                                    bool virtualDHCPEnabled,
                                    bool hasForwardingInfo,
                                    uint32_t epgVnid,
                                    uint32_t rdId,
                                    uint32_t bdId) {
    if (ofPort == OFPP_NONE)
        return;

    if (virtualDHCPEnabled && hasMac) {
        optional<Endpoint::DHCPv4Config> v4c = endPoint.getDHCPv4Config();
        optional<Endpoint::DHCPv6Config> v6c = endPoint.getDHCPv6Config();

        if (v4c) {
            flowsVirtualDhcp(elPortSec, ofPort, macAddr, true);

            if (hasForwardingInfo) {
                address_v4 serverIp(packets::LINK_LOCAL_DHCP);
                if (v4c.get().getServerIp()) {
                    boost::system::error_code ec;
                    address_v4 sip =
                        address_v4::from_string(v4c.get().getServerIp().get(),
                                                ec);
                    if (ec) {
                        LOG(WARNING) << "Invalid DHCP server IP: "
                                     << v4c.get().getServerIp().get();
                    } else  {
                        serverIp = sip;
                    }
                }

                uint8_t serverMac[6];
                if (v4c.get().getServerMac()) {
                    v4c.get().getServerMac()->toUIntArray(serverMac);
                } else {
                    memcpy(serverMac, flowMgr.getDHCPMacAddr(),
                           sizeof(serverMac));
                }

                // Ignore arp requests on uplink interface.
                flowsProxyDiscovery(elBridgeDst, 51,
                                    serverIp, serverMac,
                                    epgVnid, rdId, bdId, false,
                                    NULL, OFPP_NONE,
                                    IntFlowManager::ENCAP_NONE,
                                    false, flowMgr.getTunnelPort());
                flowsProxyDiscovery(elBridgeDst, 51,
                                    serverIp, serverMac,
                                    epgVnid, rdId, bdId, false,
                                    NULL, OFPP_NONE,
                                    IntFlowManager::ENCAP_NONE);
            }
        }
        if (v6c) {
            flowsVirtualDhcp(elPortSec, ofPort, macAddr, false);

            if (hasForwardingInfo) {
                // IPv6 link-local address made from the DHCP MAC
                address_v6 serverIp = network::
                    construct_auto_ip_addr(address_v6::from_string("fe80::"),
                                           flowMgr.getDHCPMacAddr());
                flowsProxyDiscovery(elBridgeDst, 51,
                                    serverIp, flowMgr.getDHCPMacAddr(),
                                    epgVnid, rdId, bdId, false,
                                    NULL, OFPP_NONE,
                                    IntFlowManager::ENCAP_NONE);
            }
        }

        for (const Endpoint::virt_ip_t& vip :
                 endPoint.getVirtualIPs()) {
            if (endPoint.getMAC().get() == vip.first) continue;
            network::cidr_t vip_cidr;
            if (!network::cidr_from_string(vip.second, vip_cidr)) {
                continue;
            }
            address& addr = vip_cidr.first;
            uint8_t vmacAddr[6];
            vip.first.toUIntArray(vmacAddr);

            if (v4c && addr.is_v4())
                flowsVirtualDhcp(elPortSec, ofPort, vmacAddr, true);
            else if (v6c && addr.is_v6())
                flowsVirtualDhcp(elPortSec, ofPort, vmacAddr, false);
        }
    }
}

static void flowsEndpointSource(FlowEntryList& elSrc,
                                const Endpoint& endPoint,
                                uint32_t ofPort,
                                bool hasMac,
                                uint8_t* macAddr,
                                uint8_t unkFloodMode,
                                uint8_t bcastFloodMode,
                                uint32_t epgVnid,
                                uint32_t bdId,
                                uint32_t fgrpId,
                                uint32_t rdId) {
    if (ofPort == OFPP_NONE)
        return;

    if (hasMac) {
        FlowBuilder l2Classify;
        l2Classify.priority(140)
                  .inPort(ofPort).ethSrc(macAddr);
        // Map on L2. Port security rules filter L2
        // and L3 before we reach this table, except
        // for promiscuous endpoints.
        if (!endPoint.isNatMode()) {
            actionSource(l2Classify, epgVnid, bdId, fgrpId, rdId)
                .build(elSrc);
            return;
        }

        // Map on L2 and L3 for Nat endpoints
        // This prevents reply traffic from being
        // forwarded to uplink when EP is gone.
        l2Classify.ethType(eth::type::ARP);
        actionSource(l2Classify, epgVnid, bdId, fgrpId, rdId)
            .build(elSrc);

        for (const string& ipStr : endPoint.getIPs()) {

            boost::system::error_code ec;

            address addr = address::from_string(ipStr, ec);
            if (ec) {
                LOG(WARNING) << "Invalid endpoint IP: "
                             << ipStr << ": " << ec.message();
                continue;
            }
            actionSource(FlowBuilder().priority(140)
                         .ipSrc(addr)
                         .inPort(ofPort).ethSrc(macAddr),
                         epgVnid, bdId, fgrpId, rdId)
                .build(elSrc);
        }
    }
}

static void matchServiceProto (FlowBuilder& flow, uint8_t proto,
                               const Service::ServiceMapping& sm,
                               bool forward) {
    if (!proto) return;
    flow.proto(proto);

    if (!sm.getServicePort()) return;

    uint16_t s_port = sm.getServicePort().get();
    uint16_t nh_port = s_port;
    if (sm.getNextHopPort())
        nh_port = sm.getNextHopPort().get();

    if (forward) {
        // post DNAT of service-ip/service-port to target pod-ip/pod-port
        flow.tpDst(nh_port);
    } else {
        // post SNAT of target pod-ip/pod-port to service-ip/service-port
        flow.tpSrc(s_port);
    }
}

static void matchActionServiceProto(FlowBuilder& flow, uint8_t proto,
                                    const Service::ServiceMapping& sm,
                                    bool forward, bool applyAction) {
    if (!proto) return;
    flow.proto(proto);

    if (!sm.getServicePort()) return;

    uint16_t s_port = sm.getServicePort().get();
    uint16_t nh_port = s_port;
    if (sm.getNextHopPort())
        nh_port = sm.getNextHopPort().get();

    if (nh_port == s_port) applyAction = false;

    if (forward) {
        // pre DNAT of servic-ip/service-port to target pod-ip/pod-port
        flow.tpDst(s_port);
        if (applyAction)
            flow.action().l4Dst(nh_port, proto);
    } else {
        // pre SNAT of target pod-ip/pod-port to service-ip/service-port
        flow.tpSrc(nh_port);
        if (applyAction)
            flow.action().l4Src(s_port, proto);
    }
}

static void flowRevMapCt(FlowEntryList& serviceRevFlows,
                         uint16_t priority,
                         const Service::ServiceMapping& sm,
                         const address& nextHopAddr,
                         uint32_t rdId,
                         uint16_t zoneId,
                         uint8_t proto,
                         uint32_t tunPort,
                         IntFlowManager::EncapType encapType) {
    FlowBuilder ipRevMapCt;
    matchDestDom(ipRevMapCt, 0, rdId);
    matchActionServiceProto(ipRevMapCt, proto, sm,
                            false, false);
    ipRevMapCt.conntrackState(0, FlowBuilder::CT_TRACKED)
        .priority(priority)
        .ipSrc(nextHopAddr);
    if (encapType == IntFlowManager::ENCAP_VLAN) {
        ipRevMapCt.inPort(tunPort);
        ipRevMapCt.action()
            .pushVlan()
            .regMove(MFF_REG0, MFF_VLAN_VID);
    }
    ipRevMapCt.action()
        .conntrack(0, static_cast<mf_field_id>(0),
                   zoneId, IntFlowManager::SRC_TABLE_ID);
    ipRevMapCt.build(serviceRevFlows);
}

void IntFlowManager::handleRemoteEndpointUpdate(const string& uuid) {
    LOG(DEBUG) << "Updating remote endpoint " << uuid;

    optional<shared_ptr<modelgbp::inv::RemoteInventoryEp>> ep =
        modelgbp::inv::RemoteInventoryEp::resolve(agent.getFramework(), uuid);

    if (!ep || (encapType == ENCAP_VLAN || encapType == ENCAP_NONE)) {
        switchManager.clearFlows(uuid, BRIDGE_TABLE_ID);
        switchManager.clearFlows(uuid, ROUTE_TABLE_ID);
        return;
    }

    // Get remote endpoint MAC
    uint8_t macAddr[6];
    bool hasMac = ep.get()->isMacSet();
    if (hasMac)
        ep.get()->getMac().get().toUIntArray(macAddr);

    boost::system::error_code ec;

    // Get remote tunnel destination
    optional<address> tunDst;
    bool hasTunDest = false;
    if (ep.get()->isNextHopTunnelSet()) {
        string ipStr = ep.get()->getNextHopTunnel().get();
        tunDst = address::from_string(ipStr, ec);
        if (ec || !tunDst->is_v4()) {
            LOG(WARNING) << "Invalid remote tunnel destination IP: "
                         << ipStr << ": " << ec.message();
        } else {
            hasTunDest = true;
        }
    }

    uint32_t epgVnid = 0, rdId = 0, bdId = 0, fgrpId = 0;
    optional<URI> epgURI, fgrpURI, bdURI, rdURI;
    auto epgRef = ep.get()->resolveInvRemoteInventoryEpToGroupRSrc();
    if (epgRef) {
        epgURI = epgRef.get()->getTargetURI();
    }

    bool hasForwardingInfo = false;
    if (epgURI && getGroupForwardingInfo(epgURI.get(), epgVnid, rdURI,
                                         rdId, bdURI, bdId, fgrpURI, fgrpId)) {
        hasForwardingInfo = true;
    }

    FlowEntryList elBridgeDst;
    FlowEntryList elRouteDst;
    std::vector<std::shared_ptr<modelgbp::inv::RemoteIp>> invIps;

    if (hasForwardingInfo) {
        FlowBuilder bridgeFlow;
        uint32_t outReg = 0;
        uint64_t meta;

        if (hasTunDest) {
            outReg = tunDst->to_v4().to_ulong();
            meta = flow::meta::out::REMOTE_TUNNEL;
        } else {
            meta = flow::meta::out::HOST_ACCESS;
        }

        if (hasMac) {
            matchDestDom(bridgeFlow, bdId, 0)
                .priority(10)
                .ethDst(macAddr)
                .action()
                .reg(MFF_REG2, epgVnid)
                .reg(MFF_REG7, outReg)
                .metadata(meta, flow::meta::out::MASK)
                .go(POL_TABLE_ID)
                .parent().build(elBridgeDst);
        }

        // Get remote endpoint IP addresses
        ep.get()->resolveInvRemoteIp(invIps);
        for (const auto& invIp : invIps) {
            if (!invIp->isIpSet()) continue;

            address addr = address::from_string(invIp->getIp().get(), ec);
            if (ec) {
                LOG(WARNING) << "Invalid remote endpoint IP: "
                             << invIp->getIp().get() << ": " << ec.message();
                continue;
            }

            uint8_t prefix;
            if (invIp->isPrefixLenSet()) {
                prefix = invIp->getPrefixLen().get();
            } else {
                if (addr.is_v4())
                    prefix = 32;
                else
                    prefix = 128;
            }

            FlowBuilder routeFlow;
            FlowBuilder proxyArp;
            if (hasTunDest) {
                matchDestDom(routeFlow, 0, rdId)
                    .priority(500)
                    .ethDst(getRouterMacAddr())
                    .ipDst(addr, prefix)
                    .action()
                    .reg(MFF_REG2, epgVnid)
                    .reg(MFF_REG7, outReg)
                    .metadata(meta, flow::meta::out::MASK)
                    .go(POL_TABLE_ID)
                    .parent().build(elRouteDst);
            } else {
                routeFlow
                    .priority(10 + prefix)
                    .ethType(eth::type::IP)
                    .reg(6, rdId)
                    .ethDst(getRouterMacAddr())
                    .ipDst(addr, prefix)
                    .action()
                    .reg(MFF_REG2, epgVnid)
                    .metadata(meta, flow::meta::out::MASK)
                    .go(POL_TABLE_ID)
                    .parent().build(elRouteDst);
            }

            if (addr.is_v4()) {
                if (hasMac && prefix == 32) {
                    // Resolve inter-node arp without going to leaf
                    matchDestArp(proxyArp.priority(40), addr, bdId, rdId);
                    actionArpReply(proxyArp, macAddr, addr)
                        .build(elBridgeDst);
                }
                if (invIp->isNextHopIPSet() && invIp->isNextHopMacSet()) {
                    uint8_t nextHopMac[6];
                    address nextHopIP =
                        address::from_string(invIp->getNextHopIP().get(), ec);
                    if (ec) continue;
                    invIp->getNextHopMac().get().toUIntArray(nextHopMac);
                    // Resolve arp to CSR Gateway
                    matchDestArp(proxyArp.priority(40), addr, bdId, rdId,
                                 prefix);
                    actionArpReply(proxyArp, nextHopMac, nextHopIP)
                        .build(elBridgeDst);
                }
            }
        }
    }

    switchManager.writeFlow(uuid, BRIDGE_TABLE_ID, elBridgeDst);
    switchManager.writeFlow(uuid, ROUTE_TABLE_ID, elRouteDst);
}

static void flowsEndpointPortRangeSNAT(const Snat& as,
                                       const address& nwSrc,
                                       const address& nwDst,
                                       uint8_t prefixlen,
                                       uint16_t start,
                                       uint16_t end,
                                       uint32_t rdId,
                                       uint16_t zoneId,
                                       uint32_t ofPort,
                                       int& count,
                                       FlowEntryList& elSnat) {
    address snatIp = address::from_string(as.getSnatIP());
    ActionBuilder fna;
    fna.nat(snatIp, start, end, true);

    FlowBuilder fsn;
    fsn.priority(300 - count)
       .reg(6, rdId)
       .ipSrc(nwSrc)
       .ipDst(nwDst, prefixlen)
       .action()
       .conntrack(ActionBuilder::CT_COMMIT,
                  static_cast<mf_field_id>(0),
                  zoneId, 0xff, 0, fna);
    if (as.getIfaceVlan())
        fsn.action()
           .pushVlan()
           .setVlanVid(as.getIfaceVlan().get());
    fsn.action()
       .output(ofPort)
       .parent().build(elSnat);
}

/**
 * Endpoint specific SNAT flows (EP -> ext world)
 *
 * UN-SNAT flows are added via handleSnatUpdate
 * by Snat Manager
 */
static void flowsEndpointSNAT(SnatManager& snatMgr,
                              const Snat& as,
                              uint32_t ofPort,
                              uint32_t rdId,
                              uint16_t zoneId,
                              const Endpoint& endPoint,
                              const string& uuid,
                              FlowEntryList& elRouteDst,
                              FlowEntryList& elSnat,
                              uint32_t epPort,
                              const uint8_t *epMac,
                              int& count,
                              FlowEntryList& elRevSnat) {

    boost::system::error_code ec;
    address nwDst;
    uint8_t prefixlen = 0;

    for (const string& ipStr : endPoint.getIPs()) {
        address nwSrc =
            address::from_string(ipStr, ec);
        if (ec) {
            LOG(WARNING) << "Invalid endpoint IP: "
                         << ipStr << ": " << ec.message();
            continue;
        }

        // Program route table flow to forward to snat table
        for (auto it = as.getDest().begin(); it != as.getDest().end(); ++it) {
            if (count >= 32) {
                LOG(WARNING) << "Limit 32 reached when adding SNAT for rdId "
                             << rdId << " src " << nwSrc << " dst " << *it
                             << " , hence skipping";
                continue;
            }
            FlowBuilder frd;
            frd.priority(300 - count)
               .reg(6, rdId)
               .ipSrc(nwSrc);
            std::stringstream ss(*it);
            std::string addr, mask;
            std::getline(ss, addr, '/');
            std::getline(ss, mask, '/');
            nwDst =
                address::from_string(addr, ec);
            if (ec) {
                LOG(WARNING) << "Invalid SNAT destination: "
                             << addr << "/" << mask
                             << ": " << ec.message();
            } else {
                if (mask == "")
                    prefixlen = 32;
                else
                    prefixlen = std::stoi(mask);
                frd.ipDst(nwDst, prefixlen);
            }
            frd.action()
               .go(IntFlowManager::SNAT_TABLE_ID)
               .parent().build(elRouteDst);

            // Program snat table flow to snat and output
            boost::optional<Snat::PortRanges> prs = as.getPortRanges("local");
            if (prs != boost::none && prs.get().size() > 0) {
                for (const auto& pr : prs.get()) {
                    flowsEndpointPortRangeSNAT(as, nwSrc, nwDst, prefixlen,
                                               pr.start, pr.end,
                                               rdId, zoneId, ofPort, count,
                                               elSnat);
                }
            }
            count++;
        }

        // Program reverse flows to reach this endpoint
        FlowBuilder()
            .priority(10)
            .ipDst(nwSrc)
            .conntrackState(FlowBuilder::CT_TRACKED |
                            FlowBuilder::CT_ESTABLISHED,
                            FlowBuilder::CT_TRACKED |
                            FlowBuilder::CT_ESTABLISHED |
                            FlowBuilder::CT_INVALID |
                            FlowBuilder::CT_NEW)
            .action().ethDst(epMac).output(epPort)
            .parent().build(elRevSnat);
    }
}

void IntFlowManager::handleEndpointUpdate(const string& uuid) {
    LOG(DEBUG) << "Updating endpoint " << uuid;

    EndpointManager& epMgr = agent.getEndpointManager();
    shared_ptr<const Endpoint> epWrapper = epMgr.getEndpoint(uuid);

    if (!epWrapper) {   // EP removed
        switchManager.clearFlows(uuid, SEC_TABLE_ID);
        switchManager.clearFlows(uuid, SRC_TABLE_ID);
        switchManager.clearFlows(uuid, BRIDGE_TABLE_ID);
        switchManager.clearFlows(uuid, ROUTE_TABLE_ID);
        switchManager.clearFlows(uuid, SNAT_TABLE_ID);
        switchManager.clearFlows(uuid, SNAT_REV_TABLE_ID);
        switchManager.clearFlows(uuid, SERVICE_DST_TABLE_ID);
        switchManager.clearFlows(uuid, OUT_TABLE_ID);
        removeEndpointFromFloodGroup(uuid);
        agent.getSnatManager().delEndpoint(uuid);
        updateSvcStatsFlows(uuid, false, false);
        return;
    }
    const Endpoint& endPoint = *epWrapper.get();
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

    uint32_t ofPort = OFPP_NONE;
    const optional<string>& ofPortName = endPoint.getInterfaceName();
    if (ofPortName) {
        ofPort = switchManager.getPortMapper().FindPort(ofPortName.get());
    }

    FlowEntryList elPortSec;
    FlowEntryList elSrc;
    FlowEntryList elBridgeDst;
    FlowEntryList elRouteDst;
    FlowEntryList elSnat;
    FlowEntryList elRevSnat;
    FlowEntryList elServiceMap;
    FlowEntryList elOutput;

    optional<URI> epgURI = epMgr.getComputedEPG(uuid);
    bool hasForwardingInfo = false;
    uint32_t epgVnid = 0, rdId = 0, bdId = 0, fgrpId = 0;
    optional<URI> fgrpURI, bdURI, rdURI;
    optional<shared_ptr<FloodDomain> > fd;

    uint8_t arpMode = AddressResModeEnumT::CONST_UNICAST;
    uint8_t ndMode = AddressResModeEnumT::CONST_UNICAST;
    uint8_t unkFloodMode = UnknownFloodModeEnumT::CONST_DROP;
    uint8_t bcastFloodMode = BcastFloodModeEnumT::CONST_NORMAL;

    if (epgURI && getGroupForwardingInfo(epgURI.get(), epgVnid, rdURI,
                                         rdId, bdURI, bdId, fgrpURI, fgrpId)) {
        hasForwardingInfo = true;
    }

    if(endPoint.isExternal()) {
        LOG(INFO) << "External endpoint update";
        arpMode = AddressResModeEnumT::CONST_FLOOD;
        ndMode = AddressResModeEnumT::CONST_FLOOD;
        unkFloodMode = UnknownFloodModeEnumT::CONST_FLOOD;
        bcastFloodMode = BcastFloodModeEnumT::CONST_NORMAL;
        hasForwardingInfo = true;
    } else {
        // Add stats flows for service metric collection
        updateSvcStatsFlows(uuid, false, true);

        if (hasForwardingInfo)
            fd = agent.getPolicyManager().getFDForGroup(epgURI.get());

        if (fd) {
            // Irrespective of flooding scope (epg vs. flood-domain), the
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
        }
    }

    // Virtual DHCP is allowed even without forwarding resolution
    flowsEndpointDHCPSource(*this, elPortSec, elBridgeDst, endPoint, ofPort,
                            hasMac, macAddr, virtualDHCPEnabled,
                            hasForwardingInfo, epgVnid, rdId, bdId);

    /* Add ARP responder for veth_host */
    if (uuid.find("veth_host_ac") != std::string::npos) {
        for (const string& ipStr : endPoint.getIPs()) {
            address ipa = address::from_string(ipStr, ec);
            if (ec) {
                LOG(WARNING) << "Invalid endpoint IP: "
                             << ipStr << ": " << ec.message();
                continue;
            } else {
                LOG(DEBUG) << "Found endpoint IP: "
                           << ipStr;
            }
            FlowBuilder proxyArp;
            proxyArp.priority(41).inPort(ofPort)
                .ethSrc(macAddr).arpSrc(ipa)
                .proto(arp::op::REQUEST)
                .ethDst(packets::MAC_ADDR_BROADCAST)
                .action()
                    .regMove(MFF_ETH_SRC, MFF_ETH_DST)
                    .reg(MFF_ETH_SRC, getRouterMacAddr())
                    .reg16(MFF_ARP_OP, arp::op::REPLY)
                    .regMove(MFF_ARP_SHA, MFF_ARP_THA)
                    .reg(MFF_ARP_SHA, getRouterMacAddr())
                    .regMove(MFF_ARP_TPA, MFF_ARP_SPA)
                    .reg(MFF_ARP_TPA, ipa.to_v4().to_ulong())
                    .output(OFPP_IN_PORT)
                    .parent().build(elPortSec);
        }
        uint16_t zoneId = ctZoneManager.getId("veth_host_ac");
        // Allow traffic from pod to external ip
        // This traffic will go back to access bridge
        // then to outside world via veth_host_ac
        FlowBuilder()
            .priority(15)
            .ethType(eth::type::IP)
            .metadata(flow::meta::out::HOST_ACCESS,
                      flow::meta::out::MASK)
            .action()
                .ethSrc(getRouterMacAddr())
                .ethDst(macAddr)
                .decTtl()
                .conntrack(ActionBuilder::CT_COMMIT,
                           static_cast<mf_field_id>(0),
                           zoneId, 0xff)
                .output(ofPort)
                .parent().build(elOutput);

        // Allow reverse traffic from external ips
        // to reach the pod. iptables conntrack
        // rules ensure only related or established
        // traffic is sent to us. We check it as
        // well. prio > 25 = drop priority
        FlowBuilder()
            .priority(26)
                .ethType(eth::type::IP)
                .inPort(ofPort)
                .ethSrc(macAddr)
                .action()
                    .go(SRC_TABLE_ID)
                    .parent().build(elPortSec);
    }

    if (hasForwardingInfo) {
        /* Port security flows */
        flowsEndpointPortSec(elPortSec, endPoint, ofPort,
                             hasMac, macAddr, ipAddresses);

        /* Source Table flows; applicable only to local endpoints */
        flowsEndpointSource(elSrc, endPoint, ofPort,
                            hasMac, macAddr, unkFloodMode, bcastFloodMode,
                            epgVnid, bdId, fgrpId, rdId);

        /* Bridge, route, and output flows */
        if (bdId != 0 && hasMac && ofPort != OFPP_NONE) {
            FlowBuilder().priority(10).ethDst(macAddr).reg(4, bdId)
                .action()
                .reg(MFF_REG2, epgVnid)
                .reg(MFF_REG7, ofPort)
                .go(POL_TABLE_ID)
                .parent().build(elBridgeDst);
        }

        if (rdId != 0 && bdId != 0 && ofPort != OFPP_NONE) {
            uint8_t routingMode =
                agent.getPolicyManager().getEffectiveRoutingMode(epgURI.get());

            if (virtualRouterEnabled && hasMac &&
                routingMode == RoutingModeEnumT::CONST_ENABLED) {
                for (const address& ipAddr : ipAddresses) {
                    if (endPoint.isDiscoveryProxyMode()) {
                        // Auto-reply to ARP and NDP requests for endpoint
                        // IP addresses
                        flowsProxyDiscovery(*this, elBridgeDst, 20, ipAddr,
                                            macAddr, epgVnid, rdId, bdId);
                    } else {
                        if (arpMode != AddressResModeEnumT::CONST_FLOOD &&
                            ipAddr.is_v4()) {
                            FlowBuilder e1;
                            matchDestArp(e1, ipAddr, bdId, rdId);
                            if (arpMode == AddressResModeEnumT::CONST_UNICAST) {
                                // ARP optimization: broadcast -> unicast
                                actionDestEpArp(e1, epgVnid, ofPort, macAddr);
                            }
                            // else drop the ARP packet
                            e1.priority(20)
                                .build(elBridgeDst);
                        }

                        if (ndMode != AddressResModeEnumT::CONST_FLOOD &&
                            ipAddr.is_v6()) {
                            FlowBuilder e1;
                            matchDestNd(e1, &ipAddr, bdId, rdId);
                            if (ndMode == AddressResModeEnumT::CONST_UNICAST) {
                                // neighbor discovery optimization:
                                // broadcast -> unicast
                                actionDestEpArp(e1, epgVnid, ofPort, macAddr);
                            }
                            // else drop the ND packet
                            e1.priority(20)
                                .build(elBridgeDst);
                        }
                    }

                    if (network::is_link_local(ipAddr))
                        continue;

                    {
                        FlowBuilder e0;
                        matchDestDom(e0, 0, rdId);
                        e0.priority(500)
                            .ethDst(getRouterMacAddr())
                            .ipDst(ipAddr)
                            .action()
                            .reg(MFF_REG2, epgVnid)
                            .reg(MFF_REG7, ofPort)
                            .ethSrc(getRouterMacAddr())
                            .ethDst(macAddr)
                            .decTtl()
                            .metadata(flow::meta::ROUTED, flow::meta::ROUTED)
                            .go(POL_TABLE_ID)
                            .parent().build(elRouteDst);
                    }

                }

                // virtual ip addresses in active-active AAP mode
                if (endPoint.isAapModeAA()) {
                    for (const Endpoint::virt_ip_t& vip : endPoint.getVirtualIPs()) {
                        network::cidr_t vip_cidr;
                        if (!network::cidr_from_string(vip.second, vip_cidr)) {
                            LOG(WARNING) << "Invalid endpoint VIP (CIDR): " << vip.second;
                            continue;
                        }
                        uint8_t vmac[6];
                        vip.first.toUIntArray(vmac);

                        FlowBuilder e0;
                        matchDestDom(e0, 0, rdId);
                        e0.priority(500)
                            .ethDst(vmac)
                            .ipDst(vip_cidr.first, vip_cidr.second)
                            .action()
                            .reg(MFF_REG2, epgVnid)
                            .reg(MFF_REG7, ofPort)
                            .metadata(flow::meta::ROUTED, flow::meta::ROUTED)
                            .go(POL_TABLE_ID)
                            .parent().build(elRouteDst);
                    }
                }

                // IP address mappings
                for(const Endpoint::IPAddressMapping& ipm :
                        endPoint.getIPAddressMappings()) {
                    if (!ipm.getMappedIP() || !ipm.getEgURI())
                        continue;

                    address mappedIp =
                        address::from_string(ipm.getMappedIP().get(), ec);
                    if (ec) continue;

                    address floatingIp;
                    if (ipm.getFloatingIP()) {
                        floatingIp =
                            address::from_string(ipm.getFloatingIP().get(), ec);
                        if (ec) continue;
                        if (floatingIp.is_v4() != mappedIp.is_v4()) continue;
                    }

                    uint32_t fepgVnid, frdId, fbdId, ffdId;
                    optional<URI> ffdURI, fbdURI, frdURI;
                    if (!getGroupForwardingInfo(ipm.getEgURI().get(),
                                                fepgVnid, frdURI, frdId,
                                                fbdURI, fbdId, ffdURI, ffdId))
                        continue;

                    uint32_t nextHop = OFPP_NONE;
                    if (ipm.getNextHopIf()) {
                        nextHop = switchManager.getPortMapper()
                            .FindPort(ipm.getNextHopIf().get());
                        if (nextHop == OFPP_NONE) continue;
                    }
                    uint8_t nextHopMac[6];
                    const uint8_t* nextHopMacp = NULL;
                    if (ipm.getNextHopMAC()) {
                        ipm.getNextHopMAC().get().toUIntArray(nextHopMac);
                        nextHopMacp = nextHopMac;
                    }

                    flowsIpm(*this, elSrc, elBridgeDst, elRouteDst,
                             elOutput, macAddr, ofPort,
                             epgVnid, rdId, bdId, fgrpId,
                             fepgVnid, frdId, fbdId, ffdId,
                             mappedIp, floatingIp, nextHop,
                             nextHopMacp);
                }

                const std::vector<std::string>& snatUuids
                    = endPoint.getSnatUuids();
                int count = 0;
                for (const auto& snatUuid: snatUuids) {
                    // Inform snat manager of our interest in this snat-ip
                    agent.getSnatManager().addEndpoint(snatUuid, uuid);
                    shared_ptr<const Snat> asWrapper =
                        agent.getSnatManager().getSnat(snatUuid);
                     if (asWrapper && asWrapper->isLocal() &&
                         asWrapper->getUUID() == snatUuid) {
                         const Snat& as = *asWrapper;
                         uint16_t zoneId = 0;
                         uint32_t snatPort = OFPP_NONE;

                         if (as.getZone())
                             zoneId = as.getZone().get();
                         if (zoneId == 0)
                             zoneId = ctZoneManager.getId(as.getUUID());
                         snatPort = switchManager.getPortMapper()
                             .FindPort(as.getInterfaceName());
                         if (snatPort != OFPP_NONE) {
                             flowsEndpointSNAT(agent.getSnatManager(),
                                               as, snatPort, rdId, zoneId,
                                               endPoint, uuid,
                                               elRouteDst, elSnat, ofPort,
                                               macAddr, count, elRevSnat);
                         }
                    }
                }
                LOG(DEBUG) << "Compiled " << count << " SNAT flows";
            }

            // When traffic returns from a service interface, we have a
            // special table to map the return traffic that bypasses
            // normal network semantics and policy.  The service map table
            // is reachable only for traffic originating from service
            // interfaces.
            if (hasMac) {
                std::vector<address> anycastReturnIps;
                for (const string& ipStr : endPoint.getAnycastReturnIPs()) {
                    address addr = address::from_string(ipStr, ec);
                    if (ec) {
                        LOG(WARNING) << "Invalid anycast return IP: "
                                     << ipStr << ": " << ec.message();
                    } else {
                        anycastReturnIps.push_back(addr);
                    }
                }
                if (anycastReturnIps.size() == 0) {
                    anycastReturnIps = ipAddresses;
                }

                for (const address& ipAddr : anycastReturnIps) {
                    {
                        // Deliver packets sent to service address
                        FlowBuilder serviceDest;
                        matchDestDom(serviceDest, 0, rdId);
                        serviceDest
                            .priority(50)
                            .ipDst(ipAddr)
                            .action()
                            .ethSrc(getRouterMacAddr()).ethDst(macAddr)
                            .decTtl()
                            .output(ofPort)
                            .parent().build(elServiceMap);
                    }
                    flowsProxyDiscovery(*this, elServiceMap,
                                        51, ipAddr, macAddr, 0, rdId, 0);
                }
            }
        }

        if (ofPort != OFPP_NONE) {
            // If a packet has a routing action applied, we'll allow it to
            // hairpin for ordinary default output action or reverse NAT
            // output
            vector<uint64_t> metadata {
                flow::meta::ROUTED,
                    flow::meta::ROUTED | flow::meta::out::REV_NAT
                    };
            for (auto m : metadata) {
                FlowBuilder()
                    .priority(2)
                    .inPort(ofPort)
                    .metadata(m, flow::meta::ROUTED | flow::meta::out::MASK)
                    .reg(7, ofPort)
                    .action()
                    .output(OFPP_IN_PORT)
                    .parent().build(elOutput);
            }
        }
    }

    switchManager.writeFlow(uuid, SEC_TABLE_ID, elPortSec);
    switchManager.writeFlow(uuid, SRC_TABLE_ID, elSrc);
    switchManager.writeFlow(uuid, BRIDGE_TABLE_ID, elBridgeDst);
    switchManager.writeFlow(uuid, ROUTE_TABLE_ID, elRouteDst);
    switchManager.writeFlow(uuid, SNAT_TABLE_ID, elSnat);
    switchManager.writeFlow(uuid, SNAT_REV_TABLE_ID, elRevSnat);
    switchManager.writeFlow(uuid, SERVICE_DST_TABLE_ID, elServiceMap);
    switchManager.writeFlow(uuid, OUT_TABLE_ID, elOutput);

    if (fgrpURI && ofPort != OFPP_NONE) {
        updateEndpointFloodGroup(fgrpURI.get(), endPoint, ofPort,
                                 fd);
    } else {
        removeEndpointFromFloodGroup(uuid);
    }
}

void IntFlowManager::in6AddrToLong (address sAddr, uint32_t *pAddr)
{
    boost::asio::ip::address_v6::bytes_type bytes = sAddr.to_v6().to_bytes();
    memcpy(pAddr, bytes.data(), sizeof(uint32_t) * 4);
    pAddr[0] = htonl(pAddr[0]);
    pAddr[1] = htonl(pAddr[1]);
    pAddr[2] = htonl(pAddr[2]);
    pAddr[3] = htonl(pAddr[3]);
}

template <class MO>
void IntFlowManager::updatePodSvcStatsAttr (const std::shared_ptr<MO> &obj,
                                            const attr_map &epAttr,
                                            const attr_map &svcAttr)
{
    {
        auto podItr = epAttr.find("vm-name");
        if (podItr != epAttr.end())
            obj.get()->setEp(podItr->second);
        else
            obj.get()->unsetEp();

        auto svcItr = svcAttr.find("name");
        if (svcItr != svcAttr.end())
            obj.get()->setSvc(svcItr->second);
        else
            obj.get()->unsetSvc();
    }

    {
        auto podItr = epAttr.find("namespace");
        if (podItr != epAttr.end())
            obj.get()->setEpNs(podItr->second);
        else
            obj.get()->unsetEpNs();

        auto svcItr = svcAttr.find("namespace");
        if (svcItr != svcAttr.end())
            obj.get()->setSvcNs(svcItr->second);
        else
            obj.get()->unsetSvcNs();
    }

    {
        auto svcItr = svcAttr.find("scope");
        if (svcItr != svcAttr.end())
            obj.get()->setSvcScope(svcItr->second);
        else
            obj.get()->unsetSvcScope();
    }
}

// Called from PolicyStatsManager to update stats
void IntFlowManager::
updateSvcStatsCounters (const uint64_t &cookie,
                        const uint64_t &newPktCount,
                        const uint64_t &newByteCount)
{
    const std::lock_guard<mutex> lock(svcStatMutex);

    // Additional safety for stats flows:
    // Nothing must be reported from ServiceStatsManager
    // if serviceStatsFlowDisabled=true, since the stats flows wont be created
    // in the first place.
    // If service stats collection is also disabled, we dont need this safety.
    if (serviceStatsFlowDisabled)
        return;

    boost::optional<std::string> str =
        idGen.getStringForId(ID_NMSPC_SVCSTATS, cookie);
    if (str == boost::none) {
        LOG(ERROR) << "Cookie: " << cookie
                   << " to svc metric translation does not exist";
        return;
    }

    // The idgen strings for epToSvc and svcToEp will have below format
    // eptosvc:ep-uuid:svc-uuid
    // svctoep:ep-uuid:svc-uuid

    // The idgen strings for anyToSvc and svcToAny will have below format
    // antosvc:svc-tgt:svc-uuid:nh-ip
    // svctoan:svc-tgt:svc-uuid:nh-ip

    const string& statType = str.get().substr(0,7);
    if ((statType == "eptosvc") || (statType == "svctoep")) {
        updatePodSvcStatsCounters(cookie,
                                  statType == "eptosvc",
                                  str.get(),
                                  newPktCount,
                                  newByteCount,
                                  attr_map(),
                                  attr_map());
    } else if ((statType == "antosvc") || (statType == "svctoan")) {
        updateSvcTgtStatsCounters(cookie,
                                  statType == "antosvc",
                                  str.get(),
                                  newPktCount,
                                  newByteCount,
                                  attr_map(),
                                  attr_map());
    }
}

void IntFlowManager::
updateSvcStatsCounters (const bool &isIngress,
                        const string& uuid,
                        const uint64_t &newPktCount,
                        const uint64_t &newByteCount,
                        const bool &add)
{
    Mutator mutator(agent.getFramework(), "policyelement");
    optional<shared_ptr<SvcStatUniverse> > su =
        SvcStatUniverse::resolve(agent.getFramework());
    if (su) {
        auto opSvc = SvcCounter::resolve(agent.getFramework(), uuid);
        if (opSvc) {
            uint64_t updPktCount = 0, updByteCount = 0;
            if (isIngress) {
                auto oldPktCount = opSvc.get()->getRxpackets(0);
                auto oldByteCount = opSvc.get()->getRxbytes(0);
                if (add) {
                    updPktCount = oldPktCount + newPktCount;
                    updByteCount = oldByteCount + newByteCount;
                } else {
                    updPktCount = oldPktCount - newPktCount;
                    updByteCount = oldByteCount - newByteCount;
                }
                opSvc.get()->setRxpackets(updPktCount)
                            .setRxbytes(updByteCount);
            } else {
                auto oldPktCount = opSvc.get()->getTxpackets(0);
                auto oldByteCount = opSvc.get()->getTxbytes(0);
                if (add) {
                    updPktCount = oldPktCount + newPktCount;
                    updByteCount = oldByteCount + newByteCount;
                } else {
                    updPktCount = oldPktCount - newPktCount;
                    updByteCount = oldByteCount - newByteCount;
                }
                opSvc.get()->setTxpackets(updPktCount)
                            .setTxbytes(updByteCount);
            }
            mutator.commit();
#ifdef HAVE_PROMETHEUS_SUPPORT
            // MoDB takes some time to get updated. Updating prom metrics
            // with actual value if possible that getting the values from modb.
            // This is to keep prom and modb in sync. Not doing this will update
            // prom to prior modb value during next stats update.
            if (isIngress) {
                prometheusManager.addNUpdateSvcCounter(uuid,
                                                   updByteCount,
                                                   updPktCount,
                                                   opSvc.get()->getTxbytes(0),
                                                   opSvc.get()->getTxpackets(0),
                                                   attr_map());
            } else {
                prometheusManager.addNUpdateSvcCounter(uuid,
                                                   opSvc.get()->getRxbytes(0),
                                                   opSvc.get()->getRxpackets(0),
                                                   updByteCount,
                                                   updPktCount,
                                                   attr_map());
            }
#endif
        }
    }
}

void IntFlowManager::
updateSvcTgtStatsCounters (const uint64_t &cookie,
                           const bool &isAnyToSvc,
                           const string& idStr,
                           const uint64_t &newPktCount,
                           const uint64_t &newByteCount,
                           const attr_map &svcAttr,
                           const attr_map &epAttr)
{
    Mutator mutator(agent.getFramework(), "policyelement");
    size_t pos1 = idStr.find(":");
    size_t pos2 = idStr.find(":", pos1+1);
    size_t pos3 = idStr.find(":", pos2+1);
    const string& svcUuid = idStr.substr(pos2+1, pos3-pos2-1);
    const string& nhipStr = idStr.substr(pos3+1);

    auto opSvcTgt = SvcTargetCounter::resolve(agent.getFramework(),
                                              svcUuid, nhipStr);
    if (opSvcTgt) {
        uint64_t updPktCount = 0, updByteCount = 0;
        if (isAnyToSvc) {
            auto oldPktCount = opSvcTgt.get()->getRxpackets(0);
            auto oldByteCount = opSvcTgt.get()->getRxbytes(0);
            updPktCount = oldPktCount + newPktCount;
            updByteCount = oldByteCount + newByteCount;
            opSvcTgt.get()->setRxpackets(updPktCount)
                           .setRxbytes(updByteCount);
        } else {
            auto oldPktCount = opSvcTgt.get()->getTxpackets(0);
            auto oldByteCount = opSvcTgt.get()->getTxbytes(0);
            updPktCount = oldPktCount + newPktCount;
            updByteCount = oldByteCount + newByteCount;
            opSvcTgt.get()->setTxpackets(updPktCount)
                           .setTxbytes(updByteCount);
        }

        // following will take care of updates to these attributes
        if (!newPktCount) {
            auto podItr = epAttr.find("vm-name");
            if (podItr != epAttr.end())
                opSvcTgt.get()->setName(podItr->second);
            else
                opSvcTgt.get()->unsetName();

            auto nsItr = epAttr.find("namespace");
            if (nsItr != epAttr.end())
                opSvcTgt.get()->setNamespace(nsItr->second);
            else
                opSvcTgt.get()->unsetNamespace();

        }
        mutator.commit();
#ifdef HAVE_PROMETHEUS_SUPPORT
        // MoDB takes some time to get updated. Updating prom metrics
        // with actual value if possible that getting the values from modb.
        // This is to keep prom and modb in sync. Not doing this will update
        // prom to prior modb value during next stats update.
        if (isAnyToSvc) {
            prometheusManager.addNUpdateSvcTargetCounter(svcUuid,
                                                     nhipStr,
                                                     updByteCount,
                                                     updPktCount,
                                                     opSvcTgt.get()->getTxbytes(0),
                                                     opSvcTgt.get()->getTxpackets(0),
                                                     svcAttr,
                                                     epAttr,
                                                     epAttr.size()!=0?true:false);
        } else {
            prometheusManager.addNUpdateSvcTargetCounter(svcUuid,
                                                     nhipStr,
                                                     opSvcTgt.get()->getRxbytes(0),
                                                     opSvcTgt.get()->getRxpackets(0),
                                                     updByteCount,
                                                     updPktCount,
                                                     svcAttr,
                                                     epAttr,
                                                     epAttr.size()!=0?true:false);
        }
#endif
    }
    updateSvcStatsCounters(isAnyToSvc, svcUuid, newPktCount, newByteCount, true);
}

// Private function to update stats and attributes
void IntFlowManager::
updatePodSvcStatsCounters (const uint64_t &cookie,
                           const bool& isEpToSvc,
                           const string& idStr,
                           const uint64_t &newPktCount,
                           const uint64_t &newByteCount,
                           const attr_map &epAttr,
                           const attr_map &svcAttr)
{
    Mutator mutator(agent.getFramework(), "policyelement");
    optional<shared_ptr<SvcStatUniverse> > su =
        SvcStatUniverse::resolve(agent.getFramework());
    if (su) {
        if (isEpToSvc) {
            auto pEpToSvc = su.get()->resolveGbpeEpToSvcCounter(
                                        agent.getUuid(), idStr);
            // Create mo and update attributes only during cfg update
            if (!newPktCount) {
                if (!pEpToSvc) {
                    pEpToSvc = su.get()->addGbpeEpToSvcCounter(
                                       agent.getUuid(), idStr);
                }
                updatePodSvcStatsAttr<EpToSvcCounter>(pEpToSvc.get(),
                                                      epAttr,
                                                      svcAttr);
            }

            if (pEpToSvc) {
                auto oldPktCount = pEpToSvc.get()->getPackets(0);
                auto oldByteCount = pEpToSvc.get()->getBytes(0);
                pEpToSvc.get()->setPackets(oldPktCount + newPktCount)
                               .setBytes(oldByteCount + newByteCount);
            }
        } else {
            auto pSvcToEp = su.get()->resolveGbpeSvcToEpCounter(
                                        agent.getUuid(), idStr);
            // Create mo and update attributes only during cfg update
            if (!newPktCount) {
                if (!pSvcToEp) {
                    pSvcToEp = su.get()->addGbpeSvcToEpCounter(
                                        agent.getUuid(), idStr);
                }
                updatePodSvcStatsAttr<SvcToEpCounter>(pSvcToEp.get(),
                                                      epAttr,
                                                      svcAttr);
            }

            if (pSvcToEp) {
                auto oldPktCount = pSvcToEp.get()->getPackets(0);
                auto oldByteCount = pSvcToEp.get()->getBytes(0);
                pSvcToEp.get()->setPackets(oldPktCount + newPktCount)
                               .setBytes(oldByteCount + newByteCount);
            }
        }
    }
    mutator.commit();
#ifdef HAVE_PROMETHEUS_SUPPORT
    prometheusManager.addNUpdatePodSvcCounter(isEpToSvc,
                                              idStr,
                                              epAttr,
                                              svcAttr);
#endif
}

// Clear pod svc objects
void IntFlowManager::clearPodSvcStatsCounters (const std::string& uuid)
{
    Mutator mutator(agent.getFramework(), "policyelement");
    bool isEpToSvc = !strcmp(uuid.substr(0,7).c_str(),
                             "eptosvc")?true:false;
    if (isEpToSvc) {
        EpToSvcCounter::remove(agent.getFramework(),
                               agent.getUuid(), uuid);
    } else {
        SvcToEpCounter::remove(agent.getFramework(),
                               agent.getUuid(), uuid);
    }
    mutator.commit();
#ifdef HAVE_PROMETHEUS_SUPPORT
    prometheusManager.removePodSvcCounter(isEpToSvc, uuid);
#endif
}

// Reset svc-tgt counter stats, deletion of the object will be handled in
// ServiceManager
void IntFlowManager::clearSvcTgtStatsCounters (const std::string& svcUuid,
                                               const std::string& nhipStr,
                                               const attr_map& svcAttr)
{
    using modelgbp::observer::SvcStatUniverse;
    Mutator mutator(agent.getFramework(), "policyelement");

    auto opSvcTgt = SvcTargetCounter::resolve(agent.getFramework(),
                                              svcUuid, nhipStr);
    if (opSvcTgt) {
        auto oldRxPktCount = opSvcTgt.get()->getRxpackets(0);
        auto oldRxByteCount = opSvcTgt.get()->getRxbytes(0);
        auto oldTxPktCount = opSvcTgt.get()->getTxpackets(0);
        auto oldTxByteCount = opSvcTgt.get()->getTxbytes(0);
        opSvcTgt.get()->unsetName();
        opSvcTgt.get()->unsetNamespace();
        opSvcTgt.get()->unsetRxbytes();
        opSvcTgt.get()->unsetRxpackets();
        opSvcTgt.get()->unsetTxbytes();
        opSvcTgt.get()->unsetTxpackets();
        updateSvcStatsCounters(true, svcUuid, oldRxPktCount, oldRxByteCount, false);
        updateSvcStatsCounters(false, svcUuid, oldTxPktCount, oldTxByteCount, false);
#ifdef HAVE_PROMETHEUS_SUPPORT
        // If the flows dont exist, reset the counts back to 0
        // also remove the extra pod specific label annotations
        prometheusManager.addNUpdateSvcTargetCounter(svcUuid,
                                                     nhipStr,
                                                     0, 0, 0, 0,
                                                     svcAttr,
                                                     attr_map(),
                                                     true);
#endif
    }
    mutator.commit();
}

// Reset svc counter stats, deletion of the object will be handled in
// ServiceManager
void IntFlowManager::clearSvcStatsCounters (const std::string& uuid,
                                            const attr_map& svcAttr)
{
    using modelgbp::observer::SvcStatUniverse;
    Mutator mutator(agent.getFramework(), "policyelement");
    optional<shared_ptr<SvcStatUniverse> > ssu =
                SvcStatUniverse::resolve(agent.getFramework());
    if (!ssu)
        return;
    optional<shared_ptr<SvcCounter> > opSvc =
                    ssu.get()->resolveGbpeSvcCounter(uuid);
    if (opSvc) {
        std::vector<shared_ptr<SvcTargetCounter> > out;
        opSvc.get()->resolveGbpeSvcTargetCounter(out);
        for (auto& pSvcTarget : out) {
            pSvcTarget->unsetName();
            pSvcTarget->unsetNamespace();
            pSvcTarget->unsetRxbytes();
            pSvcTarget->unsetRxpackets();
            pSvcTarget->unsetTxbytes();
            pSvcTarget->unsetTxpackets();
#ifdef HAVE_PROMETHEUS_SUPPORT
            // If the flows dont exist, reset the counts back to 0
            // also remove the extra pod specific label annotations
            auto nhip = pSvcTarget->getIp();
            if (nhip)
                prometheusManager.addNUpdateSvcTargetCounter(uuid,
                                                             nhip.get(),
                                                             0, 0, 0, 0,
                                                             svcAttr,
                                                             attr_map(),
                                                             true);
#endif
        }
        opSvc.get()->unsetRxbytes();
        opSvc.get()->unsetRxpackets();
        opSvc.get()->unsetTxbytes();
        opSvc.get()->unsetTxpackets();
#ifdef HAVE_PROMETHEUS_SUPPORT
        // If svc becomes external or anycast or if all the NH's dont
        // exist or create flows, then reset the counters back to 0
        prometheusManager.addNUpdateSvcCounter(uuid,
                                               0, 0, 0, 0,
                                               attr_map());
#endif
    }
    mutator.commit();
}

/**
 * Add/del stats flows:
 * Cluster/E-W:
 *  - "ep to svc" and "svc to ep"
 *  - "svc to any" and "any to svc": each svc will be expanded to svc-target
 */
void IntFlowManager::updateSvcStatsFlows (const string& uuid,
                                          const bool& is_svc,
                                          const bool& is_add)
{
    const std::lock_guard<mutex> lock(svcStatMutex);

    if (serviceStatsFlowDisabled)
        return;

    updatePodSvcStatsFlows(uuid, is_svc, is_add);
    updateSvcTgtStatsFlows(uuid, is_svc, is_add);
}

void IntFlowManager::updateSvcTgtStatsFlows (const string &uuid,
                                             const bool &is_svc,
                                             const bool &is_add)
{

    LOG(DEBUG) << "##### Updating *<-->svc-tgt flows:"
               << " uuid: " << uuid
               << " is_svc: " << is_svc
               << " is_add: " << is_add << "#######";

    /* A service could have multiple ServiceMappings and that could
     * have multiple next hop pod IPs. Check if these next hops are
     * local to the node, and then create flows for these.
     * Each of these "*<-->svc-tgt" are tracked together under
     * their respective "svc" uuids.
     * i.e. All the svc-tgt flows are clubbed together with "svc-tgt:svc-uuid".
     * In case there is any delta in flows due to EP delete or EP being external
     * or next-hop delete or update, the diff of flows will take effect
     * in TableState.apply() */
    unordered_map<string, FlowEntryList> uuid_felist_map;

    // Expr to del stats flow between "any to svc" and "svc to any"
    auto svcTgtFlowRemExpr =
        [this] (const string &flow_uuid,
                const string &svc_uuid) -> void {
        switchManager.clearFlows(flow_uuid, STATS_TABLE_ID);

        // Idgen would have restored the ids during agent restart. If flows for this
        // service needs to be removed, then free up the restored IDs as well.
        ServiceManager& srvMgr = agent.getServiceManager();
        shared_ptr<const Service> asWrapper = srvMgr.getService(svc_uuid);
        if (asWrapper) {
            const Service& as = *asWrapper;
            for (auto const& sm : as.getServiceMappings()) {
                for (const string& nhip : sm.getNextHopIPs()) {
                    idGen.erase(ID_NMSPC_SVCSTATS, "antosvc:"+flow_uuid+":"+nhip);
                    idGen.erase(ID_NMSPC_SVCSTATS, "svctoan:"+flow_uuid+":"+nhip);
                    if (svc_nh_map.find(svc_uuid) != svc_nh_map.end())
                        svc_nh_map[svc_uuid].erase(nhip);
                }
            }
            clearSvcStatsCounters(svc_uuid, as.getAttributes());
        } else {
            // Note: the only time asWrapper will be null is when service is
            // removed. In such a case, the observer mo and prom metrics
            // would have been deleted from ServiceManager already. The below
            // call will be a no-op.
            clearSvcStatsCounters(svc_uuid, attr_map());
        }

        // Above should have freed up all the ids. But during svc delete, asWrapper will be
        // null. Use svc_nh_map to free up in that case.
        if (svc_nh_map.find(svc_uuid) != svc_nh_map.end()) {
            for (const string& nhip : svc_nh_map[svc_uuid]) {
                idGen.erase(ID_NMSPC_SVCSTATS, "antosvc:"+flow_uuid+":"+nhip);
                idGen.erase(ID_NMSPC_SVCSTATS, "svctoan:"+flow_uuid+":"+nhip);
            }
        }
        svc_nh_map[svc_uuid].clear();
        svc_nh_map.erase(svc_uuid);
    };

    // build this set to detect if any svc-tgt state needs to be removed
    // after an update of svc or ep/nh
    unordered_set<string> nhips;

    // Expr to add stats flow between "any to svc" and "svc to any"
    auto svcTgtFlowAddExpr =
        [this, &uuid_felist_map, &nhips](const string &flow_uuid,
                                         const string &svc_uuid,
                                         const string &nhipStr,
                                         const Service::ServiceMapping &sm,
                                         const attr_map &svcAttr,
                                         const attr_map &epAttr) -> void {

        boost::system::error_code ec;
        address nhAddr = address::from_string(nhipStr, ec);
        if (ec) {
            LOG(WARNING) << "Invalid nexthop IP: "
                         << nhipStr << ": " << ec.message();
            return;
        }

        // check if service IP is valid for safety
        if (!sm.getServiceIP())
            return;
        address::from_string(sm.getServiceIP().get(), ec);
        if (ec) {
            LOG(WARNING) << "Invalid service IP: "
                         << sm.getServiceIP().get()
                         << ": " << ec.message();
            return;
        }
        LOG(DEBUG) << "Adding any<-->svc flow for"
                   << " NH IP: " << nhipStr
                   << " SVC-SM IP: " << sm.getServiceIP().get();

        uint8_t proto = 0;
        if (sm.getServiceProto()) {
            const string& protoStr = sm.getServiceProto().get();
            if ("udp" == protoStr)
                proto = 17;
            else if ("tcp" == protoStr)
                proto = 6;
            else {
                LOG(DEBUG) << "unhandled proto: " << protoStr;
                return;
            }
        }

        const string& ingStr = "antosvc:"+flow_uuid+":"+nhipStr;
        const string& egrStr = "svctoan:"+flow_uuid+":"+nhipStr;
        uint64_t cookieIdIg = (uint64_t)idGen.getId(ID_NMSPC_SVCSTATS, ingStr);
        uint64_t cookieIdEg = (uint64_t)idGen.getId(ID_NMSPC_SVCSTATS, egrStr);
        svc_nh_map[svc_uuid].insert(nhipStr);
        nhips.insert(nhipStr);

        LOG(DEBUG) << "Creating any<-->svc counters for"
                   << " flow_uuid: " << flow_uuid
                   << " cookieIg: " << cookieIdIg
                   << " cookieEg: " << cookieIdEg;

        // updates to take care of pod name and namespace change
        updateSvcTgtStatsCounters(cookieIdIg, true, ingStr, 0, 0, svcAttr, epAttr);
        updateSvcTgtStatsCounters(cookieIdEg, false, egrStr, 0, 0, svcAttr, epAttr);

        FlowBuilder anyToSvc; // to service stats
        FlowBuilder svcToAny; // from service stats

        matchServiceProto(anyToSvc, proto, sm, true);
        matchActionServiceProto(svcToAny, proto, sm, false, false);

        if (nhAddr.is_v4()) {
            anyToSvc.priority(97).ethType(eth::type::IP)
                    .ipDst(nhAddr)
                    .flags(OFPUTIL_FF_SEND_FLOW_REM)
                    .cookie(ovs_htonll(cookieIdIg))
                    .action().go(OUT_TABLE_ID);
            svcToAny.priority(97).ethType(eth::type::IP)
                    .ipSrc(nhAddr)
                    .flags(OFPUTIL_FF_SEND_FLOW_REM)
                    .cookie(ovs_htonll(cookieIdEg))
                    .action().go(OUT_TABLE_ID);
        } else {
            anyToSvc.priority(97).ethType(eth::type::IPV6)
                    .ipDst(nhAddr)
                    .flags(OFPUTIL_FF_SEND_FLOW_REM)
                    .cookie(ovs_htonll(cookieIdIg))
                    .action().go(OUT_TABLE_ID);
            svcToAny.priority(97).ethType(eth::type::IPV6)
                    .ipSrc(nhAddr)
                    .flags(OFPUTIL_FF_SEND_FLOW_REM)
                    .cookie(ovs_htonll(cookieIdEg))
                    .action().go(OUT_TABLE_ID);
        }
        anyToSvc.build(uuid_felist_map[flow_uuid]);
        svcToAny.build(uuid_felist_map[flow_uuid]);
    };

    if (is_svc) {
        if (!is_add) {
            svcTgtFlowRemExpr("svc-tgt:"+uuid, uuid);
            return;
        }

        ServiceManager& srvMgr = agent.getServiceManager();
        shared_ptr<const Service> asWrapper = srvMgr.getService(uuid);

        if (!asWrapper || !asWrapper->getDomainURI()) {
            LOG(DEBUG) << "unable to get service from uuid";
            return;
        }

        const Service& as = *asWrapper;
        LOG(DEBUG) << "####### *<-->svc-tgt Service ########";
        LOG(DEBUG) << *asWrapper;

        if ((as.getServiceMode() != Service::LOADBALANCER)
                                  || as.isExternal()) {
            LOG(DEBUG) << "*<-->svc-tgt not handled for non-LB or ext services";
            // clear obs and prom metrics during update;
            // below will be no-op during create
            svcTgtFlowRemExpr("svc-tgt:"+uuid, uuid);
            return;
        }

        for (auto const& sm : as.getServiceMappings()) {
            for (const string& nhipstr : sm.getNextHopIPs()) {
                const ip_ep_map_t& ip_ep_map = agent.getEndpointManager().getIPLocalEpMap();
                const auto& itr = ip_ep_map.find(nhipstr);
                if (itr != ip_ep_map.end()) {
                    svcTgtFlowAddExpr("svc-tgt:"+uuid,
                                      uuid,
                                      nhipstr, sm,
                                      as.getAttributes(),
                                      itr->second->getAttributes());
                }
            }
        }

        // flush svc-tgt counters and idgen cookies of NH flows that got
        // removed due to config updates of svc or ep/nh
        if (!uuid_felist_map.size()) {
            LOG(DEBUG) << "####*<-->svc-tgt no flows created for svc_uuid: " << uuid;
            // clear obs and prom metrics during update;
            // below will be no-op during create
            svcTgtFlowRemExpr("svc-tgt:"+uuid, uuid);
        } else if (svc_nh_map.find(uuid) != svc_nh_map.end()) {
            auto nh_itr = svc_nh_map[uuid].begin();
            while (nh_itr != svc_nh_map[uuid].end()) {
                if (nhips.find(*nh_itr) == nhips.end()) {
                    LOG(DEBUG) << "#### *<-->svc-tgt: deleting"
                               << " svc_uuid: " << uuid
                               << " nh_ip: " << *nh_itr;
                    idGen.erase(ID_NMSPC_SVCSTATS, "antosvc:svc-tgt:"+uuid+":"+*nh_itr);
                    idGen.erase(ID_NMSPC_SVCSTATS, "svctoan:svc-tgt:"+uuid+":"+*nh_itr);
                    clearSvcTgtStatsCounters(uuid, *nh_itr, as.getAttributes());
                    nh_itr = svc_nh_map[uuid].erase(nh_itr);
                } else {
                    nh_itr++;
                }
            }
            nhips.clear();
        }
    } else {
        unordered_set<string> svcUuids;
        ServiceManager& svcMgr = agent.getServiceManager();
        svcMgr.getServiceUUIDs(svcUuids);

        // check if this ep is servicemapping.nhIP. If so, the flows
        // for this svc-tgt will get updated
        // If the EP became external, then stats flows need to be removed
        // If the EP became local, then stats flows need to be added
        // If new IP is added, then stats flows will be added
        // If an IP is deleted, then stats flows will be deleted
        for (const string& svcUuid : svcUuids) {
            updateSvcTgtStatsFlows(svcUuid, true, true);
            // If EP IP is added, and happens to be NH of this service, then cookie needs to be updated
            // If EP IP is deleted, and happens to be NH of this service, then cookie needs to be removed
            // If EP IP is modified:
            //  - if it became NH of this service, then cookie needs to be updated
            //  - if it moved away from being NH of this service, then cookie needs to be removed
            programServiceSnatDnatFlows(svcUuid);
        }
    }

    for (auto &p : uuid_felist_map)
        switchManager.writeFlow(p.first, STATS_TABLE_ID, p.second);
}

void IntFlowManager::updatePodSvcStatsFlows (const string &uuid,
                                             const bool &is_svc,
                                             const bool &is_add)
{
    LOG(DEBUG) << "##### Updating pod<-->svc flows:"
               << " uuid: " << uuid
               << " is_svc: " << is_svc
               << " is_add: " << is_add << "#######";

    /* A service could have multiple ServiceMappings and an EP
     * could have multiple IPs. The flows are created for each of the
     * IP pairs between pod <--> svc.
     * i.e multiple of these "epIP<-->smIP" are tracked together under
     * their respective "pod<-->svc" uuids */
    unordered_map<string, FlowEntryList> uuid_felist_map;

    // Expr to del stats flow between "ep to svc" and "svc to ep"
    auto podSvcFlowRemExpr =
        [this] (const string &uuid) -> void {
        switchManager.clearFlows(uuid, STATS_TABLE_ID);
        // Qualifying the names given to idGen with eptosvc/svctoep
        // so that stat's infra's genIdList_ is unique per direction
        clearPodSvcStatsCounters("eptosvc:"+uuid);
        clearPodSvcStatsCounters("svctoep:"+uuid);
        idGen.erase(ID_NMSPC_SVCSTATS, "eptosvc:"+uuid);
        idGen.erase(ID_NMSPC_SVCSTATS, "svctoep:"+uuid);
        podSvcUuidCkMap.erase(uuid);
    };

    // Expr to add stats flow between "ep to svc" and "svc to ep"
    auto podSvcFlowAddExpr =
        [this, &uuid_felist_map](const string &uuid,
                                 const string &epipStr,
                                 const Service::ServiceMapping &sm,
                                 const attr_map &epAttr,
                                 const attr_map &svcAttr) -> void {

        boost::system::error_code ec;

        address epAddr = address::from_string(epipStr, ec);
        if (ec) {
            LOG(WARNING) << "Invalid endpoint IP: "
                         << epipStr << ": " << ec.message();
            return;
        }

        if (!sm.getServiceIP())
            return;
        address svcAddr =
            address::from_string(sm.getServiceIP().get(), ec);
        if (ec) {
            LOG(WARNING) << "Invalid service IP: "
                         << sm.getServiceIP().get()
                         << ": " << ec.message();
            return;
        }
        LOG(DEBUG) << "Adding pod<-->svc flow between"
                   << " EP IP: " << epipStr
                   << " SVC-SM IP: " << sm.getServiceIP().get();

        // ensure flows are either v4 or v6 - no mix-n-match
        if (svcAddr.is_v4() != epAddr.is_v4()) {
            LOG(DEBUG) << "Not adding flow - ip types are different";
            return;
        }

        uint8_t proto = 0;
        if (sm.getServiceProto()) {
            const string& protoStr = sm.getServiceProto().get();
            if ("udp" == protoStr)
                proto = 17;
            else if ("tcp" == protoStr)
                proto = 6;
            else {
                LOG(DEBUG) << "unhandled proto: " << protoStr;
                return;
            }
        }

        const string& ingStr = "eptosvc:"+uuid;
        const string& egrStr = "svctoep:"+uuid;
        auto itr = podSvcUuidCkMap.find(uuid);
        if (itr == podSvcUuidCkMap.end()) {
            // Qualifying the names given to idGen with eptosvc/svctoep
            // so that stat's infra's genIdList_ is unique per direction
            // Note: we are not using genIdList_ currently. But keeping
            // this extra qualifier anyway since both classes share same
            // ns
            uint64_t cookieIdIg = (uint64_t)idGen.getId(ID_NMSPC_SVCSTATS, ingStr);
            uint64_t cookieIdEg = (uint64_t)idGen.getId(ID_NMSPC_SVCSTATS, egrStr);

            // Create the objects and cookies once for every POD,SVC combination
            LOG(DEBUG) << "Creating pod<-->svc counters for"
                       << " uuid: " << uuid
                       << " cookieIg: " << cookieIdIg
                       << " cookieEg: " << cookieIdEg;
            podSvcUuidCkMap[uuid] = make_pair(cookieIdIg, cookieIdEg);
        }

        // Note: the objects are created once. But we still call below counter
        // updates to take care of ep/svc attr changes
        updatePodSvcStatsCounters(podSvcUuidCkMap[uuid].first, true, ingStr,
                                  0, 0, epAttr, svcAttr);
        updatePodSvcStatsCounters(podSvcUuidCkMap[uuid].second, false, egrStr,
                                  0, 0, epAttr, svcAttr);

        FlowBuilder epToSvc; // to service stats
        FlowBuilder svcToEp; // from service stats

        matchServiceProto(epToSvc, proto, sm, true);
        matchServiceProto(svcToEp, proto, sm, false);

        if (svcAddr.is_v4()) {
            epToSvc.priority(100).ethType(eth::type::IP)
                   .ipSrc(epAddr)
                   .reg(8, svcAddr.to_v4().to_ulong())
                   .flags(OFPUTIL_FF_SEND_FLOW_REM)
                   .cookie(ovs_htonll(podSvcUuidCkMap[uuid].first))
                   .action().go(OUT_TABLE_ID);
            svcToEp.priority(100).ethType(eth::type::IP)
                   .ipSrc(svcAddr).ipDst(epAddr)
                   .flags(OFPUTIL_FF_SEND_FLOW_REM)
                   .cookie(ovs_htonll(podSvcUuidCkMap[uuid].second))
                   .action().go(OUT_TABLE_ID);
        } else {
            uint32_t pAddr[4];
            in6AddrToLong(svcAddr, &pAddr[0]);
            // E.g. An addr = fe80::a9:fe:a9:fe will get stuffed in regs as below
            // reg8=0xfe800000,reg9=0,reg10=0xa900fe,reg11=0xa900fe
            epToSvc.priority(100).ethType(eth::type::IPV6)
                   .ipSrc(epAddr)
                   .reg(8, pAddr[0]).reg(9, pAddr[1])
                   .reg(10, pAddr[2]).reg(11, pAddr[3])
                   .flags(OFPUTIL_FF_SEND_FLOW_REM)
                   .cookie(ovs_htonll(podSvcUuidCkMap[uuid].first))
                   .action().go(OUT_TABLE_ID);
            svcToEp.priority(100).ethType(eth::type::IPV6)
                   .ipSrc(svcAddr).ipDst(epAddr)
                   .flags(OFPUTIL_FF_SEND_FLOW_REM)
                   .cookie(ovs_htonll(podSvcUuidCkMap[uuid].second))
                   .action().go(OUT_TABLE_ID);
        }
        epToSvc.build(uuid_felist_map[uuid]);
        svcToEp.build(uuid_felist_map[uuid]);
    };

    if (is_svc) {
        unordered_set<string> epUuids;
        EndpointManager& epMgr = agent.getEndpointManager();
        epMgr.getEndpointUUIDs(epUuids);

        if (!is_add) {
            for (const string& epUuid : epUuids)
                podSvcFlowRemExpr(epUuid+":"+uuid);
            return;
        }

        ServiceManager& srvMgr = agent.getServiceManager();
        shared_ptr<const Service> asWrapper = srvMgr.getService(uuid);

        if (!asWrapper || !asWrapper->getDomainURI()) {
            LOG(DEBUG) << "unable to get service from uuid";
            return;
        }

        const Service& as = *asWrapper;
        LOG(DEBUG) << "####### pod<-->svc Service ########";
        LOG(DEBUG) << *asWrapper;

        // build this set to detect if a pod<-->svc flow got created or not.
        // For e.g. if a svc-next hop becomes same as an IP in EP, then flow wont be created.
        // pre-existing flows should be deleted though and the mos should be removed.
        unordered_set<string> epsvc_uuids;
        for (auto const& sm : as.getServiceMappings()) {

            for (const string& epUuid : epUuids) {
                shared_ptr<const Endpoint> epWrapper
                     = epMgr.getEndpoint(epUuid);
                if (!epWrapper)
                    continue;

                LOG(DEBUG) << "####### pod<-->svc Endpoint ########";
                LOG(DEBUG) << *epWrapper;

                if ((as.getServiceMode() != Service::LOADBALANCER)
                                          || as.isExternal()) {
                    LOG(DEBUG) << "podsvc not handled for non-LB or ext services";
                    // clear obs and prom metrics during update;
                    // below will be no-op during create
                    podSvcFlowRemExpr(epUuid+":"+uuid);
                    continue;
                }

                const Endpoint& endPoint = *epWrapper.get();
                if (endPoint.isExternal()) {
                    LOG(DEBUG) << "pod<-->svc flows not handled for external endpoints";
                    continue;
                }

                for (const string& epipStr : endPoint.getIPs()) {
                    // Dont create EPIP <--> SVCIP flows if EPIP is one of the
                    // next hops of this service.
                    const auto& nhips = sm.getNextHopIPs();
                    if (nhips.find(epipStr) == nhips.end()) {
                        podSvcFlowAddExpr(epUuid+":"+uuid,
                                          epipStr, sm,
                                          endPoint.getAttributes(),
                                          as.getAttributes());
                    } else {
                        epsvc_uuids.insert(epUuid+":"+uuid);
                    }
                }
            }
        }

        for (const auto& epsvc_uuid: epsvc_uuids) {
            if (uuid_felist_map.find(epsvc_uuid) == uuid_felist_map.end()) {
                LOG(DEBUG) << "podsvc: no flows created for epsvc_uuid: " << epsvc_uuid;
                podSvcFlowRemExpr(epsvc_uuid);
            }
        }
        epsvc_uuids.clear();
    } else {
        unordered_set<string> svcUuids;
        ServiceManager& svcMgr = agent.getServiceManager();
        svcMgr.getServiceUUIDs(svcUuids);

        if (!is_add) {
            for (const string& svcUuid : svcUuids)
                podSvcFlowRemExpr(uuid+":"+svcUuid);
            return;
        }

        EndpointManager& epMgr = agent.getEndpointManager();
        shared_ptr<const Endpoint> epWrapper = epMgr.getEndpoint(uuid);

        // following can be valid only during is_add=false, but adding for safety
        if (!epWrapper) {
            LOG(DEBUG) << "unable to get ep from uuid";
            return;
        }

        LOG(DEBUG) << "####### pod<-->svc Endpoint ########";
        LOG(DEBUG) << *epWrapper;

        const Endpoint& endPoint = *epWrapper.get();
        if (endPoint.isExternal()) {
            LOG(DEBUG) << "pod<-->svc flows not handled for external endpoints";
            return;
        }

        // build this set to detect if a pod<-->svc flow got created or not.
        // For e.g. if an EP becomes next hop of a service, then flow wont be created.
        // pre-existing flows should be deleted though and the mos should be removed.
        // If there are more IPs of this EP which aren't next-hops of the service, then
        // flows will get created between this EP and Svc, and unwanted flows will get
        // cleaned up.
        unordered_set<string> epsvc_uuids;
        for (const string& epipStr : endPoint.getIPs()) {

            for (const string& svcUuid : svcUuids) {
                shared_ptr<const Service> asWrapper
                     = svcMgr.getService(svcUuid);
                if (!asWrapper)
                    continue;

                LOG(DEBUG) << "####### pod<-->svc Service ########";
                LOG(DEBUG) << *asWrapper;

                const Service& as = *asWrapper.get();
                if ((as.getServiceMode() != Service::LOADBALANCER)
                        || as.isExternal()) {
                    LOG(DEBUG) << "podsvc not handled for non-LB or ext services";
                    // clear obs and prom metrics during update;
                    // below will be no-op during create
                    podSvcFlowRemExpr(uuid+":"+svcUuid);
                    continue;
                }

                for (auto const& sm : as.getServiceMappings()) {
                    // Dont create EPIP <--> SVCIP flows if EPIP is one of the
                    // next hops of this service.
                    const auto& nhips = sm.getNextHopIPs();
                    if (nhips.find(epipStr) == nhips.end()) {
                        podSvcFlowAddExpr(uuid+":"+svcUuid,
                                          epipStr, sm,
                                          endPoint.getAttributes(),
                                          as.getAttributes());
                    } else {
                        epsvc_uuids.insert(uuid+":"+svcUuid);
                    }
                }
            }
        }

        for (const auto& epsvc_uuid: epsvc_uuids) {
            if (uuid_felist_map.find(epsvc_uuid) == uuid_felist_map.end()) {
                LOG(DEBUG) << "podsvc: no flows created for epsvc_uuid: " << epsvc_uuid;
                podSvcFlowRemExpr(epsvc_uuid);
            }
        }
        epsvc_uuids.clear();
    }

    for (auto &p : uuid_felist_map)
        switchManager.writeFlow(p.first, STATS_TABLE_ID, p.second);
}

void IntFlowManager::programServiceSnatDnatFlows (const string& uuid)
{
    LOG(DEBUG) << "Updating snat dnat flows for service " << uuid;

    ServiceManager& srvMgr = agent.getServiceManager();
    shared_ptr<const Service> asWrapper = srvMgr.getService(uuid);

    if (!asWrapper || !asWrapper->getDomainURI()) {
        return;
    }
    const Service& as = *asWrapper;
    FlowEntryList serviceRevFlows;
    FlowEntryList serviceNextHopFlows;

    boost::system::error_code ec;

    uint32_t ofPort = OFPP_NONE;
    const optional<string>& ofPortName = as.getInterfaceName();
    if (ofPortName)
        ofPort = switchManager.getPortMapper().FindPort(ofPortName.get());

    optional<shared_ptr<RoutingDomain > > rd =
        RoutingDomain::resolve(agent.getFramework(),
                               as.getDomainURI().get());

    if (rd) {
        uint8_t smacAddr[6];
        const uint8_t* macAddr = smacAddr;
        if (as.getServiceMAC()) {
            as.getServiceMAC().get().toUIntArray(smacAddr);
        } else {
            macAddr = getRouterMacAddr();
        }

        uint32_t rdId = getId(RoutingDomain::CLASS_ID, as.getDomainURI().get());
        uint32_t ctMark = idGen.getId(ID_NMSPC_SERVICE, uuid);
        if (as.getInterfaceName())
            ctMark |= 1 << 31;

        for (auto const& sm : as.getServiceMappings()) {
            if (!sm.getServiceIP())
                continue;

            uint16_t zoneId = -1;
            if (conntrackEnabled && sm.isConntrackMode()) {
                zoneId = ctZoneManager.getId(as.getDomainURI()->toString());
                if (zoneId == static_cast<uint16_t>(-1))
                    LOG(ERROR) << "Could not allocate connection tracking"
                               << " zone for "
                               << as.getDomainURI().get();
            }

            address serviceAddr =
                address::from_string(sm.getServiceIP().get(), ec);
            if (ec) {
                LOG(WARNING) << "Invalid service IP: "
                             << sm.getServiceIP().get()
                             << ": " << ec.message();
                continue;
            }

            vector<address> nextHopAddrs;
            for (const string& ipstr : sm.getNextHopIPs()) {
                auto nextHopAddr = address::from_string(ipstr, ec);
                if (ec) {
                    LOG(WARNING) << "Invalid service next hop IP: "
                                 << ipstr << ": " << ec.message();
                } else {
                    nextHopAddrs.push_back(nextHopAddr);
                }
            }

            uint8_t proto = 0;
            if (sm.getServiceProto()) {
                const string& protoStr = sm.getServiceProto().get();
                if ("udp" == protoStr)
                    proto = 17;
                else
                    proto = 6;
            }


            uint16_t link = 0;
            for (const address& nextHopAddr : nextHopAddrs) {
                {
                    FlowBuilder ipMap;
                    matchDestDom(ipMap, 0, rdId);
                    matchActionServiceProto(ipMap, proto, sm, true, true);
                    ipMap.ipDst(serviceAddr);

                    // use the first address as a "default" so that
                    // there is no transient case where there is no
                    // match while flows are updated.
                    if (link == 0) {
                        ipMap.priority(99);
                    } else {
                        ipMap.priority(100)
                            .reg(7, link);
                    }
                    ipMap.action().ipDst(nextHopAddr).decTtl();

                    if (as.getServiceMode() == Service::LOADBALANCER) {
                        // For LB: save v4 service address to reg8
                        // Save v6 addr in regs 8 to 11
                        if (serviceAddr.is_v4()) {
                            ipMap.action()
                                .reg(MFF_REG8, serviceAddr.to_v4().to_ulong());
                        } else {
                            uint32_t pAddr[4];
                            in6AddrToLong(serviceAddr, &pAddr[0]);
                            ipMap.action()
                                .reg(MFF_REG8, pAddr[0])
                                .reg(MFF_REG9, pAddr[1])
                                .reg(MFF_REG10, pAddr[2])
                                .reg(MFF_REG11, pAddr[3]);
                        }

                        if (zoneId != static_cast<uint16_t>(-1)) {
                            uint32_t metav = as.getInterfaceName()
                                ? flow::meta::FROM_SERVICE_INTERFACE
                                : 0;

                            ipMap.metadata(metav,
                                           flow::meta::FROM_SERVICE_INTERFACE);

                            ActionBuilder setMark;
                            setMark.reg(MFF_CT_MARK, ctMark);
                            ipMap.action()
                                .conntrack(ActionBuilder::CT_COMMIT,
                                           static_cast<mf_field_id>(0),
                                           zoneId, 0xff, 0, setMark);
                        }

                        // If a cookie was already allocated by updateSvcTgtStatsFlows(), then
                        // use it.
                        if (!serviceStatsFlowDisabled) {
                            const string& ingStr = "antosvc:svc-tgt:"+uuid+":"+nextHopAddr.to_string();
                            uint32_t cookieIdIg = idGen.getIdNoAlloc(ID_NMSPC_SVCSTATS, ingStr);
                            if (cookieIdIg != static_cast<uint32_t>(-1)) {
                                ipMap.cookie(ovs_htonll((uint64_t)cookieIdIg))
                                     .flags(OFPUTIL_FF_SEND_FLOW_REM);
                            }
                        }

                        ipMap.action()
                            .metadata(flow::meta::ROUTED, flow::meta::ROUTED)
                            .go(ROUTE_TABLE_ID);
                    } else if (as.getServiceMode() == Service::LOCAL_ANYCAST &&
                               ofPort != OFPP_NONE) {
                        ipMap.action().output(ofPort);
                    }

                    ipMap.build(serviceNextHopFlows);
                }
                if (as.getServiceMode() == Service::LOADBALANCER) {
                    // For load balanced services reverse traffic is
                    // handled with normal policy semantics
                    if (zoneId != static_cast<uint16_t>(-1)) {
                        if (encapType == ENCAP_VLAN) {
                            // traffic from the uplink will originally
                            // have had a vlan tag that was stripped
                            // in the source table.  Restore the tag
                            // before running through the conntrack
                            // table to ensure we can property rebuild
                            // the state when the packet comes back.
                            flowRevMapCt(serviceRevFlows, 101,
                                         sm, nextHopAddr, rdId, zoneId, proto,
                                         getTunnelPort(), ENCAP_VLAN);
                        }
                        flowRevMapCt(serviceRevFlows, 100,
                                     sm, nextHopAddr, rdId, zoneId, proto,
                                     0, ENCAP_NONE);
                    }
                    {
                        FlowBuilder ipRevMap;
                        matchDestDom(ipRevMap, 0, rdId);
                        matchActionServiceProto(ipRevMap, proto, sm,
                                                false, true);

                        // If a cookie was already allocated by updateSvcTgtStatsFlows(), then
                        // use it.
                        if (!serviceStatsFlowDisabled) {
                            const string& egrStr = "svctoan:svc-tgt:"+uuid+":"+nextHopAddr.to_string();
                            uint32_t cookieIdEg = idGen.getIdNoAlloc(ID_NMSPC_SVCSTATS, egrStr);
                            if (cookieIdEg != static_cast<uint32_t>(-1)) {
                                ipRevMap.cookie(ovs_htonll((uint64_t)cookieIdEg))
                                        .flags(OFPUTIL_FF_SEND_FLOW_REM);
                            }
                        }
                        ipRevMap.priority(100)
                            .ipSrc(nextHopAddr)
                            .action()
                            .ethSrc(macAddr)
                            .ipSrc(serviceAddr)
                            .decTtl();
                        if (zoneId != static_cast<uint16_t>(-1)) {
                            ipRevMap
                                .conntrackState(FlowBuilder::CT_TRACKED |
                                                FlowBuilder::CT_ESTABLISHED,
                                                FlowBuilder::CT_TRACKED |
                                                FlowBuilder::CT_ESTABLISHED |
                                                FlowBuilder::CT_INVALID |
                                                FlowBuilder::CT_NEW);

                            ipRevMap.ctMark(ctMark);
                        }
                        if (!as.getInterfaceName()) {
                            ipRevMap.action()
                                .metadata(flow::meta::ROUTED,
                                          flow::meta::ROUTED)
                                .go(BRIDGE_TABLE_ID);
                        } else if (ofPort != OFPP_NONE) {
                            if (as.getIfaceVlan()) {
                                ipRevMap.action()
                                    .pushVlan()
                                    .setVlanVid(as.getIfaceVlan().get());
                            }
                            ipRevMap.action()
                                .ethDst(getRouterMacAddr())
                                .output(ofPort);
                        }
                        ipRevMap.build(serviceRevFlows);
                    }
                }

                link += 1;
            }

        }
    }

    switchManager.writeFlow(uuid, SERVICE_REV_TABLE_ID, serviceRevFlows);
    switchManager.writeFlow(uuid, SERVICE_NEXTHOP_TABLE_ID,
                            serviceNextHopFlows);
}

void IntFlowManager::handleServiceUpdate(const string& uuid) {
    LOG(DEBUG) << "Updating service " << uuid;

    ServiceManager& srvMgr = agent.getServiceManager();
    shared_ptr<const Service> asWrapper = srvMgr.getService(uuid);

    if (!asWrapper || !asWrapper->getDomainURI()) {
        switchManager.clearFlows(uuid, SEC_TABLE_ID);
        switchManager.clearFlows(uuid, BRIDGE_TABLE_ID);
        switchManager.clearFlows(uuid, SERVICE_REV_TABLE_ID);
        switchManager.clearFlows(uuid, SERVICE_DST_TABLE_ID);
        switchManager.clearFlows(uuid, SERVICE_NEXTHOP_TABLE_ID);
        updateSvcStatsFlows(uuid, true, false);
        idGen.erase(ID_NMSPC_SERVICE, uuid);
        return;
    }

    const Service& as = *asWrapper;

    FlowEntryList secFlows;
    FlowEntryList bridgeFlows;
    FlowEntryList serviceDstFlows;

    boost::system::error_code ec;

    uint32_t ofPort = OFPP_NONE;
    const optional<string>& ofPortName = as.getInterfaceName();
    if (ofPortName)
        ofPort = switchManager.getPortMapper().FindPort(ofPortName.get());

    optional<shared_ptr<RoutingDomain > > rd =
        RoutingDomain::resolve(agent.getFramework(),
                               as.getDomainURI().get());

    if (rd) {
        uint8_t smacAddr[6];
        const uint8_t* macAddr = smacAddr;
        if (as.getServiceMAC()) {
            as.getServiceMAC().get().toUIntArray(smacAddr);
        } else {
            macAddr = getRouterMacAddr();
        }

        uint32_t rdId = getId(RoutingDomain::CLASS_ID, as.getDomainURI().get());
        uint32_t ctMark = idGen.getId(ID_NMSPC_SERVICE, uuid);
        if (as.getInterfaceName())
            ctMark |= 1 << 31;

        // Add stats flows for service metric collection
        updateSvcStatsFlows(uuid, true, true);

        for (auto const& sm : as.getServiceMappings()) {
            if (!sm.getServiceIP())
                continue;

            uint16_t zoneId = -1;
            if (conntrackEnabled && sm.isConntrackMode()) {
                zoneId = ctZoneManager.getId(as.getDomainURI()->toString());
                if (zoneId == static_cast<uint16_t>(-1))
                    LOG(ERROR) << "Could not allocate connection tracking"
                               << " zone for "
                               << as.getDomainURI().get();
            }

            address serviceAddr =
                address::from_string(sm.getServiceIP().get(), ec);
            if (ec) {
                LOG(WARNING) << "Invalid service IP: "
                             << sm.getServiceIP().get()
                             << ": " << ec.message();
                continue;
            }

            vector<address> nextHopAddrs;
            for (const string& ipstr : sm.getNextHopIPs()) {
                auto nextHopAddr = address::from_string(ipstr, ec);
                if (ec) {
                    LOG(WARNING) << "Invalid service next hop IP: "
                                 << ipstr << ": " << ec.message();
                } else {
                    nextHopAddrs.push_back(nextHopAddr);
                }
            }

            uint8_t proto = 0;
            if (sm.getServiceProto()) {
                const string& protoStr = sm.getServiceProto().get();
                if ("udp" == protoStr)
                    proto = 17;
                else
                    proto = 6;
            }

            // Traffic sent to services is intercepted in the bridge
            // table, despite the fact that it is effectively
            // performing a routing action for historical reasons
            {
                FlowBuilder serviceDest;
                matchDestDom(serviceDest, 0, rdId);
                matchActionServiceProto(serviceDest, proto, sm, true, false);
                if (as.getServiceMAC() &&
                    as.getServiceMode() == Service::LOADBALANCER)
                    serviceDest.ethDst(macAddr);

                serviceDest
                    .priority(50)
                    .ipDst(serviceAddr);
                if (as.getServiceMode() == Service::LOCAL_ANYCAST) {
                    serviceDest.action().ethSrc(getRouterMacAddr());
                }

                if (!nextHopAddrs.empty()) {
                    // map traffic to service to the next hop IPs
                    // using DNAT semantics
                    if (as.getServiceMode() == Service::LOCAL_ANYCAST) {
                        serviceDest.action().ethDst(macAddr);
                    } else {
                        serviceDest.action().ethDst(getRouterMacAddr());
                    }
                    serviceDest.action()
                        .multipath(NX_HASH_FIELDS_SYMMETRIC_L3L4_UDP,
                                   1024,
                                   ActionBuilder::NX_MP_ALG_ITER_HASH,
                                   static_cast<uint16_t>(nextHopAddrs.size()-1),
                                   32, MFF_REG7)
                        .go(SERVICE_NEXTHOP_TABLE_ID);
                } else if (as.getServiceMode() == Service::LOCAL_ANYCAST &&
                           ofPort != OFPP_NONE) {
                    serviceDest.action()
                        .ethDst(macAddr).decTtl()
                        .output(ofPort);
                }

                serviceDest.build(bridgeFlows);
            }

            if (ofPort != OFPP_NONE) {
                if (as.getServiceMode() == Service::LOCAL_ANYCAST) {
                    // For anycast services with a service interface,
                    // traffic sent from the interface is intercepted in
                    // the security table to prevent normal processing
                    // semantics, since there is otherwise no way for the
                    // policy to allow the traffic
                    if (nextHopAddrs.empty())
                        nextHopAddrs.emplace_back();

                    for (const address& nextHopAddr : nextHopAddrs) {
                        FlowBuilder svcIp;
                        svcIp.priority(100)
                            .inPort(ofPort)
                            .ethSrc(macAddr)
                            .action()
                            .reg(MFF_REG6, rdId);

                        if (nextHopAddr != address()) {
                            // If there is a next hop mapping, map the return
                            // traffic from service interface using DNAT
                            // semantics
                            svcIp.ipSrc(nextHopAddr)
                                .action()
                                .ipSrc(serviceAddr)
                                .decTtl()
                                .metadata(flow::meta::ROUTED,
                                          flow::meta::ROUTED);
                        } else {
                            svcIp.ipSrc(serviceAddr);
                        }

                        svcIp.action()
                            .go(SERVICE_DST_TABLE_ID);
                        svcIp.build(secFlows);

                        if (serviceAddr.is_v4()) {
                            // Note that v6 neighbor discovery is
                            // handled by the regular IP rules
                            FlowBuilder().priority(100)
                                .inPort(ofPort)
                                .ethSrc(macAddr)
                                .arpSrc(nextHopAddr != address()
                                        ? nextHopAddr : serviceAddr)
                                .action()
                                .reg(MFF_REG6, rdId)
                                .go(SERVICE_DST_TABLE_ID)
                                .parent().build(secFlows);
                        }
                    }

                    // Reply to ARP/ND requests for the service
                    // address
                    flowsProxyDiscovery(*this, bridgeFlows,
                                        51, serviceAddr, macAddr,
                                        0, rdId, 0);

                    if (sm.getGatewayIP()) {
                        address gwAddr =
                            address::from_string(sm.getGatewayIP().get(),
                                                 ec);
                        if (ec) {
                            LOG(WARNING) << "Invalid service gateway IP: "
                                         << sm.getGatewayIP().get()
                                         << ": " << ec.message();
                        } else {
                            flowsProxyDiscovery(*this, serviceDstFlows, 31,
                                                gwAddr, getRouterMacAddr(),
                                                0, rdId, 0, true, macAddr);
                        }
                    }
                }
            }
        }

        if (as.getServiceMode() == Service::LOADBALANCER &&
            ofPort != OFPP_NONE) {
            // For load balancer services with a service interface, we
            // allow traffic from the service interface, strip any
            // VLAN tag, and set the policy applied bit.  Because
            // these packets are already allowed by a service policy,
            // we bypass regular policy semantics for these packets,
            // but use the regular forwarding pipeline.

            IntFlowManager::EncapType serviceEncapType =
                IntFlowManager::ENCAP_NONE;
            uint32_t proxyVnid = 0;
            if (as.getIfaceVlan()) {
                proxyVnid = as.getIfaceVlan().get();
                serviceEncapType = IntFlowManager::ENCAP_VLAN;
            }

            FlowBuilder svcIface;
            svcIface.priority(90)
                .inPort(ofPort);
            if (as.getServiceMAC() != boost::none) {
                uint8_t svcMac[6];
                as.getServiceMAC().get().toUIntArray(svcMac);
                svcIface.ethDst(svcMac);
            }
            if (as.getIfaceVlan()) {
                svcIface.vlan(as.getIfaceVlan().get());
                svcIface.action().popVlan();
            }
            svcIface.action()
                .reg(MFF_REG0, proxyVnid)
                .reg(MFF_REG6, rdId)
                .metadata(flow::meta::POLICY_APPLIED |
                          flow::meta::FROM_SERVICE_INTERFACE,
                          flow::meta::POLICY_APPLIED|
                          flow::meta::FROM_SERVICE_INTERFACE)
                .go(BRIDGE_TABLE_ID);
            svcIface.build(secFlows);

            FlowBuilder svcArp;
            svcArp.priority(90)
                .inPort(ofPort)
                .ethType(eth::type::ARP)
                .proto(arp::op::REQUEST)
                .ethDst(packets::MAC_ADDR_BROADCAST);
            if (as.getIfaceVlan()) {
                svcArp.vlan(as.getIfaceVlan().get());
                svcArp.action().popVlan();
            }
            svcArp.action()
                .reg(MFF_REG0, proxyVnid)
                .reg(MFF_REG6, rdId)
                .metadata(flow::meta::POLICY_APPLIED |
                          flow::meta::FROM_SERVICE_INTERFACE,
                          flow::meta::POLICY_APPLIED|
                          flow::meta::FROM_SERVICE_INTERFACE)
                .go(BRIDGE_TABLE_ID);
            svcArp.build(secFlows);

            if (as.getIfaceIP()) {
                // Reply to ARP/ND requests for the iface address
                address ifaceAddr =
                    address::from_string(as.getIfaceIP().get(), ec);
                if (ec) {
                    LOG(WARNING) << "Invalid service interface address: "
                                 << as.getIfaceIP().get()
                                 << ":" << ec.message();
                } else {
                    flowsProxyDiscovery(bridgeFlows,
                                        51, ifaceAddr, macAddr,
                                        proxyVnid, rdId, 0, false, NULL,
                                        ofPort, serviceEncapType, true);
                    flowsProxyICMP(bridgeFlows, 51, ifaceAddr, 0, rdId);
                }
            }
        }
    }

    programServiceSnatDnatFlows(uuid);
    switchManager.writeFlow(uuid, SEC_TABLE_ID, secFlows);
    switchManager.writeFlow(uuid, BRIDGE_TABLE_ID, bridgeFlows);
    switchManager.writeFlow(uuid, SERVICE_DST_TABLE_ID, serviceDstFlows);
}

void IntFlowManager::handleLearningBridgeIfaceUpdate(const string& uuid) {
    LOG(DEBUG) << "Updating learning bridge interface " << uuid;

    LearningBridgeManager& lbMgr = agent.getLearningBridgeManager();
    shared_ptr<const LearningBridgeIface> iface = lbMgr.getLBIface(uuid);

    if (!iface) {
        switchManager.clearFlows(uuid, SEC_TABLE_ID);
        return;
    }

    uint32_t ofPort = OFPP_NONE;
    const optional<string>& ofPortName = iface->getInterfaceName();
    if (ofPortName) {
        ofPort = switchManager.getPortMapper().FindPort(ofPortName.get());
    }

    MaskList trunkVlans;
    for (auto& range : iface->getTrunkVlans()) {
        MaskList masks;
        RangeMask::getMasks(range.first, range.second, masks);
        trunkVlans.insert(trunkVlans.end(), masks.begin(), masks.end());
    }

    FlowEntryList secFlows;

    if (ofPort != OFPP_NONE) {
        for (const Mask& m : trunkVlans) {
            uint16_t tci = 0x1000 | m.first;
            uint16_t mask = 0x1000 | (0xfff & m.second);
            FlowBuilder().priority(501).inPort(ofPort)
                .tci(tci, mask)
                .ethDst(packets::MAC_ADDR_FILTERED,
                        packets::MAC_ADDR_FILTERED_MASK)
                .build(secFlows);
            FlowBuilder().priority(501).inPort(ofPort)
                .tci(tci, mask)
                .ethSrc(packets::MAC_ADDR_MULTICAST,
                        packets::MAC_ADDR_MULTICAST)
                .build(secFlows);
            // Cookie is assigned based on ofPort and VLAN; removal of
            // this flow removes all associated learned flows because
            // of the NX_LEARN_F_DELETE_LEARNED set in the learn
            // flags.
            uint64_t cookie = (uint64_t)ofPort << 32 |
                ((uint64_t)tci) | ((uint64_t)mask << 16);
            FlowBuilder().priority(500).inPort(ofPort)
                .tci(tci, mask)
                .action()
                .macVlanLearn(OFP_DEFAULT_PRIORITY, ovs_htonll(cookie),
                              LEARN_TABLE_ID)
                .go(LEARN_TABLE_ID)
                .parent().build(secFlows);
        }
    }

    switchManager.writeFlow(uuid, SEC_TABLE_ID, secFlows);
}

void IntFlowManager::
handleLearningBridgeVlanUpdate(LearningBridgeIface::vlan_range_t vlan) {
    LOG(DEBUG) << "Updating learning bridge vlan range " << vlan;

    LearningBridgeManager& lbMgr = agent.getLearningBridgeManager();

    unordered_set<string> ifaces;
    lbMgr.getIfacesByVlanRange(vlan, ifaces);
    std::set<uint16_t> ofPorts;
    for (auto& uuid : ifaces) {
        shared_ptr<const LearningBridgeIface> iface =
            lbMgr.getLBIface(uuid);
        if (!iface) continue;

        const optional<string>& ofPortName = iface->getInterfaceName();
        if (!ofPortName) continue;

        uint32_t ofPort =
            switchManager.getPortMapper().FindPort(ofPortName.get());
        if (ofPort == OFPP_NONE) continue;

        ofPorts.insert(ofPort);
    }

    FlowEntryList learnFlows;

    if (!ofPorts.empty()) {
        MaskList vlanMasks;
        RangeMask::getMasks(vlan.first, vlan.second, vlanMasks);
        for (const Mask& m : vlanMasks) {
            FlowBuilder flood;
            flood.priority(1)
                .tci(0x1000 | m.first, 0x1000 | (0xfff & m.second));
            for (auto ofPort : ofPorts) {
                flood.action().output(ofPort);
            }
            flood.build(learnFlows);
        }
    }

    switchManager.writeFlow(boost::lexical_cast<string>(vlan),
                            LEARN_TABLE_ID, learnFlows);
}

void IntFlowManager::handleSnatUpdate(const string& snatUuid) {
    LOG(DEBUG) << "Updating snat " << snatUuid;

    SnatManager& snatMgr = agent.getSnatManager();
    unordered_set<string> uuids;

    snatMgr.getEndpoints(snatUuid, uuids);
    for (const string& uuid : uuids) {
         LOG(DEBUG) << "Updating endpoint " << uuid;
         endpointUpdated(uuid);
    }

    shared_ptr<const Snat> asWrapper = snatMgr.getSnat(snatUuid);
    if (!asWrapper || asWrapper->getUUID() != snatUuid) {
        LOG(DEBUG) << "Clearing snat for uuid " << snatUuid;
        switchManager.clearFlows(snatUuid, SEC_TABLE_ID);
        switchManager.clearFlows(snatUuid, SNAT_REV_TABLE_ID);
        return;
    }

    const Snat& as = *asWrapper;
    LOG(DEBUG) << as;

    FlowEntryList toSnatFlows;
    FlowEntryList snatFlows;
    uint16_t zoneId = 0;
    boost::system::error_code ec;
    address addr = address::from_string(as.getSnatIP(), ec);
    if (ec) return;
    uint32_t snatPort = switchManager.getPortMapper()
                                     .FindPort(as.getInterfaceName());
    if (snatPort == OFPP_NONE) return;
    if (as.getZone())
        zoneId = as.getZone().get();
    if (zoneId == 0)
        zoneId = ctZoneManager.getId(as.getUUID());
    vector<uint8_t> protoVec;
    protoVec.push_back(6);
    protoVec.push_back(17);
    uint8_t dmac[6];
    uint8_t ifcMac[6];
    bool hasIfcMac = as.getInterfaceMAC() != boost::none;

    if (!hasIfcMac) {
        LOG(ERROR) << "missing inteface mac in snat "
                   << as;
        return;
    }
    as.getInterfaceMAC().get().toUIntArray(ifcMac);

    /**
     * Either redirect to snat rev table for local snat processing or
     * rewrite destination macaddr and bounce it out same interface
     */
    Snat::PortRangeMap portRangeMap = as.getPortRangeMap();
    for (auto it : portRangeMap) {
        bool local = false;
        if (it.first == "local") {
            local = true;
        } else {
            try {
                MAC(it.first).toUIntArray(dmac);
            } catch (std::invalid_argument&) {
                LOG(ERROR) << "Invalid destination mac for snat: " << it.first;
                continue;
            }
        }
        boost::optional<Snat::PortRanges> prs = it.second;
        if (prs != boost::none && prs.get().size() > 0) {
            for (const auto& pr : prs.get()) {
                MaskList snatMasks;
                RangeMask::getMasks(pr.start, pr.end, snatMasks);
                for (const Mask& m : snatMasks) {
                    for (auto protocol : protoVec) {
                        FlowBuilder maskedFlow;
                        if (local)
                            maskedFlow.priority(200);
                        else
                            maskedFlow.priority(199);
                        maskedFlow.inPort(snatPort)
                                  .ethDst(ifcMac)
                                  .ipDst(addr)
                                  .proto(protocol)
                                  .tpDst(m.first, m.second);
                        if (as.getIfaceVlan())
                            maskedFlow.vlan(as.getIfaceVlan().get());
                        if (local) {
                            if (as.getIfaceVlan())
                                maskedFlow.action().popVlan();
                            maskedFlow.action().go(SNAT_REV_TABLE_ID);
                        } else {
                            maskedFlow.action().ethDst(dmac)
                                               .output(OFPP_IN_PORT);
                        }
                        maskedFlow.build(toSnatFlows);
                    }
                }
            }
        }
    }

    ActionBuilder fna;
    fna.unnat();
    FlowBuilder()
        .priority(10)
        .ethType(eth::type::IP)
        .conntrackState(0, FlowBuilder::CT_TRACKED)
        .action()
            .conntrack(0, static_cast<mf_field_id>(0),
                       zoneId, SNAT_REV_TABLE_ID, 0, fna)
        .parent().build(snatFlows);

    switchManager.writeFlow(snatUuid, SEC_TABLE_ID, toSnatFlows);
    switchManager.writeFlow(snatUuid, SNAT_REV_TABLE_ID, snatFlows);
}

void IntFlowManager::updateEPGFlood(const URI& epgURI, uint32_t epgVnid,
                                    uint32_t fgrpId, const address& epgTunDst,
                                    bool isLocalExtDomain) {
    uint8_t bcastFloodMode = BcastFloodModeEnumT::CONST_NORMAL;
    if(!isLocalExtDomain) {
        optional<shared_ptr<FloodDomain> > fd =
            agent.getPolicyManager().getFDForGroup(epgURI);
        if (fd) {
            bcastFloodMode =
                fd.get()->getBcastFloodMode(BcastFloodModeEnumT::CONST_NORMAL);
        }
    }

    FlowEntryList grpDst;
    {
        // deliver broadcast/multicast traffic to the group table
        FlowBuilder mcast;
        matchFd(mcast, fgrpId, true);
        mcast.priority(10)
            .reg(0, epgVnid);
        if (bcastFloodMode == BcastFloodModeEnumT::CONST_ISOLATED) {
            // In isolated mode deliver only if policy has already
            // been applied (i.e. it comes from the tunnel uplink)
            mcast.metadata(flow::meta::POLICY_APPLIED,
                           flow::meta::POLICY_APPLIED);
        }
        if(!isLocalExtDomain) {
            switch (getEncapType()) {
            case ENCAP_VLAN:
                break;
            case ENCAP_VXLAN:
            case ENCAP_IVXLAN:
            default:
                mcast.action().reg(MFF_REG7, epgTunDst.to_v4().to_ulong());
                break;
            }
        } else {
            /* Unknown bridged unicast packets should be sent to uplink
             * irrespective of inventory mode for external svi BD
             */
            FlowBuilder ucast;
            actionOutputToEPGTunnel(ucast.priority(2))
                .build(grpDst);
        }
        mcast.action()
            .metadata(flow::meta::out::FLOOD, flow::meta::out::MASK)
            .go(IntFlowManager::STATS_TABLE_ID);
        mcast.build(grpDst);
    }
    switchManager.writeFlow(epgURI.toString(), BRIDGE_TABLE_ID, grpDst);
}

void IntFlowManager::createStaticFlows() {
    uint32_t tunPort = getTunnelPort();
    uint32_t uplinkPort = getUplinkPort();

    LOG(DEBUG) << "Writing static flows";

    {
        // static port security flows
        FlowEntryList portSec;
        {
            // Drop IP traffic that doesn't have the correct source
            // address
            FlowBuilder().priority(25).cookie(flow::cookie::TABLE_DROP_FLOW)
                    .flags(OFPUTIL_FF_SEND_FLOW_REM)
                    .ethType(eth::type::ARP)
                    .action().dropLog(SEC_TABLE_ID)
                    .go(EXP_DROP_TABLE_ID).parent().build(portSec);
            FlowBuilder().priority(25).cookie(flow::cookie::TABLE_DROP_FLOW)
                    .flags(OFPUTIL_FF_SEND_FLOW_REM)
                    .ethType(eth::type::IP)
                    .action().dropLog(SEC_TABLE_ID)
                    .go(EXP_DROP_TABLE_ID).parent().build(portSec);
            FlowBuilder().priority(25).cookie(flow::cookie::TABLE_DROP_FLOW)
                    .flags(OFPUTIL_FF_SEND_FLOW_REM)
                    .ethType(eth::type::IPV6)
                    .action().dropLog(SEC_TABLE_ID)
                    .go(EXP_DROP_TABLE_ID).parent().build(portSec);
        }
        {
            // Allow DHCP requests but not replies
            actionSecAllow(flowutils::match_dhcp_req(FlowBuilder().priority(27),
                                                     true))
                .build(portSec);
        }
        {
            // Allow IPv6 autoconfiguration (DHCP & router solicitiation)
            // requests but not replies
            actionSecAllow(flowutils::match_dhcp_req(FlowBuilder().priority(27),
                                                     false))
                .build(portSec);
            actionSecAllow(FlowBuilder().priority(27)
                           .ethType(eth::type::IPV6)
                           .proto(58)
                           .tpSrc(ND_ROUTER_SOLICIT)
                           .tpDst(0))
                .build(portSec);
        }
        if (tunPort != OFPP_NONE && encapType != ENCAP_NONE) {
            // allow all traffic from the tunnel uplink through the port
            // security table
            actionSecAllow(FlowBuilder().priority(50).inPort(tunPort))
                .build(portSec);
        }
        if (uplinkPort != OFPP_NONE && encapType != ENCAP_NONE) {
            // allow all traffic from the uplink through the port
            // security table
            actionSecAllow(FlowBuilder().priority(50).inPort(uplinkPort))
                .build(portSec);
        }
        switchManager.writeFlow("static", SEC_TABLE_ID, portSec);
    }
    {
        // For non-service flows, proceed to the bridge table
        FlowBuilder nonServiceFlow;
        nonServiceFlow.priority(1).action().go(BRIDGE_TABLE_ID);
        switchManager.writeFlow("static", SERVICE_REV_TABLE_ID,
                                nonServiceFlow);
    }
    {
        FlowEntryList policyStatic;

        // Bypass policy for flows that have the bypass policy bit set
        FlowBuilder().priority(PolicyManager::MAX_POLICY_RULE_PRIORITY + 51)
            .metadata(flow::meta::FROM_SERVICE_INTERFACE,
                      flow::meta::FROM_SERVICE_INTERFACE)
            .action().go(IntFlowManager::STATS_TABLE_ID)
            .parent().build(policyStatic);

        // Block flows from the uplink when not allowed by
        // higher-priority per-EPG rules to allow them.
        FlowBuilder().priority(PolicyManager::MAX_POLICY_RULE_PRIORITY + 50)
            .metadata(flow::meta::POLICY_APPLIED, flow::meta::POLICY_APPLIED)
            .build(policyStatic);

        // Implicitly allow ARP and neighbor discovery unless a rule
        // blocks them.
        FlowBuilder().priority(10)
            .ethType(eth::type::ARP)
            .action().go(IntFlowManager::STATS_TABLE_ID)
            .parent().build(policyStatic);
        FlowBuilder().priority(10)
            .ethType(eth::type::IPV6)
            .proto(58)
            .tpSrc(ND_NEIGHBOR_SOLICIT)
            .tpDst(0)
            .action().go(IntFlowManager::STATS_TABLE_ID)
            .parent().build(policyStatic);
        FlowBuilder().priority(10)
            .ethType(eth::type::IPV6)
            .proto(58)
            .tpSrc(ND_NEIGHBOR_ADVERT)
            .tpDst(0)
            .action().go(IntFlowManager::STATS_TABLE_ID)
            .parent().build(policyStatic);

        switchManager.writeFlow("static", POL_TABLE_ID, policyStatic);
    }
    {
        using namespace modelgbp::platform;
        auto invType = RemoteInventoryTypeEnumT::CONST_NONE;
        if (encapType != ENCAP_NONE && encapType != ENCAP_VLAN) {
            auto config =
                Config::resolve(agent.getFramework(),
                                agent.getPolicyManager().getOpflexDomain());
            if (config)
                invType = config.get()->
                    getInventoryType(RemoteInventoryTypeEnumT::CONST_NONE);
        }

        FlowEntryList unknownTunnelBr;
        FlowEntryList unknownTunnelRt;
        if (tunPort != OFPP_NONE &&
            invType != RemoteInventoryTypeEnumT::CONST_COMPLETE) {
            // If the remote inventory allows for unknown remote
            // endpoints, output to the tunnel interface, bypassing
            // policy note that if the flood domain is set to flood
            // unknown, then there will be a higher-priority rule
            // installed for that flood domain.
            actionOutputToEPGTunnel(FlowBuilder().priority(1))
                .build(unknownTunnelBr);

            if (virtualRouterEnabled) {
                actionOutputToEPGTunnel(FlowBuilder().priority(1))
                    .build(unknownTunnelRt);
            }
        }
        switchManager.writeFlow("static", BRIDGE_TABLE_ID, unknownTunnelBr);
        switchManager.writeFlow("static", ROUTE_TABLE_ID, unknownTunnelRt);
    }
    {
        FlowEntryList statsFlows;
        FlowBuilder().priority(10)
            .action().go(IntFlowManager::OUT_TABLE_ID)
            .parent().build(statsFlows);

        switchManager.writeFlow("static", STATS_TABLE_ID, statsFlows);
    }
    {
        FlowEntryList outFlows;
        outFlows.push_back(flowutils::default_out_flow());
        {
            FlowBuilder().priority(1)
                .metadata(flow::meta::out::REV_NAT,
                          flow::meta::out::MASK)
                .action().outputReg(MFF_REG7)
                .parent().build(outFlows);
        }
        if (encapType != ENCAP_VLAN && encapType != ENCAP_NONE &&
            tunPort != OFPP_NONE) {
            FlowBuilder().priority(15)
                .metadata(flow::meta::out::REMOTE_TUNNEL,
                          flow::meta::out::MASK)
                .action()
                .regMove(MFF_REG2, MFF_TUN_ID)
                .regMove(MFF_REG7, MFF_TUN_DST)
                .output(tunPort)
                .parent().build(outFlows);
        }
        {
            // send reverse NAT ICMP error packets to controller
            flowsRevNatICMP(outFlows, true, 3 ); // unreachable
            flowsRevNatICMP(outFlows, true, 11); // time exceeded
            flowsRevNatICMP(outFlows, true, 12); // param
        }

        switchManager.writeFlow("static", OUT_TABLE_ID, outFlows);
    }
    {
        TlvEntryList tlvFlows;
        for(int i = 0; i <= 10; i++) {
            FlowBuilder().tlv(0xffff, i, 4, i).buildTlv(tlvFlows);
        }
        FlowBuilder().tlv(0xffff, 11, 16, 11).buildTlv(tlvFlows);
        FlowBuilder().tlv(0xffff, 12, 4, 12).buildTlv(tlvFlows);
        switchManager.writeTlv("DropLogStatic", tlvFlows);
    }
    {
        FlowEntryList dropLogFlows;
        FlowBuilder().priority(0)
                .action().go(IntFlowManager::SEC_TABLE_ID)
                .parent().build(dropLogFlows);
        switchManager.writeFlow("DropLogStatic", DROP_LOG_TABLE_ID, dropLogFlows);
        /* Insert a flow at the end of every table to match dropped packets
         * and go to the drop table where it will be punted to a port when configured
         */
        for(unsigned table_id = SEC_TABLE_ID; table_id < EXP_DROP_TABLE_ID; table_id++) {
            FlowEntryList dropLogFlow;
            FlowBuilder().priority(0).cookie(flow::cookie::TABLE_DROP_FLOW)
                    .flags(OFPUTIL_FF_SEND_FLOW_REM)
                    .action().dropLog(table_id)
                    .go(EXP_DROP_TABLE_ID)
                    .parent().build(dropLogFlow);
            switchManager.writeFlow("DropLogStatic", table_id, dropLogFlow);
        }
        handleDropLogPortUpdate();
    }

}

void IntFlowManager::handleEndpointGroupDomainUpdate(const URI& epgURI) {
    LOG(DEBUG) << "Updating endpoint-group " << epgURI;

    const string& epgId = epgURI.toString();

    uint32_t tunPort = getTunnelPort();
    address epgTunDst = getEPGTunnelDst(epgURI);

    PolicyManager& polMgr = agent.getPolicyManager();
    if (!polMgr.groupExists(epgURI)) {  // EPG removed
        switchManager.clearFlows(epgId, SRC_TABLE_ID);
        switchManager.clearFlows(epgId, POL_TABLE_ID);
        switchManager.clearFlows(epgId, OUT_TABLE_ID);
        switchManager.clearFlows(epgId, BRIDGE_TABLE_ID);
        updateMulticastList(boost::none, epgURI);
        return;
    }

    uint32_t epgVnid, rdId, bdId, fgrpId;
    optional<URI> fgrpURI, bdURI, rdURI;
    if (!getGroupForwardingInfo(epgURI, epgVnid, rdURI, rdId,
                                bdURI, bdId, fgrpURI, fgrpId)) {
        return;
    }

    FlowEntryList uplinkMatch;
    if (tunPort != OFPP_NONE && encapType != ENCAP_NONE) {
        // Assign the source registers based on the VNID from the
        // tunnel uplink
        actionSource(matchEpg(FlowBuilder().priority(149).inPort(tunPort),
                              encapType, epgVnid),
                     epgVnid, bdId, fgrpId, rdId,
                     IntFlowManager::SERVICE_REV_TABLE_ID, encapType, true)
            .build(uplinkMatch);
    }
    switchManager.writeFlow(epgId, SRC_TABLE_ID, uplinkMatch);

    {
        uint8_t intraGroup = IntraGroupPolicyEnumT::CONST_ALLOW;
        optional<shared_ptr<EpGroup> > epg =
            EpGroup::resolve(agent.getFramework(), epgURI);
        if (epg && epg.get()->isIntraGroupPolicySet()) {
            intraGroup = epg.get()->getIntraGroupPolicy().get();
        }

        FlowBuilder intraGroupFlow;
        uint16_t prio = PolicyManager::MAX_POLICY_RULE_PRIORITY + 100;
        switch (intraGroup) {
        case IntraGroupPolicyEnumT::CONST_DENY:
            prio = PolicyManager::MAX_POLICY_RULE_PRIORITY + 200;
            /* fall through */
        case IntraGroupPolicyEnumT::CONST_REQUIRE_CONTRACT:
            // Only automatically allow intra-EPG traffic if its come
            // from the uplink and therefore already had policy
            // applied.
            intraGroupFlow.metadata(flow::meta::POLICY_APPLIED,
                                    flow::meta::POLICY_APPLIED);
            /* fall through */
        case IntraGroupPolicyEnumT::CONST_ALLOW:
        default:
            intraGroupFlow.action().go(IntFlowManager::STATS_TABLE_ID);
            break;
        }
        flowutils::match_group(intraGroupFlow, prio, epgVnid, epgVnid);
        switchManager.writeFlow(epgId, POL_TABLE_ID, intraGroupFlow);
    }

    if (virtualRouterEnabled && rdId != 0 && bdId != 0) {
        updateGroupSubnets(epgURI, bdId, rdId);

        FlowEntryList bridgel;
        uint8_t routingMode =
            agent.getPolicyManager().getEffectiveRoutingMode(epgURI);

        if (routingMode == RoutingModeEnumT::CONST_ENABLED) {
            FlowBuilder().priority(2)
                .reg(4, bdId)
                .action()
                .go(ROUTE_TABLE_ID)
                .parent().build(bridgel);

            if (routerAdv) {
                FlowBuilder r;
                matchDestNd(r, NULL, bdId, rdId, ND_ROUTER_SOLICIT);
                r.priority(20).cookie(flow::cookie::NEIGH_DISC);
                r.action().controller();
                r.build(bridgel);

                if (!isSyncing)
                    advertManager.scheduleInitialRouterAdv();
            }
        }
        switchManager.writeFlow(bdURI.get().toString(),
                                BRIDGE_TABLE_ID, bridgel);
    }

    updateEPGFlood(epgURI, epgVnid, fgrpId, epgTunDst);

    FlowEntryList egOutFlows;
    {
        // Output table action to resubmit the flow to the bridge
        // table with source registers set to the current EPG
        FlowBuilder().priority(10)
            .reg(7, epgVnid)
            .metadata(flow::meta::out::RESUBMIT_DST, flow::meta::out::MASK)
            .action()
            .reg(MFF_REG0, epgVnid)
            .reg(MFF_REG4, bdId)
            .reg(MFF_REG5, fgrpId)
            .reg(MFF_REG6, rdId)
            .reg(MFF_REG7, (uint32_t)0)
            .reg64(MFF_METADATA, flow::meta::ROUTED)
            .resubmit(OFPP_IN_PORT, BRIDGE_TABLE_ID)
            .parent().build(egOutFlows);
    }
    if (encapType != ENCAP_NONE && tunPort != OFPP_NONE) {
        using namespace modelgbp::platform;
        auto invType = RemoteInventoryTypeEnumT::CONST_NONE;
        if (encapType != ENCAP_VLAN) {
            auto config =
                Config::resolve(agent.getFramework(),
                                agent.getPolicyManager().getOpflexDomain());
            if (config)
                invType = config.get()->
                    getInventoryType(RemoteInventoryTypeEnumT::CONST_NONE);
        }

        if (invType == RemoteInventoryTypeEnumT::CONST_NONE) {
            // Output table action to output to the tunnel appropriate for
            // the source EPG
            FlowBuilder tunnelOut;
            tunnelOut.priority(10)
                .reg(0, epgVnid)
                .metadata(flow::meta::out::TUNNEL, flow::meta::out::MASK);
            actionTunnelMetadata(tunnelOut.action(), encapType, epgTunDst);
            tunnelOut.action().output(tunPort);
            tunnelOut.build(egOutFlows);
        }
        if (encapType != ENCAP_VLAN) {
            // If destination is the router mac, override EPG tunnel
            // and send to unicast tunnel
            FlowBuilder tunnelOutRtr;
            tunnelOutRtr.priority(11)
                .reg(0, epgVnid)
                .metadata(flow::meta::out::TUNNEL, flow::meta::out::MASK);
            if (invType == RemoteInventoryTypeEnumT::CONST_NONE)
                tunnelOutRtr.ethDst(getRouterMacAddr());
            actionTunnelMetadata(tunnelOutRtr.action(),
                                 encapType, getTunnelDst());
            tunnelOutRtr.action().output(tunPort);
            tunnelOutRtr.build(egOutFlows);
        }
    }
    switchManager.writeFlow(epgId, OUT_TABLE_ID, egOutFlows);

    unordered_set<string> epUuids;
    EndpointManager& epMgr = agent.getEndpointManager();
    epMgr.getEndpointsForIPMGroup(epgURI, epUuids);
    std::unordered_set<URI> ipmRds;
    for (const string& uuid : epUuids) {
        std::shared_ptr<const Endpoint> ep = epMgr.getEndpoint(uuid);
        if (!ep) continue;
        const boost::optional<opflex::modb::URI>& egURI = ep->getEgURI();
        if (!egURI) continue;
        boost::optional<std::shared_ptr<modelgbp::gbp::RoutingDomain> > rd =
            polMgr.getRDForGroup(egURI.get());
        if (rd)
            ipmRds.insert(rd.get()->getURI());
    }
    for (const URI& rdURI : ipmRds) {
        // update routing domains that have references to the
        // IP-mapping EPG to ensure external subnets are correctly
        // mapped.
        rdConfigUpdated(rdURI);
    }

    // note this combines with the IPM group endpoints from above:
    epMgr.getEndpointsForGroup(epgURI, epUuids);
    for (const string& uuid : epUuids) {
        advertManager.scheduleEndpointAdv(uuid);
        endpointUpdated(uuid);
    }

    PolicyManager::uri_set_t contractURIs;
    polMgr.getContractsForGroup(epgURI, contractURIs);
    for (const URI& contract : contractURIs) {
        contractUpdated(contract);
    }

    optional<string> epgMcastIp = polMgr.getMulticastIPForGroup(epgURI);
    updateMulticastList(epgMcastIp, epgURI);
    optional<string> fdcMcastIp;
    optional<shared_ptr<FloodContext> > fdCtx =
        polMgr.getFloodContextForGroup(epgURI);
    if (fdCtx) {
        if (fdCtx.get()->getMulticastGroupIP())
            fdcMcastIp = fdCtx.get()->getMulticastGroupIP().get();
        updateMulticastList(fdcMcastIp, fdCtx.get()->getURI());
    }
}

void IntFlowManager::handleLocalExternalDomainUpdated(const opflex::modb::URI &epgURI) {
    // Validate if URI is actually external
    // Generate IDs for bd and fd. rdId should be 0.
    LOG(DEBUG) << "Updating external endpoint-group " << epgURI;

    const string& epgId = epgURI.toString();
    EndpointManager& epMgr = agent.getEndpointManager();
    uint32_t uplinkPort = getUplinkPort();
    address epgTunDst;

    if (!epMgr.localExternalDomainExists(epgURI)) {  // EPG removed
        switchManager.clearFlows(epgId, SRC_TABLE_ID);
        switchManager.clearFlows(epgId, POL_TABLE_ID);
        switchManager.clearFlows(epgId, OUT_TABLE_ID);
        switchManager.clearFlows(epgId, BRIDGE_TABLE_ID);
        opflex::modb::URI fdURI =
                opflex::modb::URI("extfd:" + epgURI.toString());
        opflex::modb::URI bdURI =
                opflex::modb::URI("extbd:" + epgURI.toString());
        uint32_t fgrpId = getId(FloodDomain::CLASS_ID, fdURI);
        localExternalFdSet.erase(fgrpId);
        updateMulticastList(boost::none, epgURI);
        idGen.erase(getIdNamespace(BridgeDomain::CLASS_ID), bdURI.toString());
        idGen.erase(getIdNamespace(FloodDomain::CLASS_ID), fdURI.toString());
        return;
    }

    uint32_t epgVnid, rdId, bdId, fgrpId;
    optional<URI> fgrpURI, bdURI, rdURI;
    if (!getGroupForwardingInfo(epgURI, epgVnid, rdURI, rdId,
                                bdURI, bdId, fgrpURI, fgrpId)) {
        return;
    }

    FlowEntryList uplinkMatch;
    if (uplinkPort != OFPP_NONE && encapType != ENCAP_NONE) {
        // Assign the source registers based on the VNID from the
        // tunnel uplink
        // Note that although the concocted epgVnid(1<<30 + encap_vlan)
        // is passed in, matchEpg masks the vlan_id to 12 bits before matching
        actionSource(matchEpg(FlowBuilder().priority(149).inPort(uplinkPort),
                              ENCAP_VLAN, epgVnid),
                     epgVnid, bdId, fgrpId, rdId,
                     IntFlowManager::SERVICE_REV_TABLE_ID, ENCAP_VLAN, true)
            .build(uplinkMatch);
    }
    switchManager.writeFlow(epgId, SRC_TABLE_ID, uplinkMatch);

    /*Allow traffic within same EPG*/
    FlowBuilder intraGroupFlow;
    uint16_t prio = PolicyManager::MAX_POLICY_RULE_PRIORITY + 100;
    flowutils::match_group(intraGroupFlow, prio, epgVnid, epgVnid);
    intraGroupFlow.action().go(IntFlowManager::STATS_TABLE_ID);
    switchManager.writeFlow(epgId, POL_TABLE_ID, intraGroupFlow);

    updateEPGFlood(epgURI, epgVnid, fgrpId, epgTunDst, true);

    FlowEntryList egOutFlows;
    {
        // Output table action to resubmit the flow to the bridge
        // table with source registers set to the current EPG
        FlowBuilder().priority(10)
            .reg(7, epgVnid)
            .metadata(flow::meta::out::RESUBMIT_DST, flow::meta::out::MASK)
            .action()
            .reg(MFF_REG0, epgVnid)
            .reg(MFF_REG4, bdId)
            .reg(MFF_REG5, fgrpId)
            .reg(MFF_REG6, rdId)
            .reg(MFF_REG7, (uint32_t)0)
            .reg64(MFF_METADATA, flow::meta::ROUTED)
            .resubmit(OFPP_IN_PORT, BRIDGE_TABLE_ID)
            .parent().build(egOutFlows);
    }

    if(uplinkPort != OFPP_NONE) {
        FlowBuilder tunnelOut;
        tunnelOut.priority(10)
            .reg(0, epgVnid)
            .metadata(flow::meta::out::TUNNEL, flow::meta::out::MASK);
        actionTunnelMetadata(tunnelOut.action(), ENCAP_VLAN, epgTunDst);
        tunnelOut.action().output(uplinkPort);
        tunnelOut.build(egOutFlows);
    }

    switchManager.writeFlow(epgId, OUT_TABLE_ID, egOutFlows);

    unordered_set<string> epUuids;
    // note this combines with the IPM group endpoints from above:
    epMgr.getEndpointsForGroup(epgURI, epUuids);
    for (const string& uuid : epUuids) {
        advertManager.scheduleEndpointAdv(uuid);
        endpointUpdated(uuid);
    }

}

void IntFlowManager::updateGroupSubnets(const URI& egURI, uint32_t bdId,
                                        uint32_t rdId) {
    PolicyManager::subnet_vector_t subnets;
    agent.getPolicyManager().getSubnetsForGroup(egURI, subnets);

    uint32_t tunPort = getTunnelPort();

    for (shared_ptr<Subnet>& sn : subnets) {
        FlowEntryList el;

        optional<address> routerIp =
            PolicyManager::getRouterIpForSubnet(*sn);

        // Reply to ARP/ND requests for the router IP only from local
        // endpoints.
        if (routerIp) {
            if (routerIp.get().is_v4()) {
                if (tunPort != OFPP_NONE) {
                    FlowBuilder e0;
                    e0.priority(22).inPort(tunPort);
                    matchDestArp(e0, routerIp.get(), bdId, rdId);
                    e0.build(el);
                }

                FlowBuilder e1;
                e1.priority(20);
                matchDestArp(e1, routerIp.get(), bdId, rdId);
                actionArpReply(e1, getRouterMacAddr(), routerIp.get());
                e1.build(el);
            } else {
                address lladdr =
                    network::construct_link_local_ip_addr(routerMac);
                if (tunPort != OFPP_NONE) {
                    FlowBuilder e0;
                    e0.priority(22)
                        .inPort(tunPort).cookie(flow::cookie::NEIGH_DISC);
                    matchDestNd(e0, &lladdr, bdId, rdId);
                    e0.build(el);
                }

                FlowBuilder e1;
                e1.priority(20).cookie(flow::cookie::NEIGH_DISC);
                matchDestNd(e1, &lladdr, bdId, rdId);
                e1.action().controller();
                e1.build(el);
            }
        }
        switchManager.writeFlow(sn->getURI().toString(), BRIDGE_TABLE_ID, el);
    }
}

void IntFlowManager::handleRoutingDomainUpdate(const URI& rdURI) {
    optional<shared_ptr<RoutingDomain > > rd =
        RoutingDomain::resolve(agent.getFramework(), rdURI);
    // Avoding clash with dropLog flow objId, giving new name
    const string& rdEnfPrefURIId = "EnfPref:" + rdURI.toString();

    if (!rd) {
        LOG(DEBUG) << "Cleaning up for RD: " << rdURI;
        switchManager.clearFlows(rdURI.toString(), NAT_IN_TABLE_ID);
        switchManager.clearFlows(rdURI.toString(), ROUTE_TABLE_ID);
        switchManager.clearFlows(rdURI.toString(), POL_TABLE_ID);
        switchManager.clearFlows(rdEnfPrefURIId, POL_TABLE_ID);
        idGen.erase(getIdNamespace(RoutingDomain::CLASS_ID), rdURI.toString());
        ctZoneManager.erase(rdURI.toString());
#ifdef HAVE_PROMETHEUS_SUPPORT
        prometheusManager.removeRDDropCounter(rdURI.toString());
#endif
        agent.getPolicyManager().deleteRoutingDomain(rdURI);
        return;
    }
    LOG(DEBUG) << "Updating routing domain " << rdURI;
#ifdef HAVE_PROMETHEUS_SUPPORT
    prometheusManager.addNUpdateRDDropCounter(rdURI.toString(),
                                              true);
#endif

    FlowEntryList rdRouteFlows;
    FlowEntryList rdNatFlows;
    boost::system::error_code ec;
    uint32_t tunPort = getTunnelPort();
    uint32_t rdId = getId(RoutingDomain::CLASS_ID, rdURI);

    /* VRF unenforced mode:
     * In this mode, inter epg communication is allowed without any
     * contracts. This is achieved by installing a flow entry on top
     * of existing policy entries for this rtId.
     * Note: As a design choice we still allow the contracts within
     * this VRF to be  downloaded and installed in OVS. This will
     * ensure that contracts dont have to be pulled when this VRF
     * becomes enforced */
    uint8_t enforcementPreference =
                    rd.get()->isEnforcementPreferenceSet()?
                    rd.get()->getEnforcementPreference().get():
                    EnforcementPreferenceTypeEnumT::CONST_ENFORCED;
    if (enforcementPreference
            == EnforcementPreferenceTypeEnumT::CONST_UNENFORCED) {
        LOG(DEBUG) << "Create unenforced flow for RD: " << rdURI;
        FlowBuilder unenforcedFlow;
        unenforcedFlow.priority(PolicyManager::MAX_POLICY_RULE_PRIORITY + 250);
        unenforcedFlow.action().go(IntFlowManager::STATS_TABLE_ID);
        flowutils::match_rdId(unenforcedFlow, rdId);
        switchManager.writeFlow(rdEnfPrefURIId, POL_TABLE_ID, unenforcedFlow);
    } else {
        // Enforced case
        LOG(DEBUG) << "Remove unenforced flow for RD: " << rdURI;
        switchManager.clearFlows(rdEnfPrefURIId, POL_TABLE_ID);
    }

    // For subnets internal to a routing domain, we want to perform
    // ordinary routing without mapping to external network.  These
    // rules are lower priority than the rules that will handle
    // routing to endpoints that are local to this vswitch, so the
    // action is to output to the uplink tunnel.  Match using
    // longest-prefix.

    network::subnets_t intSubnets;

    vector<shared_ptr<RoutingDomainToIntSubnetsRSrc> > subnets_list;
    rd.get()->resolveGbpRoutingDomainToIntSubnetsRSrc(subnets_list);
    for (shared_ptr<RoutingDomainToIntSubnetsRSrc>& subnets_ref :
             subnets_list) {
        optional<URI> subnets_uri = subnets_ref->getTargetURI();
        if (subnets_uri)
            agent.getPolicyManager()
                .addRoutingDomainToSubnets(subnets_uri.get(), rdURI);
        PolicyManager::resolveSubnets(agent.getFramework(),
                                      subnets_uri, intSubnets);
    }
    std::shared_ptr<const RDConfig> rdConfig =
        agent.getExtraConfigManager().getRDConfig(rdURI);
    if (rdConfig) {
        for (const std::string& cidrSn :
                 rdConfig->getInternalSubnets()) {
            network::cidr_t cidr;
            if (network::cidr_from_string(cidrSn, cidr)) {
                intSubnets.insert(make_pair(cidr.first.to_string(),
                                            cidr.second));
            } else {
                LOG(ERROR) << "Invalid CIDR subnet: " << cidrSn;
            }
        }
    }
    for (const network::subnet_t& sn : intSubnets) {
        address addr = address::from_string(sn.first, ec);
        if (ec) continue;

        FlowBuilder snr;
        matchSubnet(snr, rdId, 300, addr, sn.second, false);
        if (tunPort != OFPP_NONE && encapType != ENCAP_NONE) {
            actionOutputToEPGTunnel(snr);
        } else {
            snr.cookie(flow::cookie::TABLE_DROP_FLOW)
               .flags(OFPUTIL_FF_SEND_FLOW_REM)
               .action().dropLog(ROUTE_TABLE_ID)
               .go(EXP_DROP_TABLE_ID);
        }
        snr.build(rdRouteFlows);
    }

    // If we miss the local endpoints and the internal subnets, check
    // each of the external layer 3 networks.  Match using
    // longest-prefix.
    vector<shared_ptr<L3ExternalDomain> > extDoms;
    rd.get()->resolveGbpL3ExternalDomain(extDoms);
    for (shared_ptr<L3ExternalDomain>& extDom : extDoms) {
        vector<shared_ptr<L3ExternalNetwork> > extNets;
        extDom->resolveGbpL3ExternalNetwork(extNets);

        for (shared_ptr<L3ExternalNetwork> net : extNets) {
            uint32_t netVnid = getExtNetVnid(net->getURI());
            vector<shared_ptr<ExternalSubnet> > extSubs;
            net->resolveGbpExternalSubnet(extSubs);
            optional<shared_ptr<L3ExternalNetworkToNatEPGroupRSrc> > natRef =
                net->resolveGbpL3ExternalNetworkToNatEPGroupRSrc();
            optional<uint32_t> natEpgVnid = boost::make_optional<uint32_t>(false, 0);
            if (natRef) {
                optional<URI> natEpg = natRef.get()->getTargetURI();
                if (natEpg)
                    natEpgVnid =
                        agent.getPolicyManager().getVnidForGroup(natEpg.get());
            }

            for (shared_ptr<ExternalSubnet> extsub : extSubs) {
                if (!extsub->isAddressSet() || !extsub->isPrefixLenSet())
                    continue;
                address addr =
                    address::from_string(extsub->getAddress().get(), ec);
                if (ec) continue;

                {
                    FlowBuilder snr;
                    matchSubnet(snr, rdId, 150, addr,
                                extsub->getPrefixLen(0), false);

                    if (natRef) {
                        // For external networks mapped to a NAT EPG,
                        // set the next hop action to NAT_OUT
                        if (natEpgVnid) {
                            snr.action()
                                .reg(MFF_REG2, netVnid)
                                .reg(MFF_REG7, natEpgVnid.get())
                                .metadata(flow::meta::out::NAT,
                                          flow::meta::out::MASK)
                                .go(POL_TABLE_ID);
                        }
                        // else drop until resolved
                    } else if (tunPort != OFPP_NONE &&
                               encapType != ENCAP_NONE) {
                        // For other external networks, output to the tunnel
                        actionOutputToEPGTunnel(snr);
                    } else {
                        snr.cookie(flow::cookie::TABLE_DROP_FLOW)
                           .flags(OFPUTIL_FF_SEND_FLOW_REM)
                           .action().dropLog(ROUTE_TABLE_ID)
                           .go(EXP_DROP_TABLE_ID);
                    }
                    // else drop the packets
                    snr.build(rdRouteFlows);
                }
                {
                    FlowBuilder snn;
                    matchSubnet(snn, rdId, 150, addr,
                                extsub->getPrefixLen(0), true);
                    snn.action()
                        .reg(MFF_REG0, netVnid)
                        // We want to ensure that on the final
                        // delivery of the packet we perform
                        // protocol-specific reverse mapping.  This
                        // doesn't let us do hop-by-hop translations
                        // however.
                        //
                        // Also remove policy applied since we're
                        // changing the effective EPG and need to
                        // apply policy again.
                        .metadata(flow::meta::out::REV_NAT,
                                  flow::meta::out::MASK |
                                  flow::meta::POLICY_APPLIED)
                        .go(POL_TABLE_ID)
                        .parent().build(rdNatFlows);
                }
            }
        }
    }

    switchManager.writeFlow(rdURI.toString(), NAT_IN_TABLE_ID, rdNatFlows);
    switchManager.writeFlow(rdURI.toString(), ROUTE_TABLE_ID, rdRouteFlows);

    std::unordered_set<string> uuids;
    agent.getServiceManager().getServicesByDomain(rdURI, uuids);
    for (const string& uuid : uuids) {
        serviceUpdated(uuid);
    }

    // create drop entry in POL_TABLE_ID for each routingDomain
    // this entry is needed to count all dropped packets per
    // routingDomain and summed up for all the routingDomain to
    // calculate per tenant drop counter.
    switchManager.writeFlow(rdURI.toString(), POL_TABLE_ID,
             FlowBuilder().priority(1)
             .cookie((flow::cookie::TABLE_DROP_FLOW|
                     flow::cookie::RD_POL_DROP_FLOW))
             .flags(OFPUTIL_FF_SEND_FLOW_REM)
             .reg(6, rdId).action()
             .dropLog(POL_TABLE_ID)
             .go(EXP_DROP_TABLE_ID).parent());
}

void
IntFlowManager::handleDomainUpdate(class_id_t cid, const URI& domURI) {

    switch (cid) {
    case RoutingDomain::CLASS_ID:
        handleRoutingDomainUpdate(domURI);
        break;
    case Subnet::CLASS_ID:
        if (!Subnet::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for Subnet: " << domURI;
            switchManager.clearFlows(domURI.toString(), BRIDGE_TABLE_ID);
        }
        break;
    case BridgeDomain::CLASS_ID:
        if (!BridgeDomain::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for BD: " << domURI;
            switchManager.clearFlows(domURI.toString(), BRIDGE_TABLE_ID);
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
            if (removeFromMulticastList(domURI))
                multicastGroupsUpdated();
        }
        break;
    case L3ExternalNetwork::CLASS_ID:
        if (!L3ExternalNetwork::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for L3ExtNet: " << domURI;
            idGen.erase(getIdNamespace(cid), domURI.toString());
        }
        break;
    }
}

/**
 * Construct a bucket object with the specified bucket ID.
 */
static
ofputil_bucket *createBucket(uint32_t bucketId) {
    ofputil_bucket *bkt = (ofputil_bucket *)malloc(sizeof(ofputil_bucket));
    bkt->weight = 0;
    bkt->bucket_id = bucketId;
    bkt->watch_port = OFPP_ANY;
    bkt->watch_group = OFPG_ANY;
    return bkt;
}

GroupEdit::Entry
IntFlowManager::createGroupMod(uint16_t type, uint32_t groupId,
                               const Ep2PortMap& ep2port) {
    GroupEdit::Entry entry(new GroupEdit::GroupMod());
    entry->mod->command = type;
    entry->mod->group_id = groupId;

    for (const Ep2PortMap::value_type& kv : ep2port) {
        ofputil_bucket *bkt = createBucket(kv.second);
        ActionBuilder().output(kv.second).build(bkt);
        ovs_list_push_back(&entry->mod->buckets, &bkt->list_node);
    }
    uint32_t tunPort = getTunnelPort();
    uint32_t uplinkPort = getUplinkPort();
    if (type != OFPGC11_DELETE && encapType != ENCAP_NONE) {
        ActionBuilder ab;
        if(localExternalFdSet.find(groupId) != localExternalFdSet.end()) {
            if(uplinkPort != OFPP_NONE) {
                ofputil_bucket *bkt = createBucket(uplinkPort);
                actionTunnelMetadata(ab, ENCAP_VLAN);
                ab.output(uplinkPort)
                        .build(bkt);
                ovs_list_push_back(&entry->mod->buckets, &bkt->list_node);
	    }
        } else if(tunPort != OFPP_NONE) {
            ofputil_bucket *bkt = createBucket(tunPort);
            actionTunnelMetadata(ab, encapType);
            ab.output(tunPort)
                .build(bkt);
            ovs_list_push_back(&entry->mod->buckets, &bkt->list_node);
        }
    }
    return entry;
}

void
IntFlowManager::
updateEndpointFloodGroup(const opflex::modb::URI& fgrpURI,
                         const Endpoint& endPoint,
                         uint32_t epPort,
                         optional<shared_ptr<FloodDomain> >& fd) {
    const std::string& epUUID = endPoint.getUUID();
    uint32_t fgrpId = getId(FloodDomain::CLASS_ID, fgrpURI);
    string fgrpStrId = "fd:" + fgrpURI.toString();
    FloodGroupMap::iterator fgrpItr = floodGroupMap.find(fgrpURI);
    if(endPoint.isExternal()) {
        localExternalFdSet.insert(fgrpId);
    }
    if (fgrpItr != floodGroupMap.end()) {
        Ep2PortMap& epMap = fgrpItr->second;
        Ep2PortMap::iterator epItr = epMap.find(epUUID);

        if (epItr == epMap.end()) {
            /* EP not attached to this flood-group, check and remove
             * if it was attached to a different one */
            removeEndpointFromFloodGroup(epUUID);
        }
        if (epItr == epMap.end() || epItr->second != epPort) {
            LOG(DEBUG) << "Adding " << epUUID << " to group " << fgrpId;
            epMap[epUUID] = epPort;
            GroupEdit::Entry e = createGroupMod(OFPGC11_MODIFY, fgrpId, epMap);
            switchManager.writeGroupMod(e);
        } else if(endPoint.isExternal()) {
            /* Uplink could've come up.
             * When uplink goes down, it is handled in the domain change*/
            GroupEdit::Entry e = createGroupMod(OFPGC11_MODIFY, fgrpId, epMap);
            switchManager.writeGroupMod(e);
	}
    } else {
        /* Remove EP attachment to old floodgroup, if any */
        removeEndpointFromFloodGroup(epUUID);
        floodGroupMap[fgrpURI][epUUID] = epPort;
        GroupEdit::Entry e =
            createGroupMod(OFPGC11_ADD, fgrpId, floodGroupMap[fgrpURI]);
        switchManager.writeGroupMod(e);
    }

    FlowEntryList fdOutput;
    {
        // Output table action to output to the flood group appropriate
        // for the source EPG.
        FlowBuilder().priority(10).reg(5, fgrpId)
            .metadata(flow::meta::out::FLOOD, flow::meta::out::MASK)
            .action()
            .group(fgrpId)
            .parent().build(fdOutput);
    }
    switchManager.writeFlow(fgrpStrId, OUT_TABLE_ID, fdOutput);
}

void IntFlowManager::removeEndpointFromFloodGroup(const std::string& epUUID) {
    for (FloodGroupMap::iterator itr = floodGroupMap.begin();
         itr != floodGroupMap.end();
         ++itr) {
        const URI& fgrpURI = itr->first;
        Ep2PortMap& epMap = itr->second;
        if (epMap.erase(epUUID) == 0) {
            continue;
        }
        uint32_t fgrpId = getId(FloodDomain::CLASS_ID, fgrpURI);
        uint16_t type = epMap.empty() ?
                OFPGC11_DELETE : OFPGC11_MODIFY;
        GroupEdit::Entry e0 =
                createGroupMod(type, fgrpId, epMap);
        if (epMap.empty()) {
            string fgrpStrId = "fd:" + fgrpURI.toString();
            switchManager.clearFlows(fgrpStrId, OUT_TABLE_ID);
            switchManager.clearFlows(fgrpStrId, BRIDGE_TABLE_ID);
            floodGroupMap.erase(fgrpURI);
        }
        switchManager.writeGroupMod(e0);
        break;
    }
}

void IntFlowManager::addContractRules(FlowEntryList& entryList,
                             const uint32_t pvnid,
                             const uint32_t cvnid,
                             bool allowBidirectional,
                             const PolicyManager::rule_list_t& rules) {
    for (const shared_ptr<PolicyRule>& pc : rules) {
        uint8_t dir = pc->getDirection();
        const shared_ptr<L24Classifier>& cls = pc->getL24Classifier();
        const opflex::modb::URI& ruleURI = cls.get()->getURI();
        uint64_t cookie = getId(L24Classifier::CLASS_ID, ruleURI);
        flowutils::ClassAction act = flowutils::CA_DENY;
        if (pc->getAllow())
            act = flowutils::CA_ALLOW;

        if (dir == DirectionEnumT::CONST_BIDIRECTIONAL &&
            !allowBidirectional) {
            dir = DirectionEnumT::CONST_IN;
        }
        if (dir == DirectionEnumT::CONST_IN ||
            dir == DirectionEnumT::CONST_BIDIRECTIONAL) {
            flowutils::add_classifier_entries(*cls, act,
                                              boost::none,
                                              boost::none,
                                              IntFlowManager::STATS_TABLE_ID,
                                              pc->getPriority(),
                                              OFPUTIL_FF_SEND_FLOW_REM,
                                              cookie,
                                              cvnid, pvnid,
                                              entryList);
        }
        if (dir == DirectionEnumT::CONST_OUT ||
            dir == DirectionEnumT::CONST_BIDIRECTIONAL) {
            flowutils::add_classifier_entries(*cls, act,
                                              boost::none,
                                              boost::none,
                                              IntFlowManager::STATS_TABLE_ID,
                                              pc->getPriority(),
                                              OFPUTIL_FF_SEND_FLOW_REM,
                                              cookie,
                                              pvnid, cvnid,
                                              entryList);
        }
    }
}

void
IntFlowManager::handleContractUpdate(const opflex::modb::URI& contractURI) {
    LOG(DEBUG) << "Updating contract " << contractURI;

    const string& contractId = contractURI.toString();
    PolicyManager& polMgr = agent.getPolicyManager();
    if (!polMgr.contractExists(contractURI)) {  // Contract removed
        switchManager.clearFlows(contractId, POL_TABLE_ID);
        return;
    }
    PolicyManager::uri_set_t provURIs;
    PolicyManager::uri_set_t consURIs;
    PolicyManager::uri_set_t intraURIs;
    polMgr.getContractProviders(contractURI, provURIs);
    polMgr.getContractConsumers(contractURI, consURIs);
    polMgr.getContractIntra(contractURI, intraURIs);

    typedef unordered_set<uint32_t> id_set_t;
    id_set_t provIds;
    id_set_t consIds;
    id_set_t intraIds;
    getGroupVnid(provURIs, provIds);
    getGroupVnid(consURIs, consIds);
    getGroupVnid(intraURIs, intraIds);

    PolicyManager::rule_list_t rules;
    polMgr.getContractRules(contractURI, rules);

    LOG(DEBUG) << "Update for contract " << contractURI
               << ", #prov=" << provIds.size()
               << ", #cons=" << consIds.size()
               << ", #intra=" << intraIds.size()
               << ", #rules=" << rules.size();

    FlowEntryList entryList;

    for (const uint32_t& pvnid : provIds) {
        for (const uint32_t& cvnid : consIds) {
            if (pvnid == cvnid)
                continue;

            /*
             * Collapse bidirectional rules - if consumer 'cvnid' is
             * also a provider and provider 'pvnid' is also a
             * consumer, then add entry for cvnid to pvnid traffic
             * only.
             */
            bool allowBidirectional =
                provIds.find(cvnid) == provIds.end() ||
                consIds.find(pvnid) == consIds.end();

            addContractRules(entryList, pvnid, cvnid,
                             allowBidirectional,
                             rules);
        }
    }
    for (const uint32_t& ivnid : intraIds) {
        addContractRules(entryList, ivnid, ivnid, false, rules);
    }

    switchManager.writeFlow(contractId, POL_TABLE_ID, entryList);
}

void IntFlowManager::initPlatformConfig() {

    using namespace modelgbp::platform;

    optional<shared_ptr<Config> > config =
        Config::resolve(agent.getFramework(),
                        agent.getPolicyManager().getOpflexDomain());
    mcastTunDst = boost::none;
    if (config) {
        optional<const string&> ipStr =
            config.get()->getMulticastGroupIP();
        if (ipStr) {
            boost::system::error_code ec;
            address ip(address::from_string(ipStr.get(), ec));
            if (ec) {
                LOG(ERROR) << "Invalid multicast tunnel destination: "
                           << ipStr.get() << ": " << ec.message();
            } else if (!ip.is_v4()) {
                LOG(ERROR) << "Multicast tunnel destination must be IPv4: "
                           << ipStr.get();
            } else {
                mcastTunDst = ip;
            }
        }
        updateMulticastList(
            ipStr ? optional<string>(ipStr.get()) : optional<string>(),
            config.get()->getURI());
    }
}

void IntFlowManager::handleConfigUpdate(const opflex::modb::URI& configURI) {
    LOG(DEBUG) << "Updating platform config " << configURI;
    initPlatformConfig();

    // Directly update the group-table
    updateGroupTable();

    // update any flows that might have been affected by platform
    // config update
    createStaticFlows();

    PolicyManager::uri_set_t epgURIs;
    agent.getPolicyManager().getGroups(epgURIs);
    for (const URI& epg : epgURIs) {
        egDomainUpdated(epg);
    }
}

void IntFlowManager::updateGroupTable() {
    for (FloodGroupMap::value_type& kv : floodGroupMap) {
        const URI& fgrpURI = kv.first;
        uint32_t fgrpId = getId(FloodDomain::CLASS_ID, fgrpURI);
        Ep2PortMap& epMap = kv.second;

        GroupEdit::Entry e1 = createGroupMod(OFPGC11_MODIFY, fgrpId, epMap);
        switchManager.writeGroupMod(e1);
    }
}

void IntFlowManager::handleDropLogPortUpdate() {
    if(dropLogIface.empty() || !dropLogDst.is_v4()) {
        switchManager.clearFlows("DropLogStatic", EXP_DROP_TABLE_ID);
        LOG(WARNING) << "Ignoring dropLog port " << dropLogIface
        << " " << dropLogDst;
        return;
    }
    FlowEntryList catchDropFlows;
    int dropLogPort = switchManager.getPortMapper().FindPort(dropLogIface);
    if(dropLogPort != OFPP_NONE) {
        FlowBuilder().priority(0)
                .metadata(flow::meta::DROP_LOG, flow::meta::DROP_LOG)
                .action().reg(MFF_TUN_DST, dropLogDst.to_v4().to_ulong())
                .output(dropLogPort)
                .parent().build(catchDropFlows);
        switchManager.writeFlow("DropLogStatic", EXP_DROP_TABLE_ID, catchDropFlows);
    }
}

void IntFlowManager::handlePortStatusUpdate(const string& portName,
                                            uint32_t) {
    LOG(DEBUG) << "Port-status update for " << portName;
    if (portName == encapIface) {
        initPlatformConfig();
        createStaticFlows();

        PolicyManager::uri_set_t epgURIs;
        agent.getPolicyManager().getGroups(epgURIs);
        for (const URI& epg : epgURIs) {
            egDomainUpdated(epg);
        }
        PolicyManager::uri_set_t rdURIs;
        agent.getPolicyManager().getRoutingDomains(rdURIs);
        for (const URI& rd : rdURIs) {
            rdConfigUpdated(rd);
        }
        /* Directly update the group-table */
        updateGroupTable();
    } else if(portName == dropLogIface) {
        handleDropLogPortUpdate();
    } else if(portName == uplinkIface) {
        createStaticFlows();
        std::unordered_set<opflex::modb::URI> domains;
        agent.getEndpointManager().getLocalExternalDomains(domains);
        for(const URI &domain:domains) {
            handleLocalExternalDomainUpdated(domain);
        }
    } else {
        {
            unordered_set<string> uuids;
            agent.getEndpointManager().getEndpointsByIface(portName, uuids);
            agent.getEndpointManager().getEndpointsByIpmNextHopIf(portName,
                                                                  uuids);
            for (const string& uuid : uuids) {
                endpointUpdated(uuid);
            }
        }
        {
            unordered_set<string> uuids;
            agent.getServiceManager()
                .getServicesByIface(portName, uuids);
            for (const string& uuid : uuids) {
                serviceUpdated(uuid);
            }
        }
        {
            unordered_set<string> uuids;
            agent.getLearningBridgeManager()
                .getLBIfaceByIface(portName, uuids);
            for (const string& uuid : uuids) {
                lbIfaceUpdated(uuid);

                std::set<LearningBridgeManager::vlan_range_t> ranges;
                agent.getLearningBridgeManager()
                    .getVlanRangesByIface(uuid, ranges);
                for (auto& r : ranges) {
                    lbVlanUpdated(r);
                }
            }
        }
        {
            SnatManager::snats_t snats;
            agent.getSnatManager()
                .getSnatsByIface(portName, snats);
            for (const string& snatUuid : snats) {
                snatUpdated(snatUuid);
            }
        }
    }
}

void IntFlowManager::getGroupVnid(const unordered_set<URI>& uris,
    /* out */unordered_set<uint32_t>& ids) {
    PolicyManager& pm = agent.getPolicyManager();
    for (const URI& u : uris) {
        optional<uint32_t> vnid = pm.getVnidForGroup(u);
        optional<shared_ptr<RoutingDomain> > rd;
        if (vnid) {
            rd = pm.getRDForGroup(u);
        } else {
            rd = pm.getRDForL3ExtNet(u);
            if (rd) {
                vnid = getExtNetVnid(u);
            }
        }
        if (vnid && rd) {
            ids.insert(vnid.get());
        }
    }
}

typedef std::function<bool(opflex::ofcore::OFFramework&,
                           const string&,
                           const string&)> IdCb;

static const IdCb ID_NAMESPACE_CB[] =
    {IdGenerator::uriIdGarbageCb<FloodDomain>,
     IdGenerator::uriIdGarbageCb<BridgeDomain>,
     IdGenerator::uriIdGarbageCb<RoutingDomain>,
     IdGenerator::uriIdGarbageCb<L3ExternalNetwork>,
     IdGenerator::uriIdGarbageCb<L24Classifier>};

static bool serviceIdGarbageCb(ServiceManager& serviceManager,
                               const std::string& nmspc,
                               const std::string& str) {
    return (bool)serviceManager.getService(str);
}

static bool svcStatsIdGarbageCb(EndpointManager& epManager,
                              ServiceManager& serviceManager,
                              opflex::ofcore::OFFramework& framework,
                              const std::string& nmspc,
                              const std::string& str) {
    // The idgen strings for epToSvc and svcToEp will have below format
    // eptosvc:ep-uuid:svc-uuid
    // svctoep:ep-uuid:svc-uuid

    // The idgen strings for anyToSvc and svcToAny will have below format
    // antosvc:svc-tgt:svc-uuid:nh-ip
    // svctoan:svc-tgt:svc-uuid:nh-ip

    const string& statType = str.substr(0,7);
    if ((statType == "eptosvc") || (statType == "svctoep")) {
        size_t pos1 = str.find(":");
        size_t pos2 = str.find(":", pos1+1);
        const string& epUuid = str.substr(pos1+1, pos2-pos1-1);
        const string& svcUuid = str.substr(pos2+1);
        return ((bool)serviceManager.getService(svcUuid)
                 && (bool)epManager.getEndpoint(epUuid));
    } else if ((statType == "antosvc") || (statType == "svctoan")) {
        size_t pos1 = str.find(":");
        size_t pos2 = str.find(":", pos1+1);
        size_t pos3 = str.find(":", pos2+1);
        const string& svcUuid = str.substr(pos2+1, pos3-pos2-1);
        const string& nhipStr = str.substr(pos3+1);

        // If service got deleted, cleanup all cookies of that service
        if (!serviceManager.getService(svcUuid))
            return false;

        // Check if the service target got deleted
        if (!SvcTargetCounter::resolve(framework, svcUuid, nhipStr))
            return false;

        // ensure the pod is still local
        const ip_ep_map_t& ip_ep_map = epManager.getIPLocalEpMap();
        if (ip_ep_map.find(nhipStr) != ip_ep_map.end())
            return true;
    }
    return false;
}

void IntFlowManager::cleanup() {
    for (size_t i = 0; i < sizeof(ID_NAMESPACE_CB)/sizeof(IdCb); i++) {
        agent.getAgentIOService()
            .dispatch([=]() {
                    auto gcb = [this, i](const std::string& ns,
                                         const std::string& str) -> bool {
                        return ID_NAMESPACE_CB[i](agent.getFramework(),
                                                  ns, str);
                    };
                    idGen.collectGarbage(ID_NAMESPACES[i], gcb);
                });
    }

    agent.getAgentIOService()
        .dispatch([=]() {
                auto sgcb = [this](const std::string& ns,
                                   const std::string& str) -> bool {
                    return serviceIdGarbageCb(agent.getServiceManager(),
                                              ns, str);
                };
                idGen.collectGarbage(ID_NMSPC_SERVICE, sgcb);
            });

    agent.getAgentIOService()
        .dispatch([=]() {
                auto ssgcb = [this](const std::string& ns,
                                   const std::string& str) -> bool {
                    return svcStatsIdGarbageCb(agent.getEndpointManager(),
                                               agent.getServiceManager(),
                                               agent.getFramework(),
                                               ns, str);
                };
                idGen.collectGarbage(ID_NMSPC_SVCSTATS, ssgcb);
            });
}

const char * IntFlowManager::getIdNamespace(class_id_t cid) {
    const char *nmspc = NULL;
    switch (cid) {
    case RoutingDomain::CLASS_ID:   nmspc = ID_NMSPC_RD; break;
    case BridgeDomain::CLASS_ID:    nmspc = ID_NMSPC_BD; break;
    case FloodDomain::CLASS_ID:     nmspc = ID_NMSPC_FD; break;
    case L3ExternalNetwork::CLASS_ID: nmspc = ID_NMSPC_EXTNET; break;
    case L24Classifier::CLASS_ID: nmspc = ID_NMSPC_L24CLASS_RULE; break;
    case SvcCounter::CLASS_ID:
    case SvcTargetCounter::CLASS_ID:
    case SvcToEpCounter::CLASS_ID: // both ep2svc and svc2ep share same ns
    case EpToSvcCounter::CLASS_ID: nmspc = ID_NMSPC_SVCSTATS; break;
    default:
        assert(false);
    }
    return nmspc;
}


uint32_t IntFlowManager::getId(class_id_t cid, const URI& uri) {
    return idGen.getId(getIdNamespace(cid), uri.toString());
}

uint32_t IntFlowManager::getExtNetVnid(const opflex::modb::URI& uri) {
    // External networks are assigned private VNIDs that have bit 31 (MSB)
    // set to 1. This is fine because legal VNIDs are 24-bits or less.
    return (getId(L3ExternalNetwork::CLASS_ID, uri) | (1 << 31));
}

void IntFlowManager::updateMulticastList(const optional<string>& mcastIp,
                                      const URI& uri) {
    bool update = false;
    if ((encapType != ENCAP_VXLAN && encapType != ENCAP_IVXLAN) ||
        getTunnelPort() == OFPP_NONE ||
        !mcastIp) {
        update |= removeFromMulticastList(uri);

    } else {
        boost::system::error_code ec;
        address ip(address::from_string(mcastIp.get(), ec));
        if (ec || !ip.is_multicast()) {
            LOG(WARNING) << "Ignoring invalid/unsupported multicast "
                "subscription IP: " << mcastIp.get();
            return;
        }
        MulticastMap::iterator itr = mcastMap.find(mcastIp.get());
        if (itr != mcastMap.end()) {
            UriSet& uris = itr->second;
            UriSet::iterator jtr = uris.find(uri);
            if (jtr == uris.end()) {
                // remove old association, if any
                update |= removeFromMulticastList(uri);
                uris.insert(uri);
            }
        } else {
            // remove old association, if any
            update |= removeFromMulticastList(uri);
            mcastMap[mcastIp.get()].insert(uri);
            update |= !isSyncing;
        }
    }

    if (update)
        multicastGroupsUpdated();
}

bool IntFlowManager::removeFromMulticastList(const URI& uri) {
    for (MulticastMap::value_type& kv : mcastMap) {
        UriSet& uris = kv.second;
        if (uris.erase(uri) > 0 && uris.empty()) {
            mcastMap.erase(kv.first);
            return !isSyncing;
        }
    }
    return false;
}

static const std::string MCAST_QUEUE_ITEM("mcast-groups");

void IntFlowManager::multicastGroupsUpdated() {
    taskQueue.dispatch(MCAST_QUEUE_ITEM,
                       [this]() { writeMulticastGroups(); });
}

void IntFlowManager::writeMulticastGroups() {
    if (mcastGroupFile == "") return;

    pt::ptree tree;
    pt::ptree groups;
    for (MulticastMap::value_type& kv : mcastMap)
        groups.push_back(std::make_pair("", pt::ptree(kv.first)));
    tree.add_child("multicast-groups", groups);

    try {
        pt::write_json(mcastGroupFile, tree);
    } catch (pt::json_parser_error& e) {
        LOG(ERROR) << "Could not write multicast group file "
                   << e.what();
    }
}

void IntFlowManager::checkGroupEntry(GroupMap& recvGroups,
                                     uint32_t groupId,
                                     const Ep2PortMap& epMap,
                                     GroupEdit& ge) {
    GroupMap::iterator itr;
    itr = recvGroups.find(groupId);
    uint16_t comm = OFPGC11_ADD;
    GroupEdit::Entry recv;
    if (itr != recvGroups.end()) {
        comm = OFPGC11_MODIFY;
        recv = itr->second;
    }
    GroupEdit::Entry e0 = createGroupMod(comm, groupId, epMap);
    if (!GroupEdit::groupEq(e0, recv)) {
        ge.edits.push_back(e0);
    }
    if (itr != recvGroups.end()) {
        recvGroups.erase(itr);
    }
}

std::vector<FlowEdit>
IntFlowManager::reconcileFlows(std::vector<TableState> flowTables,
                               std::vector<FlowEntryList>& recvFlows) {
    // special handling for learning table; reconcile only the
    // reactive flows.
    FlowEntryList learnFlows;
    recvFlows[IntFlowManager::LEARN_TABLE_ID].swap(learnFlows);

    for (const FlowEntryPtr& fe : learnFlows) {
        if (fe->entry->cookie == 0) {
            recvFlows[IntFlowManager::LEARN_TABLE_ID].push_back(fe);
        }
    }

    return SwitchStateHandler::reconcileFlows(flowTables, recvFlows);
}

GroupEdit IntFlowManager::reconcileGroups(GroupMap& recvGroups) {
    GroupEdit ge;
    for (FloodGroupMap::value_type& kv : floodGroupMap) {
        const URI& fgrpURI = kv.first;
        Ep2PortMap& epMap = kv.second;

        uint32_t fgrpId = getId(FloodDomain::CLASS_ID, fgrpURI);
        checkGroupEntry(recvGroups, fgrpId, epMap, ge);
    }
    Ep2PortMap tmp;
    for (const GroupMap::value_type& kv : recvGroups) {
        GroupEdit::Entry e0 = createGroupMod(OFPGC11_DELETE, kv.first, tmp);
        ge.edits.push_back(e0);
    }
    return ge;
}

void IntFlowManager::completeSync() {
    writeMulticastGroups();
    advertManager.start();
}

} // namespace opflexagent
