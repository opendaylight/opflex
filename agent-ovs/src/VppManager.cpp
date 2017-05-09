/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <boost/system/error_code.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/asio/placeholders.hpp>

#include <netinet/icmp6.h>

#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/gbp/IntraGroupPolicyEnumT.hpp>
#include <modelgbp/gbp/UnknownFloodModeEnumT.hpp>
#include <modelgbp/gbp/BcastFloodModeEnumT.hpp>
#include <modelgbp/gbp/AddressResModeEnumT.hpp>
#include <modelgbp/gbp/RoutingModeEnumT.hpp>
#include <modelgbp/gbp/ConnTrackEnumT.hpp>

#include "logging.h"
#include "Endpoint.h"
#include "EndpointManager.h"
#include "EndpointListener.h"
#include "VppManager.h"
#include "VppApi.h"
#include "Packets.h"

#include "arp.h"
#include "eth.h"


using std::string;
using std::vector;
using std::ostringstream;
using std::shared_ptr;
using std::unordered_set;
using std::unordered_map;
using std::bind;
using boost::algorithm::trim;
using boost::ref;
using boost::optional;
using boost::asio::deadline_timer;
using boost::asio::ip::address;
using boost::asio::ip::address_v6;
using boost::asio::placeholders::error;
using std::chrono::milliseconds;
using std::unique_lock;
using std::mutex;
using opflex::modb::URI;
using opflex::modb::MAC;
using opflex::modb::class_id_t;

namespace pt = boost::property_tree;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;

namespace ovsagent {

    static const char* ID_NAMESPACES[] =
        {"floodDomain", "bridgeDomain", "routingDomain",
         "contract", "externalNetwork"};

    static const char* ID_NMSPC_FD      = ID_NAMESPACES[0];
    static const char* ID_NMSPC_BD      = ID_NAMESPACES[1];
    static const char* ID_NMSPC_RD      = ID_NAMESPACES[2];
    static const char* ID_NMSPC_CON     = ID_NAMESPACES[3];
    static const char* ID_NMSPC_EXTNET  = ID_NAMESPACES[4];

    VppManager::VppManager(Agent& agent_,
                           IdGenerator& idGen_) :
        agent(agent_),
        taskQueue(agent.getAgentIOService()),
        idGen(idGen_),
        floodScope(FLOOD_DOMAIN),
        virtualRouterEnabled(true),
        routerAdv(false),
        virtualDHCPEnabled(false),
        stopping(false),
        vppApi(std::unique_ptr<VppConnection>(new VppConnection())) {

        memset(routerMac, 0, sizeof(routerMac));
        memset(dhcpMac, 0, sizeof(dhcpMac));

        agent.getFramework().registerPeerStatusListener(this);

    }

    void VppManager::start(const std::string& name) {

        for (size_t i = 0; i < sizeof(ID_NAMESPACES)/sizeof(char*); i++) {
            idGen.initNamespace(ID_NAMESPACES[i]);
        }
        vppApi.connect(name);
        initPlatformConfig();
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

    }

    void VppManager::setFloodScope(FloodScope fscope) {
        floodScope = fscope;
    }

    void VppManager::setVirtualRouter(bool virtualRouterEnabled,
                                      bool routerAdv,
                                      const string& virtualRouterMac) {
    }

    void VppManager::setVirtualDHCP(bool dhcpEnabled,
                                    const string& mac) {
        this->virtualDHCPEnabled = dhcpEnabled;
        try {
            MAC(mac).toUIntArray(dhcpMac);
        } catch (std::invalid_argument) {
            LOG(ERROR) << "Invalid virtual DHCP server MAC: " << mac;
        }
    }


    void VppManager::setMulticastGroupFile(const std::string& mcastGroupFile) {
        this->mcastGroupFile = mcastGroupFile;
    }

    void VppManager::enableConnTrack() {
        conntrackEnabled = true;
    }

    void VppManager::endpointUpdated(const std::string& uuid) {
        if (stopping) return;

        taskQueue.dispatch(uuid,
                           bind(&VppManager::handleEndpointUpdate, this, uuid));
    }

    void VppManager::serviceUpdated(const std::string& uuid) {
        if (stopping) return;

        taskQueue.dispatch(uuid,
                           bind(&VppManager::handleAnycastServiceUpdate,
                                this, uuid));
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

    void VppManager::contractUpdated(const opflex::modb::URI& contractURI) {
        if (stopping) return;
        taskQueue.dispatch(contractURI.toString(),
                           bind(&VppManager::handleContractUpdate,
                                this, contractURI));
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
        if (stopping || isSyncing) return;
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
        return true;
    }

    void VppManager::handleEndpointUpdate(const string& uuid) {

        LOG(DEBUG) << "stub: Updating endpoint " << uuid;
    }

    void VppManager::handleAnycastServiceUpdate(const string& uuid) {
        LOG(DEBUG) << "Updating anycast service " << uuid;

    }

    void VppManager::updateEPGFlood(const URI& epgURI, uint32_t epgVnid,
                                    uint32_t fgrpId, address epgTunDst) {
        LOG(DEBUG) << "Updating EPGFlood " << fgrpId;
    }


    void VppManager::handleEndpointGroupDomainUpdate(const URI& epgURI) {
        LOG(DEBUG) << "Updating endpoint-group " << epgURI;
    }

    void VppManager::updateGroupSubnets(const URI& egURI, uint32_t bdId,
                                        uint32_t rdId) {
        LOG(DEBUG) << "Updating GroupSubnets bd: " << bdId << " rd: " << rdId;
    }

    void VppManager::handleRoutingDomainUpdate(const URI& rdURI) {
        optional<shared_ptr<RoutingDomain > > rd =
            RoutingDomain::resolve(agent.getFramework(), rdURI);

        if (!rd) {
            LOG(DEBUG) << "Cleaning up for RD: " << rdURI;
            return;
        }
        LOG(DEBUG) << "Updating routing domain " << rdURI;
    }

    void
    VppManager::handleDomainUpdate(class_id_t cid, const URI& domURI) {
        LOG(DEBUG) << "Updating domain " << domURI;
    }

    void
    VppManager::updateEndpointFloodGroup(const opflex::modb::URI& fgrpURI,
                                         const Endpoint& endPoint, uint32_t epPort,
                                         bool isPromiscuous,
                                         optional<shared_ptr<FloodDomain> >& fd) {
        LOG(DEBUG) << "Updating domain " << fgrpURI;
    }

    void VppManager::removeEndpointFromFloodGroup(const std::string& epUUID) {
        LOG(DEBUG) << "Removing EP from FD " << epUUID;
    }

    void
    VppManager::handleContractUpdate(const opflex::modb::URI& contractURI) {
        LOG(DEBUG) << "Updating contract " << contractURI;

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
    }

    void VppManager::handlePortStatusUpdate(const string& portName,
                                            uint32_t) {
        LOG(DEBUG) << "Port-status update for " << portName;
    }

    void VppManager::getGroupVnidAndRdId(const unordered_set<URI>& uris,
                                         /* out */unordered_map<uint32_t, uint32_t>& ids) {
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
                ids[vnid.get()] = getId(RoutingDomain::CLASS_ID,
                                        rd.get()->getURI());
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

    uint32_t VppManager::getExtNetVnid(const opflex::modb::URI& uri) {
        // External networks are assigned private VNIDs that have bit 31 (MSB)
        // set to 1. This is fine because legal VNIDs are 24-bits or less.
        return (getId(L3ExternalNetwork::CLASS_ID, uri) | (1 << 31));
    }

    void VppManager::updateMulticastList(const optional<string>& mcastIp,
                                         const URI& uri) {
    }

    bool VppManager::removeFromMulticastList(const URI& uri) {
        return true;
    }

    static const std::string MCAST_QUEUE_ITEM("mcast-groups");

    void VppManager::multicastGroupsUpdated() {
        taskQueue.dispatch(MCAST_QUEUE_ITEM,
                           bind(&VppManager::writeMulticastGroups, this));
    }

    void VppManager::writeMulticastGroups() {
        if (mcastGroupFile == "") return;

        pt::ptree tree;
        pt::ptree groups;
        for (MulticastMap::value_type& kv : mcastMap)
            groups.push_back(std::make_pair("", pt::ptree(kv.first)));
        tree.add_child("multicast-groups", groups);

        try {
            pt::write_json(mcastGroupFile, tree);
        } catch (pt::json_parser_error e) {
            LOG(ERROR) << "Could not write multicast group file "
                       << e.what();
        }
    }


} // namespace ovsagent
