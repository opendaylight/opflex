/*
 * Test suite for class IntFlowManager
 *
 * Copyright (c) 2014-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <sstream>
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/format.hpp>

#include <modelgbp/gbp/AddressResModeEnumT.hpp>
#include <modelgbp/gbp/BcastFloodModeEnumT.hpp>
#include <modelgbp/gbp/RoutingModeEnumT.hpp>
#include <modelgbp/platform/RemoteInventoryTypeEnumT.hpp>
#include <modelgbp/gbp/EnforcementPreferenceTypeEnumT.hpp>
#include <modelgbp/gbpe/SvcToEpCounter.hpp>
#include <modelgbp/gbpe/EpToSvcCounter.hpp>

#include <opflexagent/logging.h>
#include <opflexagent/LearningBridgeSource.h>
#include "IntFlowManager.h"
#include "FlowExecutor.h"
#include "Packets.h"

#include "MockSwitchManager.h"
#include "TableState.h"
#include "ActionBuilder.h"
#include "RangeMask.h"
#include "FlowConstants.h"
#include "FlowUtils.h"
#include "FlowManagerFixture.h"
#include "FlowBuilder.h"
#include "ovs-shim.h"

using namespace boost::assign;
using namespace opflex::modb;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using modelgbp::observer::SvcStatUniverse;
using namespace modelgbp::platform;
using namespace opflexagent;
namespace fs = boost::filesystem;
namespace pt = boost::property_tree;
using std::vector;
using std::string;
using std::make_pair;
using std::shared_ptr;
using std::unordered_set;
using std::unordered_map;
using boost::asio::ip::address;
using boost::asio::io_service;
using boost::optional;

BOOST_AUTO_TEST_SUITE(IntFlowManager_test)

class BaseIntFlowManagerFixture : public FlowManagerFixture {
public:
    BaseIntFlowManagerFixture()
        : FlowManagerFixture(),
          intFlowManager(agent, switchManager, idGen,
                         ctZoneManager, pktInHandler,
                         tunnelEpManager),
          pktInHandler(agent, intFlowManager),
          policyMgr(agent.getPolicyManager()),
          ep2_port(11), ep4_port(22) {

        expTables.resize(IntFlowManager::NUM_FLOW_TABLES);

        tunIf = "br0_vxlan0";
        uplinkIf = "uplink";
        intFlowManager.setEncapIface(tunIf);
        intFlowManager.setUplinkIface(uplinkIf);
        intFlowManager.setTunnel("10.11.12.13", 4789);
        intFlowManager.setVirtualRouter(true, true, "aa:bb:cc:dd:ee:ff");
        intFlowManager.setVirtualDHCP(true, "aa:bb:cc:dd:ee:ff");
        portmapper.setPort(uplinkIf, 1024);
        portmapper.setPort(tunIf, 2048);
        portmapper.setPort(2048, tunIf);
        portmapper.setPort(1024, uplinkIf);
        tun_port_new = 4096;

        switchManager.registerStateHandler(&intFlowManager);
        intFlowManager.enableConnTrack();

        pktInHandler.registerConnection(switchManager.getConnection(), NULL);
        pktInHandler.setPortMapper(&switchManager.getPortMapper(), NULL);
        pktInHandler.setFlowReader(&switchManager.getFlowReader());

    }
    virtual ~BaseIntFlowManagerFixture() {
        intFlowManager.stop();
        stop();
    }

    void createGroupEntries(IntFlowManager::EncapType encapType);
    void createOnConnectEntries(IntFlowManager::EncapType encapType,
                                FlowEntryList& flows,
                                GroupEdit::EntryList& groups);

    /**
     * Initialize flow tables with static flow entries not scoped to any
     * object
     */
    void initExpStatic(uint8_t remoteInventoryType =
                       RemoteInventoryTypeEnumT::CONST_NONE);

    /** Initialize EPG-scoped flow entries */
    void initExpEpg(shared_ptr<EpGroup>& epg,
                    uint32_t fdId = 0, uint32_t bdId = 1, uint32_t rdId = 1,
                    bool isolated = false,
                    uint8_t remoteInventoryType =
                    RemoteInventoryTypeEnumT::CONST_NONE);

    /** Initialize flood domain-scoped flow entries */
    void initExpFd(uint32_t fdId = 0);

    /** Initialize bridge domain-scoped flow entries */
    void initExpBd(uint32_t bdId = 1, uint32_t rdId = 1,
                   bool routeOn = true);

    /** Initialize routing domain-scoped flow entries */
    void initExpRd(uint32_t rdId = 1,
                   bool isUnenforced = false,
                   bool includeSubnets = true);

    /** Initialize endpoint-scoped flow entries */
    void initExpEp(shared_ptr<Endpoint>& ep,
                   shared_ptr<EpGroup>& epg,
                   uint32_t fdId = 0, uint32_t bdId = 1, uint32_t rdId = 1,
                   bool arpOn = true, bool routeOn = true);

    /** Initialize flows related to IP address mapping */
    void initExpIpMapping(bool natEpgMap = true, bool nextHop = false);

    /** Initialize remote endpoint flows */
    void initExpRemoteEp();

    /** Initialize contract 1 flows */
    void initExpCon1();

    /** Initialize contract 2 flows */
    void initExpCon2();

    /** Initialize contract 3 flows */
    void initExpCon3();

    /** Initialize subnet-scoped flow entries */
    void initSubnets(PolicyManager::subnet_vector_t& sns,
                     uint32_t bdId = 1, uint32_t rdId = 1);

    /** Initialize service-scoped flow entries for local services */
    void initExpAnycastService(Service &as, int nextHop = 0);

    /**
     * Initialize flows in stats table for pod to svc
     * and vice versa */
    void initExpPodServiceStats(const string& svc_ip,
                                shared_ptr<Endpoint>& ep,
                                Service &as,
                                const string& proto = "",
                                uint16_t svc_port = 0,
                                uint16_t nh_port = 0);

    /**
     * Initialize flows in stats table for * to svc
     * and vice versa */
    void initExpAnySvcStats(const string& nh_ip,
                            uint16_t nh_port,
                            const string& uuid,
                            uint64_t rxCookie,
                            uint64_t txCookie);

    /** Initialize service-scoped flow entries for load balanced
        services */
    void initExpLBService(Service &as1,
                          Service &as2,
                          bool conntrack = false,
                          bool remote = false);

    /** Initialize virtual IP flow entries */
    void initExpVirtualIp();

    /** Initialize DHCP flow entries */
    void initExpVirtualDhcp(bool virtIp, bool serverOverride);

    /** Initialize learning bridge flow entries */
    void initExpLearningBridge();

    /** Initialize ext SVI BD entries */
    void initExpSviBD(const URI& sviBDURI, uint32_t fdId, uint32_t bdId);

    /** Initialize ext SVI Ep entries */
    void initExpExtSviEp(shared_ptr<Endpoint>& ep,
            uint32_t fdId, uint32_t bdId);

    // Test drivers
    void epgTest();
    void routeModeTest();
    void arpModeTest();
    void fdTest();
    void groupFloodTest();
    void connectTest();
    void portStatusTest();
    void loadBalancedServiceTest();
    void remoteEndpointTest();

    IntFlowManager intFlowManager;
    PacketInHandler pktInHandler;
    PolicyManager& policyMgr;

    string tunIf,uplinkIf;

    vector<string> fe_connect_learn;
    string fe_connect_1, fe_connect_2;
    string ge_fd0, ge_bkt_ep0, ge_bkt_ep2, ge_bkt_tun, ge_bkt_up;
    string ge_fd1, ge_bkt_ep4, ge_bkt_ep5;
    string ge_bkt_tun_new;
    string ge_epg0, ge_epg2, ge_extsvi1;
    uint32_t ep2_port;
    uint32_t ep4_port;
    uint32_t tun_port_new;
};

class IntFlowManagerFixture : public BaseIntFlowManagerFixture {
public:
    IntFlowManagerFixture() : BaseIntFlowManagerFixture() {
        createObjects();

        portmapper.setPort(ep0->getInterfaceName().get(), 80);
        portmapper.setPort(80, ep0->getInterfaceName().get());

        WAIT_FOR(policyMgr.groupExists(epg0->getURI()), 500);
        WAIT_FOR(policyMgr.getBDForGroup(epg0->getURI()) != boost::none, 500);

        WAIT_FOR(policyMgr.groupExists(epg1->getURI()), 500);
        WAIT_FOR(policyMgr.getRDForGroup(epg1->getURI()) != boost::none, 500);

        WAIT_FOR(policyMgr.groupExists(epg3->getURI()), 500);
        WAIT_FOR(policyMgr.getRDForGroup(epg3->getURI()) != boost::none, 500);

        WAIT_FOR(policyMgr.groupExists(epg4->getURI()), 500);
        WAIT_FOR(policyMgr.getRDForGroup(epg4->getURI()) != boost::none, 500);

        start();
    }
};

class VxlanIntFlowManagerFixture : public IntFlowManagerFixture {
public:
    VxlanIntFlowManagerFixture() : IntFlowManagerFixture() {
        intFlowManager.setEncapType(IntFlowManager::ENCAP_VXLAN);
        intFlowManager.start();
        createGroupEntries(IntFlowManager::ENCAP_VXLAN);
    }

    virtual ~VxlanIntFlowManagerFixture() {}
};

class VlanIntFlowManagerFixture : public IntFlowManagerFixture {
public:
    VlanIntFlowManagerFixture() : IntFlowManagerFixture() {
        intFlowManager.setEncapType(IntFlowManager::ENCAP_VLAN);
        intFlowManager.start();
        createGroupEntries(IntFlowManager::ENCAP_VLAN);
    }

    virtual ~VlanIntFlowManagerFixture() {}
};

void BaseIntFlowManagerFixture::epgTest() {
    setConnected();

    /* create */
    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("create", 500);

    /* no EPG */
    ep0->setEgURI(opflex::modb::URI("/nothing"));
    epSrc.updateEndpoint(*ep0);
    intFlowManager.endpointUpdated(ep0->getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("remove epg", 500);

    /* restore EPG */
    ep0->setEgURI(epg0->getURI());
    epSrc.updateEndpoint(*ep0);
    intFlowManager.endpointUpdated(ep0->getUUID());

    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("restore epg", 500);

    /* forwarding object change */
    Mutator m1(framework, policyOwner);
    epg0->addGbpEpGroupToNetworkRSrc()
        ->setTargetFloodDomain(fd0->getURI());
    m1.commit();
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);
    PolicyManager::subnet_vector_t sns;
    WAIT_FOR_DO(sns.size() == 4, 500, sns.clear();
                policyMgr.getSubnetsForGroup(epg0->getURI(), sns));

    intFlowManager.egDomainUpdated(epg0->getURI());

    exec.Clear();
    exec.ExpectGroup(FlowEdit::ADD, ge_fd0 + ge_bkt_ep0 + ge_bkt_tun);
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0, 1);
    initExpFd(1);
    initExpBd(1, 1, true);
    initExpRd();
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg0, 1, 1, 1);
    initSubnets(sns);
    WAIT_FOR_TABLES("forwarding change", 500);

    /* Change flood domain to isolated */
    fd0->setBcastFloodMode(BcastFloodModeEnumT::CONST_ISOLATED);
    m1.commit();
    optional<shared_ptr<modelgbp::gbp::FloodDomain> > fdptr;
    WAIT_FOR((fdptr = policyMgr.getFDForGroup(epg2->getURI()))
             ? (fdptr.get()
                ->getBcastFloodMode(BcastFloodModeEnumT::CONST_NORMAL) ==
                BcastFloodModeEnumT::CONST_ISOLATED) : false, 500);
    intFlowManager.egDomainUpdated(epg0->getURI());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0, 1, 1, 1, true);
    initExpFd(1);
    initExpBd(1, 1, true);
    initExpRd();
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg0, 1, 1, 1);
    initSubnets(sns);
    WAIT_FOR_TABLES("isolated", 500);

    /* remove domain objects */
    PolicyManager::subnet_vector_t subnets, subnets_copy;
    policyMgr.getSubnetsForGroup(epg0->getURI(), subnets);
    BOOST_CHECK(!subnets.empty());
    subnets_copy = subnets;

    rd0->remove();
    for (shared_ptr<Subnet>& sn : subnets) {
        sn->remove();
    }
    m1.commit();
    WAIT_FOR(!policyMgr.getRDForGroup(epg0->getURI()), 500);
    WAIT_FOR_DO(subnets.empty(), 500,
        subnets.clear(); policyMgr.getSubnetsForGroup(epg0->getURI(), subnets));

    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());
    for (shared_ptr<Subnet>& sn : subnets_copy) {
        intFlowManager.domainUpdated(Subnet::CLASS_ID, sn->getURI());
    }

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0, 1, 1, 1, true);
    initExpFd(1);
    initExpBd(1, 1, true);
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg0, 1, 1, 1);
    WAIT_FOR_TABLES("remove domain", 500);

    /* remove */
    epSrc.removeEndpoint(ep0->getUUID());
    epSrc.removeEndpoint(ep2->getUUID());

    Mutator m2(framework, policyOwner);
    epg0->remove();
    bd0->remove();
    m2.commit();
    WAIT_FOR(policyMgr.groupExists(epg0->getURI()) == false, 500);

    exec.IgnoreGroupMods();

    intFlowManager.domainUpdated(BridgeDomain::CLASS_ID, bd0->getURI());
    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.endpointUpdated(ep0->getUUID());
    intFlowManager.endpointUpdated(ep2->getUUID());

    clearExpFlowTables();
    initExpStatic();
    WAIT_FOR_TABLES("remove ep", 500);
}

BOOST_FIXTURE_TEST_CASE(epg_vxlan, VxlanIntFlowManagerFixture) {
    epgTest();
}

BOOST_FIXTURE_TEST_CASE(epg_vlan, VlanIntFlowManagerFixture) {
    epgTest();
}

void BaseIntFlowManagerFixture::routeModeTest() {
    setConnected();
    intFlowManager.egDomainUpdated(epg0->getURI());
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("create", 500);

    /* Disable routing on bridge domain */
    {
        Mutator mutator(framework, policyOwner);
        bd0->setRoutingMode(RoutingModeEnumT::CONST_DISABLED);
        mutator.commit();
    }
    WAIT_FOR(policyMgr.getBDForGroup(epg0->getURI()).get()
             ->getRoutingMode(RoutingModeEnumT::CONST_ENABLED) ==
             RoutingModeEnumT::CONST_DISABLED, 500);
    intFlowManager.egDomainUpdated(epg0->getURI());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd(1, 1, false);
    initExpEp(ep0, epg0, 0, 1, 1, true, false);
    initExpEp(ep2, epg0, 0, 1, 1, true, false);
    WAIT_FOR_TABLES("disable", 500);

    /* Enable routing */
    {
        Mutator mutator(framework, policyOwner);
        bd0->setRoutingMode(RoutingModeEnumT::CONST_ENABLED);
        mutator.commit();
    }
    WAIT_FOR(policyMgr.getBDForGroup(epg0->getURI()).get()
             ->getRoutingMode(RoutingModeEnumT::CONST_ENABLED) ==
             RoutingModeEnumT::CONST_ENABLED, 500);
    intFlowManager.egDomainUpdated(epg0->getURI());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("enable", 500);

}

BOOST_FIXTURE_TEST_CASE(routemode_vxlan, VxlanIntFlowManagerFixture) {
    routeModeTest();
}

BOOST_FIXTURE_TEST_CASE(routemode_vlan, VlanIntFlowManagerFixture) {
    routeModeTest();
}

void BaseIntFlowManagerFixture::arpModeTest() {
    /* setup entries for epg0 connected to fd0 */
    setConnected();
    exec.IgnoreGroupMods();

    Mutator m1(framework, policyOwner);
    epg0->addGbpEpGroupToNetworkRSrc()
        ->setTargetFloodDomain(fd0->getURI());
    m1.commit();
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);
    PolicyManager::subnet_vector_t sns;
    WAIT_FOR_DO(sns.size() == 4, 500, sns.clear();
                policyMgr.getSubnetsForGroup(epg0->getURI(), sns));
    intFlowManager.egDomainUpdated(epg0->getURI());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0, 1);
    initExpFd(1);
    initExpBd(1, 1, true);
    initExpEp(ep0, epg0, 1, 1, 1, true);
    initExpEp(ep2, epg0, 1, 1, 1, true);
    initSubnets(sns);
    WAIT_FOR_TABLES("create", 500);

    /* change arp mode to flood */
    fd0->setArpMode(AddressResModeEnumT::CONST_FLOOD)
        .setNeighborDiscMode(AddressResModeEnumT::CONST_FLOOD);
    m1.commit();
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()).get()
             ->getArpMode(AddressResModeEnumT::CONST_UNICAST)
             == AddressResModeEnumT::CONST_FLOOD, 500);
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()).get()
             ->getNeighborDiscMode(AddressResModeEnumT::CONST_UNICAST)
             == AddressResModeEnumT::CONST_FLOOD, 500);
    intFlowManager.egDomainUpdated(epg0->getURI());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0, 1);
    initExpFd(1);
    initExpBd(1, 1, true);
    initExpEp(ep0, epg0, 1, 1, 1, false);
    initExpEp(ep2, epg0, 1, 1, 1, false);
    initSubnets(sns);
    WAIT_FOR_TABLES("disable", 500);

    /* set discovery proxy mode on for ep0 */
    ep0->setDiscoveryProxyMode(true);
    epSrc.updateEndpoint(*ep0);
    intFlowManager.endpointUpdated(ep0->getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0, 1);
    initExpFd(1);
    initExpBd(1, 1, true);
    initExpEp(ep0, epg0, 1, 1, 1, false);
    initExpEp(ep2, epg0, 1, 1, 1, false);
    initSubnets(sns);
    WAIT_FOR_TABLES("discoveryproxy", 500);
}

BOOST_FIXTURE_TEST_CASE(arpmode_vxlan, VxlanIntFlowManagerFixture) {
    arpModeTest();
}

BOOST_FIXTURE_TEST_CASE(arpmode_vlan, VlanIntFlowManagerFixture) {
    arpModeTest();
}

BOOST_FIXTURE_TEST_CASE(localEp, VxlanIntFlowManagerFixture) {
    setConnected();

    /* created */
    intFlowManager.endpointUpdated(ep0->getUUID());

    initExpStatic();
    initExpEp(ep0, epg0);
    WAIT_FOR_TABLES("create", 500);

    /* endpoint group change */
    ep0->setEgURI(epg1->getURI());
    epSrc.updateEndpoint(*ep0);
    intFlowManager.endpointUpdated(ep0->getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEp(ep0, epg1, 0, 2);
    WAIT_FOR_TABLES("change epg", 500);

    /* endpoint group changes back to old one */
    ep0->setEgURI(epg0->getURI());
    epSrc.updateEndpoint(*ep0);
    intFlowManager.endpointUpdated(ep0->getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEp(ep0, epg0);
    WAIT_FOR_TABLES("change epg back", 500);

    /* port-mapping change */
    portmapper.setPort(ep0->getInterfaceName().get(), 180);
    intFlowManager.portStatusUpdate(ep0->getInterfaceName().get(),
                                 180, false);

    clearExpFlowTables();
    initExpStatic();
    initExpEp(ep0, epg0);
    WAIT_FOR_TABLES("change port", 500);

    /* remove endpoint */
    epSrc.removeEndpoint(ep0->getUUID());
    intFlowManager.endpointUpdated(ep0->getUUID());

    clearExpFlowTables();
    initExpStatic();
    WAIT_FOR_TABLES("remove", 500);
}

BOOST_FIXTURE_TEST_CASE(noifaceEp, VxlanIntFlowManagerFixture) {
    setConnected();

    /* create */
    intFlowManager.endpointUpdated(ep2->getUUID());

    initExpStatic();
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("create", 500);

    /* endpoint group change */
    ep2->setEgURI(epg1->getURI());
    epSrc.updateEndpoint(*ep2);
    intFlowManager.endpointUpdated(ep2->getUUID());

    initExpEp(ep2, epg1, 0, 2);
    WAIT_FOR_TABLES("change", 500);
}

void BaseIntFlowManagerFixture::fdTest() {
    setConnected();

    /* create */
    portmapper.setPort(ep2->getInterfaceName().get(), ep2_port);
    portmapper.setPort(ep4->getInterfaceName().get(), ep4_port);
    intFlowManager.endpointUpdated(ep0->getUUID());
    intFlowManager.endpointUpdated(ep2->getUUID());

    initExpStatic();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("create", 500);

    /* Set FD & update EP0 */
    {
        Mutator m1(framework, policyOwner);
        epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetFloodDomain(fd0->getURI());
        m1.commit();
    }
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);

    exec.Clear();
    exec.ExpectGroup(FlowEdit::ADD, ge_fd0 + ge_bkt_ep0 + ge_bkt_tun);

    intFlowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpStatic();
    initExpFd(1);
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("ep0", 500);

    /* update EP2 */
    exec.Clear();
    exec.ExpectGroup(FlowEdit::MOD, ge_fd0 + ge_bkt_ep0 + ge_bkt_ep2
            + ge_bkt_tun);
    intFlowManager.endpointUpdated(ep2->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpStatic();
    initExpFd(1);
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg0, 1, 1, 1);
    WAIT_FOR_TABLES("ep2", 500);

    /* remove port-mapping for ep2 */
    portmapper.erasePort(ep2->getInterfaceName().get());
    exec.Clear();
    exec.ExpectGroup(FlowEdit::MOD, ge_fd0 + ge_bkt_ep0 + ge_bkt_tun);
    intFlowManager.endpointUpdated(ep2->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpStatic();
    initExpFd(1);
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg0, 1, 1, 1);
    WAIT_FOR_TABLES("removeport", 500);

    /* remove ep0 */
    epSrc.removeEndpoint(ep0->getUUID());
    exec.Clear();
    exec.ExpectGroup(FlowEdit::DEL, ge_fd0);
    intFlowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpStatic();
    WAIT_FOR_TABLES("removeep", 500);

    /* check promiscous flood */
    WAIT_FOR(policyMgr.getFDForGroup(epg3->getURI()) != boost::none, 500);
    exec.Clear();
    exec.ExpectGroup(FlowEdit::ADD, ge_fd1 + ge_bkt_ep4 + ge_bkt_tun);
    intFlowManager.endpointUpdated(ep4->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    /* group changes on tunnel port change */
    exec.Clear();
    exec.ExpectGroup(FlowEdit::MOD, ge_fd1 + ge_bkt_ep4 + ge_bkt_tun_new);
    portmapper.setPort(tunIf, tun_port_new);
    intFlowManager.portStatusUpdate(tunIf, tun_port_new, false);
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    /* Clear second group */
    epSrc.removeEndpoint(ep4->getUUID());
    exec.Clear();
    exec.ExpectGroup(FlowEdit::DEL, ge_fd1);
    intFlowManager.endpointUpdated(ep4->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);
}

BOOST_FIXTURE_TEST_CASE(fd_vxlan, VxlanIntFlowManagerFixture) {
    fdTest();
}

BOOST_FIXTURE_TEST_CASE(fd_vlan, VlanIntFlowManagerFixture) {
    fdTest();
}

void BaseIntFlowManagerFixture::groupFloodTest() {
    intFlowManager.setFloodScope(IntFlowManager::ENDPOINT_GROUP);
    setConnected();

    /* "create" local endpoints ep0 and ep2 */
    portmapper.setPort(ep2->getInterfaceName().get(), ep2_port);
    intFlowManager.endpointUpdated(ep0->getUUID());
    intFlowManager.endpointUpdated(ep2->getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("create", 500);

    /* Move ep2 to epg2, add epg0 to fd0. Now fd0 has 2 epgs */
    ep2->setEgURI(epg2->getURI());
    epSrc.updateEndpoint(*ep2);
    Mutator m1(framework, policyOwner);
    epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetFloodDomain(fd0->getURI());
    m1.commit();
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);
    WAIT_FOR(policyMgr.getFDForGroup(epg2->getURI()) != boost::none, 500);

    exec.Clear();
    //exec.Expect(FlowEdit::MOD, fe_ep0_fd0_1);
    //exec.Expect(FlowEdit::ADD, fe_ep0_fd0_2);
    exec.ExpectGroup(FlowEdit::ADD, ge_epg0 + ge_bkt_ep0 + ge_bkt_tun);
    intFlowManager.endpointUpdated(ep0->getUUID());

    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpStatic();
    initExpFd(1);
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("ep0", 500);

    exec.Clear();
    exec.ExpectGroup(FlowEdit::ADD, ge_epg2 + ge_bkt_ep2 + ge_bkt_tun);
    intFlowManager.endpointUpdated(ep2->getUUID());

    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpStatic();
    initExpFd(1);
    initExpFd(2);
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg2, 2, 1, 1);
    WAIT_FOR_TABLES("ep2", 500);

    /* remove ep0 */
    epSrc.removeEndpoint(ep0->getUUID());
    exec.Clear();
    exec.ExpectGroup(FlowEdit::DEL, ge_epg0);
    intFlowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpStatic();
    initExpFd(2);
    initExpEp(ep2, epg2, 2, 1, 1);
    WAIT_FOR_TABLES("remove", 500);
}

BOOST_FIXTURE_TEST_CASE(group_flood_vxlan, VxlanIntFlowManagerFixture) {
    groupFloodTest();
}

BOOST_FIXTURE_TEST_CASE(group_flood_vlan, VlanIntFlowManagerFixture) {
    groupFloodTest();
}

BOOST_FIXTURE_TEST_CASE(policy, VxlanIntFlowManagerFixture) {
    setConnected();

    createPolicyObjects();

    PolicyManager::uri_set_t egs;
    WAIT_FOR_DO(egs.size() == 2, 1000, egs.clear();
                policyMgr.getContractProviders(con1->getURI(), egs));
    egs.clear();
    WAIT_FOR_DO(egs.size() == 2, 500, egs.clear();
                policyMgr.getContractConsumers(con1->getURI(), egs));
    egs.clear();
    WAIT_FOR_DO(egs.size() == 2, 500, egs.clear();
                policyMgr.getContractIntra(con2->getURI(), egs));

    /* add con2 */
    intFlowManager.contractUpdated(con2->getURI());
    initExpStatic();
    initExpCon2();
    WAIT_FOR_TABLES("con2", 500);

    /* add con1 */
    intFlowManager.contractUpdated(con1->getURI());
    initExpCon1();
    WAIT_FOR_TABLES("con1", 500);

    /* remove */
    Mutator m2(framework, policyOwner);
    con2->remove();
    m2.commit();
    PolicyManager::rule_list_t rules;
    policyMgr.getContractRules(con2->getURI(), rules);
    WAIT_FOR_DO(rules.empty(), 500,
        rules.clear(); policyMgr.getContractRules(con2->getURI(), rules));
    intFlowManager.contractUpdated(con2->getURI());

    clearExpFlowTables();
    initExpStatic();
    initExpCon1();
    WAIT_FOR_TABLES("remove", 500);
}

BOOST_FIXTURE_TEST_CASE(policy_portrange, VxlanIntFlowManagerFixture) {
    setConnected();

    createPolicyObjects();

    PolicyManager::uri_set_t egs;
    WAIT_FOR_DO(egs.size() == 1, 1000, egs.clear();
                policyMgr.getContractProviders(con3->getURI(), egs));
    egs.clear();
    WAIT_FOR_DO(egs.size() == 1, 500, egs.clear();
                policyMgr.getContractConsumers(con3->getURI(), egs));
    PolicyManager::rule_list_t rules;
    WAIT_FOR_DO(rules.size() == 3, 500, rules.clear();
                policyMgr.getContractRules(con3->getURI(), rules));

    /* add con3 */
    intFlowManager.contractUpdated(con3->getURI());
    initExpStatic();
    initExpCon3();
    WAIT_FOR_TABLES("con3", 500);
}

void BaseIntFlowManagerFixture::connectTest() {
    exec.ignoredFlowMods.insert(FlowEdit::ADD);
    exec.Expect(FlowEdit::DEL, fe_connect_1);
    exec.Expect(FlowEdit::MOD, fe_connect_2);
    exec.Expect(FlowEdit::DEL, fe_connect_learn);
    exec.ExpectGroup(FlowEdit::DEL, "group_id=10,type=all");
    intFlowManager.egDomainUpdated(epg4->getURI());
    setConnected();

    WAIT_FOR(exec.IsEmpty(), 500);

    initExpStatic();
    initExpEpg(epg4, 0, 0);
    WAIT_FOR_TABLES("connect", 500);
}

BOOST_FIXTURE_TEST_CASE(connect_vxlan, VxlanIntFlowManagerFixture) {
    createOnConnectEntries(IntFlowManager::ENCAP_VXLAN,
                           reader.flows, reader.groups);
    connectTest();
}

BOOST_FIXTURE_TEST_CASE(connect_vlan, VlanIntFlowManagerFixture) {
    createOnConnectEntries(IntFlowManager::ENCAP_VLAN,
                           reader.flows, reader.groups);
    connectTest();
}

void BaseIntFlowManagerFixture::portStatusTest() {
    setConnected();

    /* create entries for epg0, ep0 and ep2 with original tunnel port */
    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("create", 500);

    /* delete all groups except epg0, then update tunnel port */
    PolicyManager::uri_set_t epgURIs;
    WAIT_FOR_DO(epgURIs.size() == 5, 500,
                epgURIs.clear(); policyMgr.getGroups(epgURIs));

    epgURIs.erase(epg0->getURI());
    Mutator m2(framework, policyOwner);
    for (const URI& u : epgURIs) {
        EpGroup::resolve(agent.getFramework(), u).get()->remove();
    }
    m2.commit();
    epgURIs.clear();
    WAIT_FOR_DO(epgURIs.size() == 1, 500,
                epgURIs.clear(); policyMgr.getGroups(epgURIs));
    portmapper.setPort(tunIf, tun_port_new);
    for (const URI& u : epgURIs) {
        intFlowManager.egDomainUpdated(u);
    }
    intFlowManager.portStatusUpdate(tunIf, tun_port_new, false);

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("update", 500);

    /* remove mapping for tunnel port */
    portmapper.erasePort(tunIf);
    intFlowManager.portStatusUpdate(tunIf, tun_port_new, false);

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("remove", 500);
}

BOOST_FIXTURE_TEST_CASE(portstatus_vxlan, VxlanIntFlowManagerFixture) {
    portStatusTest();
}

BOOST_FIXTURE_TEST_CASE(portstatus_vlan, VlanIntFlowManagerFixture) {
    portStatusTest();
}

static unordered_set<string> readMcast(const std::string& filePath) {
    try {
        unordered_set<string> ips;
        pt::ptree properties;
        pt::read_json(filePath, properties);
        optional<pt::ptree&> groups =
            properties.get_child_optional("multicast-groups");
        if (groups) {
            for (const pt::ptree::value_type &v : groups.get())
                ips.insert(v.second.data());
        }
        return ips;
    } catch (pt::json_parser_error& e) {
        LOG(DEBUG) << "Could not parse: " << e.what();
        return unordered_set<string>();
    }
}

BOOST_FIXTURE_TEST_CASE(mcast, VxlanIntFlowManagerFixture) {
    exec.IgnoreGroupMods();

    TempGuard tg;
    fs::path temp = tg.temp_dir / "mcast.json";

    MockSwitchConnection conn;
    intFlowManager.setMulticastGroupFile(temp.string());

    string prmBr = " " + conn.getSwitchName() + " 4789 ";

    WAIT_FOR(config->isMulticastGroupIPSet(), 500);
    string mcast1 = config->getMulticastGroupIP().get();
    string mcast2 = "224.1.1.2";
    string mcast3 = "224.5.1.1";
    string mcast4 = "224.5.1.2";

    unordered_set<string> expected;
    expected.insert(mcast1);

    intFlowManager.configUpdated(config->getURI());
    setConnected();
#define CHECK_MCAST                                                     \
    WAIT_FOR_ONFAIL(readMcast(temp.string()) == expected, 500,          \
            { for (const std::string& ip : readMcast(temp.string()))    \
                    LOG(ERROR) << ip; })
    CHECK_MCAST;
    {
        Mutator mutator(framework, policyOwner);
        config->setMulticastGroupIP(mcast2);
        mutator.commit();
    }

    WAIT_FOR(policyMgr.getMulticastIPForGroup(epg2->getURI()), 500);
    intFlowManager.configUpdated(config->getURI());
    intFlowManager.egDomainUpdated(epg2->getURI());
    expected.clear();
    expected.insert(mcast2);
    expected.insert(mcast3);

    CHECK_MCAST;

    {
        Mutator mutator(framework, policyOwner);
        fd0ctx->setMulticastGroupIP(mcast4);
        mutator.commit();
    }
    WAIT_FOR_DO(fd0ctx->getMulticastGroupIP("") == mcast4, 500,
        fd0ctx = policyMgr.getFloodContextForGroup(epg2->getURI()).get());

    intFlowManager.configUpdated(config->getURI());
    intFlowManager.egDomainUpdated(epg2->getURI());
    expected.insert(mcast4);

    CHECK_MCAST;

    {
        Mutator mutator(framework, policyOwner);
        fd0ctx->unsetMulticastGroupIP();
        mutator.commit();
    }
    WAIT_FOR_DO(fd0ctx->isMulticastGroupIPSet() == false, 500,
        fd0ctx = policyMgr.getFloodContextForGroup(epg2->getURI()).get());
    intFlowManager.egDomainUpdated(epg2->getURI());

    expected.erase(mcast4);

    CHECK_MCAST;
#undef CHECK_MCAST
}

BOOST_FIXTURE_TEST_CASE(ipMapping, VxlanIntFlowManagerFixture) {
    setConnected();
    intFlowManager.egDomainUpdated(epg0->getURI());

    shared_ptr<modelgbp::policy::Space> common;
    shared_ptr<FloodDomain> fd_ext;
    shared_ptr<EpGroup> eg_nat;
    shared_ptr<RoutingDomain> rd_ext;
    shared_ptr<BridgeDomain> bd_ext;
    shared_ptr<Subnets> subnets_ext;
    shared_ptr<L3ExternalDomain> l3ext;
    shared_ptr<L3ExternalNetwork> l3ext_net;
    {
        Mutator mutator(framework, policyOwner);
        common = universe->addPolicySpace("common");
        bd_ext = common->addGbpBridgeDomain("bd_ext");
        rd_ext = common->addGbpRoutingDomain("rd_ext");
        fd_ext = common->addGbpFloodDomain("fd_ext");

        fd_ext->addGbpFloodDomainToNetworkRSrc()
            ->setTargetBridgeDomain(bd_ext->getURI());
        bd_ext->addGbpBridgeDomainToNetworkRSrc()
            ->setTargetRoutingDomain(rd_ext->getURI());

        subnets_ext = common->addGbpSubnets("subnets_ext");
        subnets_ext->addGbpSubnet("subnet_ext4")
            ->setAddress("5.5.5.0")
            .setPrefixLen(24);
        subnets_ext->addGbpSubnet("subnet_ext6")
            ->setAddress("fdf1:9f86:d1af:6cc9::")
            .setPrefixLen(64)
            .setVirtualRouterIp("fdf1:9f86:d1af:6cc9::1");
        bd_ext->addGbpForwardingBehavioralGroupToSubnetsRSrc()
            ->setTargetSubnets(subnets_ext->getURI());
        rd_ext->addGbpRoutingDomainToIntSubnetsRSrc(subnets_ext->
                                                    getURI().toString());

        eg_nat = common->addGbpEpGroup("nat-epg");
        eg_nat->addGbpeInstContext()->setEncapId(0x4242);
        eg_nat->addGbpEpGroupToNetworkRSrc()
            ->setTargetFloodDomain(fd_ext->getURI());

        l3ext = rd0->addGbpL3ExternalDomain("ext");
        l3ext_net = l3ext->addGbpL3ExternalNetwork("outside");
        l3ext_net->addGbpExternalSubnet("outside")
            ->setAddress("5.5.0.0")
            .setPrefixLen(8);
        l3ext_net->addGbpExternalSubnet("outside_v6")
            ->setAddress("fdf1::ffff")
            .setPrefixLen(16);
        mutator.commit();
    }

    Endpoint::IPAddressMapping ipm4("91c5b217-d244-432c-922d-533c6c036ab3");
    ipm4.setMappedIP("10.20.44.2");
    ipm4.setFloatingIP("5.5.5.5");
    ipm4.setEgURI(eg_nat->getURI());
    ep0->addIPAddressMapping(ipm4);
    Endpoint::IPAddressMapping ipm6("c54b8503-1f14-4529-a0b3-cf704e1dfeb7");
    ipm6.setMappedIP("2001:db8::2");
    ipm6.setFloatingIP("fdf1:9f86:d1af:6cc9::5");
    ipm6.setEgURI(eg_nat->getURI());
    ep0->addIPAddressMapping(ipm6);
    epSrc.updateEndpoint(*ep0);

    WAIT_FOR(policyMgr.getBDForGroup(eg_nat->getURI()) != boost::none, 500);
    WAIT_FOR(policyMgr.getFDForGroup(eg_nat->getURI()) != boost::none, 500);
    WAIT_FOR(policyMgr.getRDForGroup(eg_nat->getURI()) != boost::none, 500);
    PolicyManager::subnet_vector_t sns;
    WAIT_FOR_DO(sns.size() == 2, 500, sns.clear();
                policyMgr.getSubnetsForGroup(eg_nat->getURI(), sns));

    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());
    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.egDomainUpdated(eg_nat->getURI());
    intFlowManager.endpointUpdated(ep0->getUUID());
    intFlowManager.endpointUpdated(ep2->getUUID());

    initExpStatic();
    initExpEpg(epg0);
    initExpEpg(eg_nat, 1, 2, 2);
    initExpBd();
    initExpBd(2, 2);
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpIpMapping(false);
    initSubnets(sns, 2, 2);
    WAIT_FOR_TABLES("create", 500);

    {
        Mutator mutator(framework, policyOwner);
        l3ext_net->addGbpL3ExternalNetworkToNatEPGroupRSrc()
            ->setTargetEpGroup(eg_nat->getURI());
        mutator.commit();
    }
    WAIT_FOR(policyMgr.getVnidForGroup(eg_nat->getURI())
             .get_value_or(0) == 0x4242, 500);
    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpEpg(eg_nat, 1, 2, 2);
    initExpBd();
    initExpBd(2, 2);
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpIpMapping(true);
    initSubnets(sns, 2, 2);
    WAIT_FOR_TABLES("natmapping", 500);

    // Add next hop for mapping
    portmapper.setPort("nexthop", 42);
    portmapper.setPort(42, "nexthop");
    ep0->clearIPAddressMappings();
    ipm4.setNextHopIf("nexthop");
    ipm4.setNextHopMAC(MAC("42:00:42:42:42:42"));
    ep0->addIPAddressMapping(ipm4);
    ipm6.setNextHopIf("nexthop");
    ipm6.setNextHopMAC(MAC("42:00:42:42:42:42"));
    ep0->addIPAddressMapping(ipm6);
    epSrc.updateEndpoint(*ep0);

    unordered_set<string> eps;
    agent.getEndpointManager().getEndpointsByIpmNextHopIf("nexthop", eps);
    BOOST_CHECK_EQUAL(1, eps.size());
    intFlowManager.endpointUpdated(ep0->getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpEpg(eg_nat, 1, 2, 2);
    initExpBd();
    initExpBd(2, 2);
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpIpMapping(true, true);
    initSubnets(sns, 2, 2);
    WAIT_FOR_TABLES("nexthop", 500);
}

BOOST_FIXTURE_TEST_CASE(routingDomainUnenforcedMode, BaseIntFlowManagerFixture) {
    {
        Mutator mutator(framework, policyOwner);

        bd0 = space->addGbpBridgeDomain("bd0");
        rd0 = space->addGbpRoutingDomain("rd0");
        rd0->setEnforcementPreference(
             EnforcementPreferenceTypeEnumT::CONST_UNENFORCED);
        bd0->addGbpBridgeDomainToNetworkRSrc()
            ->setTargetRoutingDomain(rd0->getURI());

        epg0 = space->addGbpEpGroup("epg0");
        epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetBridgeDomain(bd0->getURI());
        epg0->addGbpeInstContext()->setEncapId(0xA0A);
        mutator.commit();
    }

    start();
    intFlowManager.setEncapType(IntFlowManager::ENCAP_VXLAN);
    intFlowManager.start();
    setConnected();

    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd(1, true, false);
    WAIT_FOR_TABLES("vrf-unenforced", 500);

    /* Note: VRF enforced wont have POL table flow entry, which is
     * covered as part of several other tests. Not adding a separate
     * one here */
}

BOOST_FIXTURE_TEST_CASE(remoteInventoryMode, BaseIntFlowManagerFixture) {
    {
        Mutator mutator(framework, policyOwner);
        config = universe->addPlatformConfig("default");
        config->setMulticastGroupIP("224.1.1.1");

        bd0 = space->addGbpBridgeDomain("bd0");
        rd0 = space->addGbpRoutingDomain("rd0");
        bd0->addGbpBridgeDomainToNetworkRSrc()
            ->setTargetRoutingDomain(rd0->getURI());

        epg0 = space->addGbpEpGroup("epg0");
        epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetBridgeDomain(bd0->getURI());
        epg0->addGbpeInstContext()->setEncapId(0xA0A);
        mutator.commit();
    }

    start();
    intFlowManager.setEncapType(IntFlowManager::ENCAP_VXLAN);
    intFlowManager.start();
    setConnected();

    /* create */
    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd(1, false, false);
    WAIT_FOR_TABLES("create", 500);

    /* on-link inventory */
    {
        Mutator mutator(framework, policyOwner);
        auto config = universe->resolvePlatformConfig("default");
        BOOST_REQUIRE(config);
        config.get()->
            setInventoryType(RemoteInventoryTypeEnumT::CONST_ON_LINK);
        mutator.commit();
        intFlowManager.configUpdated(config.get()->getURI());
    }

    clearExpFlowTables();
    initExpStatic(RemoteInventoryTypeEnumT::CONST_ON_LINK);
    initExpEpg(epg0, 0, 1, 1, false, RemoteInventoryTypeEnumT::CONST_ON_LINK);
    initExpBd();
    initExpRd(1, false, false);
    WAIT_FOR_TABLES("on_link", 500);

    /* complete inventory */
    {
        Mutator mutator(framework, policyOwner);
        auto config = universe->resolvePlatformConfig("default");
        BOOST_REQUIRE(config);
        config.get()->
            setInventoryType(RemoteInventoryTypeEnumT::CONST_COMPLETE);
        mutator.commit();
        intFlowManager.configUpdated(config.get()->getURI());
    }

    clearExpFlowTables();
    initExpStatic(RemoteInventoryTypeEnumT::CONST_COMPLETE);
    initExpEpg(epg0, 0, 1, 1, false, RemoteInventoryTypeEnumT::CONST_COMPLETE);
    initExpBd();
    initExpRd(1, false, false);
    WAIT_FOR_TABLES("complete", 500);

}

void BaseIntFlowManagerFixture::remoteEndpointTest() {
    setConnected();
    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    Mutator m(framework, policyOwner);
    auto invu = modelgbp::inv::Universe::resolve(framework);
    auto inv = invu.get()->addInvRemoteEndpointInventory();
    auto rep1 = inv->addInvRemoteInventoryEp("ep1");
    rep1->setMac(MAC("ab:cd:ef:ab:cd:ef"))
        .setNextHopTunnel("5.6.7.8")
        .addInvRemoteInventoryEpToGroupRSrc()
        ->setTargetEpGroup(epg0->getURI());
    rep1->addInvRemoteIp("1.3.5.7");
    rep1->addInvRemoteIp("2.4.6.8");
    m.commit();

    intFlowManager.remoteEndpointUpdated("ep1");

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpRemoteEp();
    WAIT_FOR_TABLES("remoteep", 500);
}

BOOST_FIXTURE_TEST_CASE(remoteEndpoint_vxlan, VxlanIntFlowManagerFixture) {
    remoteEndpointTest();
}

BOOST_FIXTURE_TEST_CASE(remoteEndpoint_vlan, VlanIntFlowManagerFixture) {
    remoteEndpointTest();
}

BOOST_FIXTURE_TEST_CASE(anycastService, VxlanIntFlowManagerFixture) {
    setConnected();
    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());
    portmapper.setPort("service-iface", 17);
    portmapper.setPort(17, "service-iface");

    Service as;
    as.setUUID("ed84daef-1696-4b98-8c80-6b22d85f4dc2");
    as.setServiceMAC(MAC("ed:84:da:ef:16:96"));
    as.setDomainURI(URI(rd0->getURI()));
    as.setInterfaceName("service-iface");

    Service::ServiceMapping sm1;
    sm1.setServiceIP("169.254.169.254");
    sm1.setGatewayIP("169.254.1.1");
    as.addServiceMapping(sm1);

    Service::ServiceMapping sm2;
    sm2.setServiceIP("fe80::a9:fe:a9:fe");
    sm2.setGatewayIP("fe80::1");
    as.addServiceMapping(sm2);

    servSrc.updateService(as);
    intFlowManager.serviceUpdated(as.getUUID());

    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpAnycastService(as);
    WAIT_FOR_TABLES("create", 500);

    as.clearServiceMappings();
    sm1.addNextHopIP("169.254.169.1");
    as.addServiceMapping(sm1);
    sm2.addNextHopIP("fe80::a9:fe:a9:1");
    as.addServiceMapping(sm2);

    servSrc.updateService(as);
    intFlowManager.serviceUpdated(as.getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpAnycastService(as,1);
    WAIT_FOR_TABLES("nexthop", 500);

    as.clearServiceMappings();
    sm1.addNextHopIP("169.254.169.2");
    as.addServiceMapping(sm1);
    sm2.addNextHopIP("fe80::a9:fe:a9:2");
    as.addServiceMapping(sm2);

    servSrc.updateService(as);
    intFlowManager.serviceUpdated(as.getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpAnycastService(as,2);
    WAIT_FOR_TABLES("nexthop lb", 500);

    // Test service deletion
    servSrc.removeService(as.getUUID());
    intFlowManager.serviceUpdated(as.getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("delete", 500);
}

BOOST_FIXTURE_TEST_CASE(loadBalancedService_vxlan, VxlanIntFlowManagerFixture) {
    loadBalancedServiceTest();
}

BOOST_FIXTURE_TEST_CASE(loadBalancedService_vlan, VlanIntFlowManagerFixture) {
    loadBalancedServiceTest();
}

void BaseIntFlowManagerFixture::loadBalancedServiceTest() {
    setConnected();
    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    Service as1;
    as1.setUUID("ed84daef-1696-4b98-8c80-6b22d85f4dc2");
    as1.setDomainURI(URI(rd0->getURI()));
    as1.setServiceMode(Service::LOADBALANCER);

    Service as2;
    as2.setUUID("262f1120-3e08-4529-affe-6164de3ff410");
    as2.setDomainURI(URI(rd0->getURI()));
    as2.setServiceMode(Service::LOADBALANCER);

    Service::ServiceMapping sm1;
    sm1.setServiceIP("169.254.169.254");
    sm1.setServiceProto("udp");
    sm1.addNextHopIP("10.20.44.2");
    sm1.addNextHopIP("169.254.169.2");
    sm1.setServicePort(53);
    sm1.setNextHopPort(5353);
    as1.addServiceMapping(sm1);

    Service::ServiceMapping sm2;
    sm2.setServiceIP("fe80::a9:fe:a9:fe");
    sm2.setServiceProto("tcp");
    sm2.addNextHopIP("2001:db8::2");
    sm2.addNextHopIP("fe80::a9:fe:a9:2");
    sm2.setServicePort(80);
    as1.addServiceMapping(sm2);

    as1.clearServiceMappings();
    sm1.addNextHopIP("169.254.169.2");
    as1.addServiceMapping(sm1);

    sm2.addNextHopIP("fe80::a9:fe:a9:2");
    as2.addServiceMapping(sm2);

    as1.addAttribute("name", "coredns");
    as1.addAttribute("scope", "cluster");
    as1.addAttribute("namespace", "default");

    as2.addAttribute("name", "redis-master");
    as2.addAttribute("scope", "cluster");
    as2.addAttribute("namespace", "kube-system");

    servSrc.updateService(as1);
    servSrc.updateService(as2);

    intFlowManager.serviceUpdated(as1.getUUID());
    intFlowManager.serviceUpdated(as2.getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpLBService(as1, as2);
    WAIT_FOR_TABLES("create", 500);

    as1.clearServiceMappings();
    as2.clearServiceMappings();
    sm1.setConntrackMode(true);
    as1.addServiceMapping(sm1);
    sm2.setConntrackMode(true);
    as2.addServiceMapping(sm2);

    servSrc.updateService(as1);
    servSrc.updateService(as2);
    intFlowManager.serviceUpdated(as1.getUUID());
    intFlowManager.serviceUpdated(as2.getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpLBService(as1, as2, true);
    WAIT_FOR_TABLES("conntrack", 500);

    portmapper.setPort("service-iface", 17);
    portmapper.setPort(17, "service-iface");
    as1.setServiceMAC(MAC("13:37:13:37:13:37"));
    as1.setInterfaceName("service-iface");
    as1.setIfaceIP("1.1.1.1");
    as1.setIfaceVlan(4003);
    as2.setServiceMAC(MAC("13:37:13:37:13:37"));
    as2.setInterfaceName("service-iface");
    as2.setIfaceIP("fd82:3372:749c:631b::42");
    as2.setIfaceVlan(4003);

    as1.clearServiceMappings();
    as2.clearServiceMappings();
    sm1.setConntrackMode(false);
    as1.addServiceMapping(sm1);
    sm2.setConntrackMode(false);
    as2.addServiceMapping(sm2);

    as1.clearAttributes();
    as1.addAttribute("name", "coredns");
    as1.addAttribute("scope", "ext");
    as1.addAttribute("namespace", "default");

    as2.clearAttributes();
    as2.addAttribute("name", "redis-master");
    as2.addAttribute("scope", "ext");
    as2.addAttribute("namespace", "kube-system");

    servSrc.updateService(as1);
    servSrc.updateService(as2);
    intFlowManager.serviceUpdated(as1.getUUID());
    intFlowManager.serviceUpdated(as2.getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpLBService(as1, as2, false, true);
    WAIT_FOR_TABLES("exposed", 500);

    as1.clearServiceMappings();
    as2.clearServiceMappings();
    sm1.setConntrackMode(true);
    as1.addServiceMapping(sm1);
    sm2.setConntrackMode(true);
    as2.addServiceMapping(sm2);

    servSrc.updateService(as1);
    servSrc.updateService(as2);
    intFlowManager.serviceUpdated(as1.getUUID());
    intFlowManager.serviceUpdated(as2.getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpLBService(as1, as2, true, true);
    WAIT_FOR_TABLES("exposed conntrack", 500);

    // Test service deletion
    servSrc.removeService(as1.getUUID());
    servSrc.removeService(as2.getUUID());
    intFlowManager.serviceUpdated(as1.getUUID());
    intFlowManager.serviceUpdated(as2.getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("delete", 500);
}

BOOST_FIXTURE_TEST_CASE(vip, VxlanIntFlowManagerFixture) {
    setConnected();
    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:42"), "42.42.42.42"));
    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:43"), "42.42.42.16/28"));
    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:42"), "42::42"));
    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:43"), "42::10/124"));
    ep0->addVirtualIP(make_pair(MAC("00:00:00:00:80:00"), "10.20.44.3"));
    epSrc.updateEndpoint(*ep0);
    intFlowManager.endpointUpdated(ep0->getUUID());

    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpVirtualIp();
    WAIT_FOR_TABLES("create", 500);
}

BOOST_FIXTURE_TEST_CASE(virtDhcp, VxlanIntFlowManagerFixture) {
    setConnected();
    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    Endpoint::DHCPv4Config v4;
    Endpoint::DHCPv6Config v6;

    ep0->setDHCPv4Config(v4);
    ep0->setDHCPv6Config(v6);
    epSrc.updateEndpoint(*ep0);
    intFlowManager.endpointUpdated(ep0->getUUID());

    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpVirtualDhcp(false, false);
    WAIT_FOR_TABLES("create", 500);

    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:42"), "42.42.42.42"));
    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:43"), "42.42.42.16/28"));
    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:42"), "42::42"));
    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:43"), "42::10/124"));
    ep0->addVirtualIP(make_pair(MAC("00:00:00:00:80:00"), "10.20.44.3"));

    v4.setServerIp("1.2.3.4");
    v4.setServerMac(MAC("ff:bb:ff:dd:ee:ff"));
    ep0->setDHCPv4Config(v4);

    epSrc.updateEndpoint(*ep0);
    intFlowManager.endpointUpdated(ep0->getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpVirtualIp();
    initExpVirtualDhcp(true, true);
    WAIT_FOR_TABLES("virtip", 500);
}

BOOST_FIXTURE_TEST_CASE(learningBridge, BaseIntFlowManagerFixture) {
    start();
    intFlowManager.setEncapType(IntFlowManager::ENCAP_VLAN);
    intFlowManager.start();
    setConnected();

    portmapper.setPort("eth0", 100);
    portmapper.setPort(100, "eth0");
    portmapper.setPort("eth1", 101);
    portmapper.setPort(101, "eth1");
    portmapper.setPort("eth2", 102);
    portmapper.setPort(102, "eth2");

    agent.getLearningBridgeManager().registerListener(&intFlowManager);

    LearningBridgeSource lbSource(&agent.getLearningBridgeManager());
    LearningBridgeIface if1;
    if1.setUUID("1");
    if1.setInterfaceName("eth0");
    if1.setTrunkVlans({ {0x400,0x4ff}, {4093,4093} });
    lbSource.updateLBIface(if1);

    LearningBridgeIface if2;
    if2.setUUID("2");
    if2.setInterfaceName("eth1");
    if2.setTrunkVlans({ {0x20,0x2f}, {4093,4093} });
    lbSource.updateLBIface(if2);

    LearningBridgeIface if3;
    if3.setUUID("3");
    if3.setInterfaceName("eth2");
    if3.setTrunkVlans({ {0x400,0x4ff}, {0x20,0x2f}, {4093,4093} });
    lbSource.updateLBIface(if3);

    clearExpFlowTables();
    initExpStatic();
    initExpLearningBridge();
    WAIT_FOR_TABLES("basic", 500);

    agent.getLearningBridgeManager().unregisterListener(&intFlowManager);
}

#define ADDF(flow) addExpFlowEntry(expTables, flow)
enum TABLE {
    DROPLOG, SEC, SRC, SNAT_REV, SVR, BR, SVH, RT, SNAT, NAT, LRN, SVD, POL, STAT, OUT, EXP_DROPLOG
};

void BaseIntFlowManagerFixture::initExpStatic(uint8_t remoteInventoryType) {
    uint32_t tunPort = intFlowManager.getTunnelPort();
    uint32_t uplink = intFlowManager.getUplinkPort();
    uint8_t rmacArr[6];
    memcpy(rmacArr, intFlowManager.getRouterMacAddr(), sizeof(rmacArr));
    string rmac = MAC(rmacArr).toString();
    ADDF(Bldr().table(DROPLOG).priority(0).actions().go(SEC).done());
    ADDF(Bldr().table(STAT).priority(10).actions().go(OUT).done());
    ADDF(Bldr().table(SEC)
            .cookie(ovs_ntohll(flow::cookie::TABLE_DROP_FLOW))
            .flags(OFPUTIL_FF_SEND_FLOW_REM)
            .priority(25).arp().actions()
            .dropLog(SEC).go(EXP_DROPLOG).done());
    ADDF(Bldr().table(SEC)
            .cookie(ovs_ntohll(flow::cookie::TABLE_DROP_FLOW))
            .flags(OFPUTIL_FF_SEND_FLOW_REM)
            .priority(25).ip().actions()
            .dropLog(SEC).go(EXP_DROPLOG).done());
    ADDF(Bldr().table(SEC)
            .cookie(ovs_ntohll(flow::cookie::TABLE_DROP_FLOW))
            .flags(OFPUTIL_FF_SEND_FLOW_REM)
            .priority(25).ipv6().actions()
            .dropLog(SEC).go(EXP_DROPLOG).done());
    ADDF(Bldr().table(SEC).priority(27).udp().isTpSrc(68).isTpDst(67)
         .actions().go(SRC).done());
    ADDF(Bldr().table(SEC).priority(27).udp6().isTpSrc(546).isTpDst(547)
         .actions().go(SRC).done());
    ADDF(Bldr().table(SEC).priority(27).icmp6().icmp_type(133).icmp_code(0)
         .actions().go(SRC).done());
    ADDF(Bldr().table(SVR).priority(1).actions().go(BR).done());
    ADDF(Bldr().table(POL).priority(8242).isPolicyApplied()
         .actions().drop().done());
    ADDF(Bldr().table(POL).priority(8243).isFromServiceIface()
         .actions().go(STAT).done());
    ADDF(Bldr().table(POL).priority(10).arp().actions().go(STAT).done());
    ADDF(Bldr().table(POL).priority(10).icmp6().icmp_type(135).icmp_code(0)
         .actions().go(STAT).done());
    ADDF(Bldr().table(POL).priority(10).icmp6().icmp_type(136).icmp_code(0)
         .actions().go(STAT).done());
    ADDF(Bldr().table(OUT).priority(1).isMdAct(0)
         .actions().out(OUTPORT).done());
    ADDF(Bldr().table(OUT).priority(1)
         .isMdAct(flow::meta::out::REV_NAT)
         .actions().out(OUTPORT).done());
    ADDF(Bldr().table(OUT).priority(10)
         .cookie(ovs_ntohll(flow::cookie::ICMP_ERROR_V4))
         .icmp().isMdAct(flow::meta::out::REV_NAT).icmp_type(3)
         .actions().controller(65535).done());
    ADDF(Bldr().table(OUT).priority(10)
         .cookie(ovs_ntohll(flow::cookie::ICMP_ERROR_V4))
         .icmp().isMdAct(flow::meta::out::REV_NAT).icmp_type(11)
         .actions().controller(65535).done());
    ADDF(Bldr().table(OUT).priority(10)
         .cookie(ovs_ntohll(flow::cookie::ICMP_ERROR_V4))
         .icmp().isMdAct(flow::meta::out::REV_NAT).icmp_type(12)
         .actions().controller(65535).done());

    if (tunPort != OFPP_NONE) {
        ADDF(Bldr().table(SEC).priority(50).in(tunPort)
             .actions().go(SRC).done());
        if (remoteInventoryType != RemoteInventoryTypeEnumT::CONST_COMPLETE) {
            ADDF(Bldr().table(BR).priority(1)
                 .actions().mdAct(flow::meta::out::TUNNEL)
                 .go(STAT).done());
            ADDF(Bldr().table(RT).priority(1)
                 .actions().mdAct(flow::meta::out::TUNNEL)
                 .go(STAT).done());
        }
        if (intFlowManager.getEncapType() != IntFlowManager::ENCAP_VLAN) {
            ADDF(Bldr().table(OUT).priority(15)
                .isMdAct(flow::meta::out::REMOTE_TUNNEL)
                .actions()
                .move(DEPG, TUNID).move(OUTPORT, TUNDST)
                .outPort(tunPort)
                .done());
        }
    }

    for(int i=SEC; i<=OUT; i++) {
        ADDF(Bldr().table(i)
        .cookie(ovs_ntohll(flow::cookie::TABLE_DROP_FLOW))
        .flags(OFPUTIL_FF_SEND_FLOW_REM).priority(0)
        .actions().dropLog(i).go(EXP_DROPLOG).done());
    }

    if (uplink != OFPP_NONE) {
        ADDF(Bldr().table(SEC).priority(50).in(uplink)
            .actions().go(SRC).done());
    }
}

// Initialize EPG-scoped flow entries
void BaseIntFlowManagerFixture::initExpEpg(shared_ptr<EpGroup>& epg,
                                           uint32_t fdId, uint32_t bdId,
                                           uint32_t rdId, bool isolated,
                                           uint8_t remoteInventoryType) {
    IntFlowManager::EncapType encapType = intFlowManager.getEncapType();
    uint32_t tunPort = intFlowManager.getTunnelPort();
    WAIT_FOR(policyMgr.getVnidForGroup(epg->getURI()), 500);
    uint32_t vnid = policyMgr.getVnidForGroup(epg->getURI()).get();
    address mcast = intFlowManager.getEPGTunnelDst(epg->getURI());
    uint8_t rmacArr[6];
    memcpy(rmacArr, intFlowManager.getRouterMacAddr(), sizeof(rmacArr));
    string rmac = MAC(rmacArr).toString();
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    switch (encapType) {
    case IntFlowManager::ENCAP_VLAN:
        if (isolated) {
            ADDF(Bldr().table(BR).priority(10)
                 .reg(SEPG, vnid).reg(FD, fdId)
                 .isPolicyApplied().isEthDst(mmac).actions()
                 .mdAct(flow::meta::out::FLOOD)
                 .go(STAT).done());
        } else {
            ADDF(Bldr().table(BR).priority(10)
                 .reg(SEPG, vnid).reg(FD, fdId).isEthDst(mmac).actions()
                 .mdAct(flow::meta::out::FLOOD)
                 .go(STAT).done());
        }
        break;
    case IntFlowManager::ENCAP_VXLAN:
    case IntFlowManager::ENCAP_IVXLAN:
    default:
        if (isolated) {
            ADDF(Bldr().table(BR).priority(10)
                 .reg(SEPG, vnid).reg(FD, fdId)
                 .isPolicyApplied().isEthDst(mmac).actions()
                 .load(OUTPORT, mcast.to_v4().to_ulong())
                 .mdAct(flow::meta::out::FLOOD)
                 .go(STAT).done());
        } else {
            ADDF(Bldr().table(BR).priority(10)
                 .reg(SEPG, vnid).reg(FD, fdId).isEthDst(mmac).actions()
                 .load(OUTPORT, mcast.to_v4().to_ulong())
                 .mdAct(flow::meta::out::FLOOD)
                 .go(STAT).done());
        }
    }

    if (tunPort != OFPP_NONE) {
        switch (encapType) {
        case IntFlowManager::ENCAP_VLAN:
            ADDF(Bldr().table(SRC).priority(149)
                 .in(tunPort).isVlan(vnid)
                 .actions().popVlan()
                 .load(SEPG, vnid).load(BD, bdId)
                 .load(FD, fdId).load(RD, rdId)
                 .polApplied().go(SVR).done());
            ADDF(Bldr().table(OUT).priority(10).reg(SEPG, vnid)
                 .isMdAct(flow::meta::out::TUNNEL)
                 .actions().pushVlan().move(SEPG12, VLAN)
                 .outPort(tunPort).done());
            break;
        case IntFlowManager::ENCAP_VXLAN:
        case IntFlowManager::ENCAP_IVXLAN:
        default:
            ADDF(Bldr().table(SRC).priority(149).tunId(vnid)
                 .in(tunPort).actions().load(SEPG, vnid).load(BD, bdId)
                 .load(FD, fdId).load(RD, rdId).polApplied().go(SVR).done());
            if (remoteInventoryType == RemoteInventoryTypeEnumT::CONST_NONE) {
                ADDF(Bldr().table(OUT).priority(10).reg(SEPG, vnid)
                     .isMdAct(flow::meta::out::TUNNEL)
                     .actions().move(SEPG, TUNID)
                     .load(TUNDST, mcast.to_v4().to_ulong())
                     .outPort(tunPort).done());
                ADDF(Bldr().table(OUT).priority(11).reg(SEPG, vnid)
                     .isMdAct(flow::meta::out::TUNNEL)
                     .isEthDst(rmac)
                     .actions().move(SEPG, TUNID)
                     .load(TUNDST, intFlowManager.getTunnelDst()
                           .to_v4().to_ulong())
                     .outPort(tunPort).done());
            } else {
                ADDF(Bldr().table(OUT).priority(11).reg(SEPG, vnid)
                     .isMdAct(flow::meta::out::TUNNEL)
                     .actions().move(SEPG, TUNID)
                     .load(TUNDST, intFlowManager.getTunnelDst()
                           .to_v4().to_ulong())
                     .outPort(tunPort).done());
            }
            break;
        }
    }
    ADDF(Bldr().table(POL).priority(8292).reg(SEPG, vnid)
         .reg(DEPG, vnid).actions().go(STAT).done());
    ADDF(Bldr().table(OUT).priority(10)
         .reg(OUTPORT, vnid).isMdAct(flow::meta::out::RESUBMIT_DST)
         .actions().load(SEPG, vnid).load(BD, bdId)
         .load(FD, fdId).load(RD, rdId)
         .load(OUTPORT, 0).load64(METADATA, flow::meta::ROUTED)
         .resubmit(BR).done());
}

// Initialize flood domain-scoped flow entries
void BaseIntFlowManagerFixture::initExpFd(uint32_t fdId) {
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    if (fdId != 0) {
        ADDF(Bldr().table(OUT).priority(10).reg(FD, fdId)
             .isMdAct(flow::meta::out::FLOOD)
             .actions().group(fdId).done());
    }
}

// Initialize bridge domain-scoped flow entries
void BaseIntFlowManagerFixture::initExpBd(uint32_t bdId, uint32_t rdId,
                                          bool routeOn) {
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    if (routeOn) {
        /* Go to routing table */
        ADDF(Bldr().table(BR).priority(2).reg(BD, bdId)
             .actions().go(RT).done());

        /* router solicitation */
        ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::NEIGH_DISC))
             .table(BR).priority(20).icmp6().reg(BD, bdId).reg(RD, rdId)
             .isEthDst(mmac).icmp_type(133).icmp_code(0)
             .actions().controller(65535)
             .done());
    }
}

// Initialize routing domain-scoped flow entries
void BaseIntFlowManagerFixture::initExpRd(uint32_t rdId,
                                          bool isUnenforced,
                                          bool includeSubnets) {
    uint32_t tunPort = intFlowManager.getTunnelPort();

    ADDF(Bldr().cookie(ovs_htonll(flow::cookie::TABLE_DROP_FLOW|
         flow::cookie::RD_POL_DROP_FLOW)).flags(OFPUTIL_FF_SEND_FLOW_REM).
         table(POL).priority(1).reg(RD, rdId).actions().
         dropLog(POL).go(EXP_DROPLOG).done());

    if (isUnenforced) {
        ADDF(Bldr().table(POL).priority(8442).reg(RD, rdId).
                                  actions().go(STAT).done());
    }

    if (!includeSubnets) return;

    if (tunPort != OFPP_NONE) {
        ADDF(Bldr().table(RT).priority(324)
             .ip().reg(RD, rdId).isIpDst("10.20.44.0/24")
             .actions().mdAct(flow::meta::out::TUNNEL)
             .go(STAT).done());
        ADDF(Bldr().table(RT).priority(324)
             .ip().reg(RD, rdId).isIpDst("10.20.45.0/24")
             .actions().mdAct(flow::meta::out::TUNNEL)
             .go(STAT).done());
        ADDF(Bldr().table(RT).priority(332)
             .ipv6().reg(RD, rdId).isIpv6Dst("2001:db8::/32")
             .actions().mdAct(flow::meta::out::TUNNEL)
             .go(STAT).done());
    } else {
        ADDF(Bldr().cookie(ovs_htonll(flow::cookie::TABLE_DROP_FLOW))
             .flags(OFPUTIL_FF_SEND_FLOW_REM).table(RT).priority(324)
             .ip().reg(RD, rdId).isIpDst("10.20.44.0/24")
             .actions().dropLog(RT).go(EXP_DROPLOG).done());
        ADDF(Bldr().cookie(ovs_htonll(flow::cookie::TABLE_DROP_FLOW))
             .flags(OFPUTIL_FF_SEND_FLOW_REM).table(RT).priority(324)
             .ip().reg(RD, rdId).isIpDst("10.20.45.0/24")
             .actions().dropLog(RT).go(EXP_DROPLOG).done());
        ADDF(Bldr().cookie(ovs_htonll(flow::cookie::TABLE_DROP_FLOW))
             .flags(OFPUTIL_FF_SEND_FLOW_REM).table(RT).priority(332)
             .ipv6().reg(RD, rdId).isIpv6Dst("2001:db8::/32")
             .actions().dropLog(RT).go(EXP_DROPLOG).done());
    }
}

// Initialize endpoint-scoped flow entries
void BaseIntFlowManagerFixture::initExpEp(shared_ptr<Endpoint>& ep,
                                          shared_ptr<EpGroup>& epg,
                                          uint32_t fdId, uint32_t bdId,
                                          uint32_t rdId, bool arpOn,
                                          bool routeOn) {
    IntFlowManager::EncapType encapType = intFlowManager.getEncapType();
    string mac = ep->getMAC().get().toString();
    uint32_t port = portmapper.FindPort(ep->getInterfaceName().get());
    unordered_set<string> ips(ep->getIPs());
    string lladdr =
        network::construct_link_local_ip_addr(ep->getMAC().get()).to_string();
    ips.insert(lladdr);
    const unordered_set<string>* acastIps = &ep->getAnycastReturnIPs();
    if (acastIps->size() == 0) acastIps = &ips;
    uint32_t vnid = policyMgr.getVnidForGroup(epg->getURI()).get();
    uint32_t tunPort = intFlowManager.getTunnelPort();

    string bmac("ff:ff:ff:ff:ff:ff");
    uint8_t rmacArr[6];
    memcpy(rmacArr, intFlowManager.getRouterMacAddr(), sizeof(rmacArr));
    string rmac = MAC(rmacArr).toString();
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    // source rules
    if (port != OFPP_NONE) {
        ADDF(Bldr().table(SEC).priority(20).in(port).isEthSrc(mac)
             .actions().go(SRC).done());

        for (const string& ip : ips) {
            address ipa = address::from_string(ip);
            if (ipa.is_v4()) {
                ADDF(Bldr().table(SEC).priority(30).ip().in(port)
                     .isEthSrc(mac).isIpSrc(ip).actions().go(SRC).done());
                ADDF(Bldr().table(SEC).priority(40).arp().in(port)
                     .isEthSrc(mac).isSpa(ip).actions().go(SRC).done());
            } else {
                ADDF(Bldr().table(SEC).priority(30).ipv6().in(port)
                     .isEthSrc(mac).isIpv6Src(ip).actions().go(SRC).done());
                ADDF(Bldr().table(SEC).priority(40).icmp6().in(port)
                     .isEthSrc(mac).icmp_type(136).icmp_code(0).isNdTarget(ip)
                     .actions().go(SRC).done());
            }
        }

        ADDF(Bldr().table(SRC).priority(140).in(port)
             .isEthSrc(mac).actions().load(SEPG, vnid).load(BD, bdId)
             .load(FD, fdId).load(RD, rdId).go(SVR).done());
    }

    // dest rules
    if (port != OFPP_NONE) {
        // bridge
        ADDF(Bldr().table(BR).priority(10).reg(BD, bdId)
             .isEthDst(mac).actions().load(DEPG, vnid)
             .load(OUTPORT, port).go(POL).done());

        if (routeOn) {
            for (const string& ip : ips) {
                address ipa = address::from_string(ip);
                if (ipa.is_v4()) {
                    // route
                    ADDF(Bldr().table(RT).priority(500).ip()
                         .reg(RD, rdId)
                         .isEthDst(rmac).isIpDst(ip)
                         .actions().load(DEPG, vnid)
                         .load(OUTPORT, port).ethSrc(rmac)
                         .ethDst(mac).decTtl()
                         .meta(flow::meta::ROUTED, flow::meta::ROUTED)
                         .go(POL).done());
                    if (ep->isDiscoveryProxyMode()) {
                        // proxy arp
                        ADDF(Bldr().table(BR).priority(20).arp()
                             .reg(BD, bdId).reg(RD, rdId)
                             .isEthDst(bmac).isTpa(ipa.to_string())
                             .isArpOp(1)
                             .actions().move(ETHSRC, ETHDST)
                             .load(ETHSRC, "0x8000").load(ARPOP, 2)
                             .move(ARPSHA, ARPTHA).load(ARPSHA, "0x8000")
                             .move(ARPSPA, ARPTPA).load(ARPSPA,
                                                        ipa.to_v4().to_ulong())
                             .inport().done());
                        switch (encapType) {
                        case IntFlowManager::ENCAP_VLAN:
                            ADDF(Bldr().table(BR).priority(21).arp()
                                 .reg(BD, bdId).reg(RD, rdId).in(tunPort)
                                 .isEthDst(bmac).isTpa(ipa.to_string())
                                 .isArpOp(1)
                                 .actions()
                                 .move(ETHSRC, ETHDST)
                                 .load(ETHSRC, "0x8000").load(ARPOP, 2)
                                 .move(ARPSHA, ARPTHA).load(ARPSHA, "0x8000")
                                 .move(ARPSPA, ARPTPA)
                                 .load(ARPSPA, ipa.to_v4().to_ulong())
                                 .pushVlan().move(SEPG12, VLAN)
                                 .inport().done());
                            break;
                        case IntFlowManager::ENCAP_VXLAN:
                        case IntFlowManager::ENCAP_IVXLAN:
                        default:
                            ADDF(Bldr().table(BR).priority(21).arp()
                                 .reg(BD, bdId).reg(RD, rdId).in(tunPort)
                                 .isEthDst(bmac).isTpa(ipa.to_string())
                                 .isArpOp(1)
                                 .actions()
                                 .move(ETHSRC, ETHDST)
                                 .load(ETHSRC, "0x8000").load(ARPOP, 2)
                                 .move(ARPSHA, ARPTHA).load(ARPSHA, "0x8000")
                                 .move(ARPSPA, ARPTPA)
                                 .load(ARPSPA, ipa.to_v4().to_ulong())
                                 .move(TUNSRC, TUNDST)
                                 .inport().done());
                            break;
                        }
                    } else if (arpOn) {
                        // arp optimization
                        ADDF(Bldr().table(BR).priority(20).arp()
                             .reg(BD, bdId).reg(RD, rdId)
                             .isEthDst(bmac).isTpa(ip).isArpOp(1)
                             .actions().load(DEPG, vnid)
                             .load(OUTPORT, port)
                             .ethDst(mac).go(POL).done());
                    }
                } else {
                    // route
                    if (ip != lladdr) {
                        ADDF(Bldr().table(RT).priority(500).ipv6()
                             .reg(RD, rdId)
                             .isEthDst(rmac).isIpv6Dst(ip)
                             .actions().load(DEPG, vnid)
                             .load(OUTPORT, port).ethSrc(rmac)
                             .ethDst(mac).decTtl()
                             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
                             .go(POL).done());
                    }
                    if (ep->isDiscoveryProxyMode()) {
                        // proxy neighbor discovery
                        ADDF(Bldr()
                             .cookie(ovs_ntohll(flow::cookie::NEIGH_DISC))
                             .table(BR).priority(20).icmp6()
                             .reg(BD, bdId).reg(RD, rdId).isEthDst(mmac)
                             .icmp_type(135).icmp_code(0)
                             .isNdTarget(ipa.to_string())
                             .actions().load(SEPG, vnid)
                             .load64(METADATA, 0x100008000000000ll)
                             .controller(65535).done());
                    } else if (arpOn) {
                        // nd optimization
                        ADDF(Bldr().table(BR).priority(20).icmp6()
                             .reg(BD, bdId).reg(RD, rdId)
                             .isEthDst(mmac).icmp_type(135).icmp_code(0)
                             .isNdTarget(ip).actions().load(DEPG, vnid)
                             .load(OUTPORT, port)
                             .ethDst(mac).go(POL).done());
                    }
                }
            }
        }
        // service map
        uint8_t macAddr[6];
        ep->getMAC().get().toUIntArray(macAddr);
        uint64_t regValue =
            (((uint64_t)macAddr[5]) << 0 ) |
            (((uint64_t)macAddr[4]) << 8 ) |
            (((uint64_t)macAddr[3]) << 16) |
            (((uint64_t)macAddr[2]) << 24) |
            (((uint64_t)macAddr[1]) << 32) |
            (((uint64_t)macAddr[0]) << 40);

        uint64_t metadata = 0;
        ep->getMAC().get().toUIntArray((uint8_t*)&metadata);
        ((uint8_t*)&metadata)[7] = 1;

        for (const string& ip : *acastIps) {
            address ipa = address::from_string(ip);
            if (ipa.is_v4()) {
                ADDF(Bldr().table(SVD).priority(50).ip()
                     .reg(RD, rdId).isIpDst(ip)
                     .actions()
                     .ethSrc(rmac).ethDst(mac)
                     .decTtl().outPort(port).done());
                ADDF(Bldr().table(SVD).priority(51).arp()
                     .reg(RD, rdId)
                     .isEthDst(bmac).isTpa(ipa.to_string())
                     .isArpOp(1)
                     .actions().move(ETHSRC, ETHDST)
                     .load64(ETHSRC, regValue).load(ARPOP, 2)
                     .move(ARPSHA, ARPTHA).load64(ARPSHA, regValue)
                     .move(ARPSPA, ARPTPA).load(ARPSPA,
                                                ipa.to_v4().to_ulong())
                     .inport().done());
            } else {
                ADDF(Bldr().table(SVD).priority(50).ipv6()
                     .reg(RD, rdId).isIpv6Dst(ip)
                     .actions()
                     .ethSrc(rmac).ethDst(mac)
                     .decTtl().outPort(port).done());
                ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::NEIGH_DISC))
                     .table(SVD).priority(51).icmp6()
                     .reg(RD, rdId).isEthDst(mmac)
                     .icmp_type(135).icmp_code(0)
                     .isNdTarget(ipa.to_string())
                     .actions()
                     .load64(METADATA, metadata)
                     .controller(65535).done());
            }
        }
    }

    // hairpin output rule
    if (port != OFPP_NONE) {
        ADDF(Bldr().table(OUT).priority(2)
             .reg(OUTPORT, port).isMd("0x400/0x4ff").in(port)
             .actions().inport().done());
        ADDF(Bldr().table(OUT).priority(2)
             .reg(OUTPORT, port).isMd("0x403/0x4ff").in(port)
             .actions().inport().done());
    }
}

void BaseIntFlowManagerFixture::initSubnets(PolicyManager::subnet_vector_t& sns,
                                            uint32_t bdId, uint32_t rdId) {
    string bmac("ff:ff:ff:ff:ff:ff");
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");
    uint32_t tunPort = intFlowManager.getTunnelPort();

    /* Router entries when epg0 is connected to fd0
     */
    for (shared_ptr<Subnet>& sn : sns) {
        if (!sn->isAddressSet()) {
            continue;
        }
        address nip = address::from_string(sn->getAddress(""));
        address rip;
        if (nip.is_v6()) {
            rip = address::from_string("fe80::a8bb:ccff:fedd:eeff");
        } else {
            if (!sn->isVirtualRouterIpSet()) continue;
            rip = address::from_string(sn->getVirtualRouterIp(""));
        }

        if (rip.is_v6()) {
            ADDF(Bldr()
                 .cookie(ovs_ntohll(flow::cookie::NEIGH_DISC))
                 .table(BR).priority(22).icmp6()
                 .reg(BD, bdId).reg(RD, rdId).in(tunPort)
                 .isEthDst(mmac).icmp_type(135).icmp_code(0)
                 .isNdTarget(rip.to_string())
                 .actions().drop().done());
            ADDF(Bldr()
                 .cookie(ovs_ntohll(flow::cookie::NEIGH_DISC))
                 .table(BR).priority(20).icmp6()
                 .reg(BD, bdId).reg(RD, rdId)
                 .isEthDst(mmac).icmp_type(135).icmp_code(0)
                 .isNdTarget(rip.to_string())
                 .actions().controller(65535)
                 .done());
        } else {
            ADDF(Bldr().table(BR).priority(22).arp()
                 .reg(BD, bdId).reg(RD, rdId).in(tunPort)
                 .isEthDst(bmac).isTpa(rip.to_string())
                 .isArpOp(1).actions().drop().done());
            ADDF(Bldr().table(BR).priority(20).arp()
                 .reg(BD, bdId).reg(RD, rdId)
                 .isEthDst(bmac).isTpa(rip.to_string())
                 .isArpOp(1)
                 .actions().move(ETHSRC, ETHDST)
                 .load(ETHSRC, "0xaabbccddeeff")
                 .load(ARPOP, 2)
                 .move(ARPSHA, ARPTHA)
                 .load(ARPSHA, "0xaabbccddeeff")
                 .move(ARPSPA, ARPTPA)
                 .load(ARPSPA, rip.to_v4().to_ulong())
                 .inport().done());
        }
    }
}

void BaseIntFlowManagerFixture::initExpCon1() {
    uint16_t prio = PolicyManager::MAX_POLICY_RULE_PRIORITY;
    PolicyManager::uri_set_t ps, cs;
    unordered_set<uint32_t> pvnids, cvnids;

    policyMgr.getContractProviders(con1->getURI(), ps);
    policyMgr.getContractConsumers(con1->getURI(), cs);
    intFlowManager.getGroupVnid(ps, pvnids);
    intFlowManager.getGroupVnid(cs, cvnids);

    for (const uint32_t& pvnid : pvnids) {
        for (const uint32_t& cvnid : cvnids) {
            /* classifer 1  */
            const opflex::modb::URI& ruleURI_1 = classifier1->getURI();
            uint32_t con1_cookie = intFlowManager.getId(
                         classifier1->getClassId(), ruleURI_1);
            ADDF(Bldr(SEND_FLOW_REM).table(POL)
                 .priority(prio)
                 .cookie(con1_cookie).tcp()
                 .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpDst(80)
                 .actions()
                 .go(STAT)
                 .done());
            /* classifier 2  */
            const opflex::modb::URI& ruleURI_2 = classifier2->getURI();
            con1_cookie = intFlowManager.getId(classifier2->getClassId(),
                                               ruleURI_2);
            ADDF(Bldr(SEND_FLOW_REM).table(POL)
                 .priority(prio-128)
                 .cookie(con1_cookie).arp()
                 .reg(SEPG, pvnid).reg(DEPG, cvnid)
                 .actions().go(STAT).done());
            /* classifier 6 */
            const opflex::modb::URI& ruleURI_6 = classifier6->getURI();
            con1_cookie = intFlowManager.getId(classifier6->getClassId(),
                                               ruleURI_6);
            ADDF(Bldr(SEND_FLOW_REM).table(POL)
                 .priority(prio-256)
                 .cookie(con1_cookie).tcp()
                 .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpSrc(22)
                 .isTcpFlags("+syn+ack").actions().go(STAT).done());
            /* classifier 7 */
            const opflex::modb::URI& ruleURI_7 = classifier7->getURI();
            con1_cookie = intFlowManager.getId(classifier7->getClassId(),
                                               ruleURI_7);
            ADDF(Bldr(SEND_FLOW_REM).table(POL)
                 .priority(prio-384)
                 .cookie(con1_cookie).tcp()
                 .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpSrc(21)
                 .isTcpFlags("+ack").actions().go(STAT).done());
            ADDF(Bldr(SEND_FLOW_REM).table(POL)
                 .priority(prio-384)
                 .cookie(con1_cookie).tcp()
                 .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpSrc(21)
                 .isTcpFlags("+rst").actions().go(STAT).done());
        }
    }
}

void BaseIntFlowManagerFixture::initExpCon2() {
    uint16_t prio = PolicyManager::MAX_POLICY_RULE_PRIORITY;
    PolicyManager::uri_set_t ps, cs;
    unordered_set<uint32_t> ivnids;
    const opflex::modb::URI& ruleURI_5 = classifier5->getURI();
    uint32_t con2_cookie = intFlowManager.getId(
                         classifier5->getClassId(), ruleURI_5);

    policyMgr.getContractIntra(con2->getURI(), ps);
    intFlowManager.getGroupVnid(ps, ivnids);
    for (const uint32_t& ivnid : ivnids) {
        ADDF(Bldr(SEND_FLOW_REM).table(POL).priority(prio).cookie(con2_cookie)
             .reg(SEPG, ivnid).reg(DEPG, ivnid).isEth(0x8906)
             .actions().go(STAT).done());
    }
}

void BaseIntFlowManagerFixture::initExpCon3() {
    uint32_t epg0_vnid = policyMgr.getVnidForGroup(epg0->getURI()).get();
    uint32_t epg1_vnid = policyMgr.getVnidForGroup(epg1->getURI()).get();
    uint16_t prio = PolicyManager::MAX_POLICY_RULE_PRIORITY;
    PolicyManager::uri_set_t ps, cs;

    const opflex::modb::URI& ruleURI_3 = classifier3->getURI();
    uint32_t con3_cookie = intFlowManager.getId(
                         classifier3->getClassId(), ruleURI_3);
    const opflex::modb::URI& ruleURI_4 = classifier4->getURI();
    uint32_t con4_cookie = intFlowManager.getId(
                         classifier4->getClassId(), ruleURI_4);
    const opflex::modb::URI& ruleURI_10 = classifier10->getURI();
    uint32_t con10_cookie = intFlowManager.getId(
        classifier10->getClassId(), ruleURI_10);
    MaskList ml_80_85 = list_of<Mask>(0x0050, 0xfffc)(0x0054, 0xfffe);
    MaskList ml_66_69 = list_of<Mask>(0x0042, 0xfffe)(0x0044, 0xfffe);
    MaskList ml_94_95 = list_of<Mask>(0x005e, 0xfffe);
    for (const Mask& mk : ml_80_85) {
        ADDF(Bldr(SEND_FLOW_REM).table(POL).priority(prio)
             .cookie(con3_cookie).tcp()
             .reg(SEPG, epg1_vnid).reg(DEPG, epg0_vnid)
             .isTpDst(mk.first, mk.second).actions().drop().done());
    }
    for (const Mask& mks : ml_66_69) {
        for (const Mask& mkd : ml_94_95) {
            ADDF(Bldr(SEND_FLOW_REM).table(POL).priority(prio-128)
                 .cookie(con4_cookie).tcp()
                 .reg(SEPG, epg1_vnid).reg(DEPG, epg0_vnid)
                 .isTpSrc(mks.first, mks.second).isTpDst(mkd.first, mkd.second)
                 .actions().go(STAT).done());
        }
    }
    ADDF(Bldr(SEND_FLOW_REM).table(POL).priority(prio-256)
        .cookie(con10_cookie).icmp().reg(SEPG, epg1_vnid).reg(DEPG, epg0_vnid)
        .icmp_type(10).icmp_code(5).actions().go(STAT).done());
}

// Initialize flows related to IP address mapping/NAT
void BaseIntFlowManagerFixture::initExpIpMapping(bool natEpgMap, bool nextHop) {
    uint8_t rmacArr[6];
    memcpy(rmacArr, intFlowManager.getRouterMacAddr(), sizeof(rmacArr));
    string rmac(MAC(rmacArr).toString());
    uint32_t port = portmapper.FindPort(ep0->getInterfaceName().get());
    string epmac(ep0->getMAC().get().toString());
    string bmac("ff:ff:ff:ff:ff:ff");
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    uint32_t tunPort = intFlowManager.getTunnelPort();

    ADDF(Bldr().table(RT).priority(452).ip().reg(SEPG, 0x4242).reg(RD, 2)
         .isIpDst("5.5.5.5")
         .actions().ethSrc(rmac).ethDst(epmac)
         .ipDst("10.20.44.2").decTtl()
         .load(DEPG, 0xa0a).load(BD, 1).load(FD, 0)
         .load(RD, 1).load(OUTPORT, port)
         .meta(flow::meta::ROUTED, flow::meta::ROUTED)
         .go(NAT).done());
    ADDF(Bldr().table(RT).priority(450).ip().reg(RD, 2)
         .isIpDst("5.5.5.5")
         .actions().load(DEPG, 0x4242).load(OUTPORT, 0x4242)
         .mdAct(flow::meta::out::RESUBMIT_DST)
         .go(POL).done());
    ADDF(Bldr().table(BR).priority(20).arp()
         .reg(BD, 2).reg(RD, 2)
         .isEthDst(bmac).isTpa("5.5.5.5")
         .isArpOp(1)
         .actions().move(ETHSRC, ETHDST)
         .load(ETHSRC, "0x8000").load(ARPOP, 2)
         .move(ARPSHA, ARPTHA).load(ARPSHA, "0x8000")
         .move(ARPSPA, ARPTPA).load(ARPSPA, 0x5050505)
         .inport().done());
    ADDF(Bldr().table(BR).priority(21).arp()
         .reg(BD, 2).reg(RD, 2).in(tunPort)
         .isEthDst(bmac).isTpa("5.5.5.5")
         .isArpOp(1)
         .actions().move(ETHSRC, ETHDST)
         .load(ETHSRC, "0x8000").load(ARPOP, 2)
         .move(ARPSHA, ARPTHA).load(ARPSHA, "0x8000")
         .move(ARPSPA, ARPTPA).load(ARPSPA, 0x5050505)
         .move(TUNSRC, TUNDST)
         .inport().done());

    ADDF(Bldr().table(RT).priority(452).ipv6().reg(SEPG, 0x4242).reg(RD, 2)
         .isIpv6Dst("fdf1:9f86:d1af:6cc9::5")
         .actions().ethSrc(rmac).ethDst(epmac)
         .ipv6Dst("2001:db8::2").decTtl()
         .load(DEPG, 0xa0a).load(BD, 1).load(FD, 0)
         .load(RD, 1).load(OUTPORT, port)
         .meta(flow::meta::ROUTED, flow::meta::ROUTED)
         .go(NAT).done());
    ADDF(Bldr().table(RT).priority(450).ipv6().reg(RD, 2)
         .isIpv6Dst("fdf1:9f86:d1af:6cc9::5")
         .actions().load(DEPG, 0x4242).load(OUTPORT, 0x4242)
         .mdAct(flow::meta::out::RESUBMIT_DST)
         .go(POL).done());
    ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::NEIGH_DISC))
         .table(BR).priority(20).icmp6()
         .reg(BD, 2).reg(RD, 2).isEthDst(mmac).icmp_type(135).icmp_code(0)
         .isNdTarget("fdf1:9f86:d1af:6cc9::5")
         .actions().load(SEPG, 0x4242).load64(METADATA, 0x100008000000000ll)
         .controller(65535).done());

    if (natEpgMap) {
        ADDF(Bldr().table(RT).priority(166).ipv6().reg(RD, 1)
             .isIpv6Dst("fdf1::/16")
             .actions().load(DEPG, 0x80000001).load(OUTPORT, 0x4242)
             .mdAct(flow::meta::out::NAT).go(POL).done());
        ADDF(Bldr().table(RT).priority(158).ip().reg(RD, 1)
             .isIpDst("5.0.0.0/8")
             .actions().load(DEPG, 0x80000001).load(OUTPORT, 0x4242)
             .mdAct(flow::meta::out::NAT).go(POL).done());
    } else {
        ADDF(Bldr().table(RT).priority(166).ipv6().reg(RD, 1)
             .isIpv6Dst("fdf1::/16")
             .actions().mdAct(flow::meta::out::TUNNEL)
             .go(STAT).done());
        ADDF(Bldr().table(RT).priority(158).ip().reg(RD, 1)
             .isIpDst("5.0.0.0/8")
             .actions().mdAct(flow::meta::out::TUNNEL)
             .go(STAT).done());
    }

    ADDF(Bldr().table(NAT).priority(166).ipv6().reg(RD, 1)
         .isIpv6Src("fdf1::/16").actions()
         .load(SEPG, 0x80000001)
         .meta(flow::meta::out::REV_NAT,
               flow::meta::out::MASK |
               flow::meta::POLICY_APPLIED).go(POL).done());
    ADDF(Bldr().table(NAT).priority(158).ip().reg(RD, 1)
         .isIpSrc("5.0.0.0/8").actions()
         .load(SEPG, 0x80000001)
         .meta(flow::meta::out::REV_NAT,
               flow::meta::out::MASK |
               flow::meta::POLICY_APPLIED).go(POL).done());

    if (nextHop) {
        ADDF(Bldr().table(SRC).priority(201).ip().in(42)
             .isEthSrc("42:00:42:42:42:42").isIpDst("5.5.5.5")
             .actions().ethSrc(rmac).ethDst(epmac).ipDst("10.20.44.2").decTtl()
             .load(DEPG, 0xa0a).load(BD, 1).load(FD, 0).load(RD, 1)
             .load(OUTPORT, 0x50)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(NAT).done());
        ADDF(Bldr().table(SRC).priority(200).isPktMark(1).ip().in(42)
             .isEthSrc("42:00:42:42:42:42").isIpDst("10.20.44.2")
             .actions().ethSrc(rmac)
             .load(DEPG, 0xa0a).load(BD, 1).load(FD, 0).load(RD, 1)
             .load(OUTPORT, 0x50)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(NAT).done());
        ADDF(Bldr().table(SRC).priority(201).ipv6().in(42)
             .isEthSrc("42:00:42:42:42:42").isIpv6Dst("fdf1:9f86:d1af:6cc9::5")
             .actions().ethSrc(rmac).ethDst(epmac)
             .ipv6Dst("2001:db8::2").decTtl()
             .load(DEPG, 0xa0a).load(BD, 1).load(FD, 0).load(RD, 1)
             .load(OUTPORT, 0x50)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(NAT).done());
        ADDF(Bldr().table(SRC).priority(200).isPktMark(1).ipv6().in(42)
             .isEthSrc("42:00:42:42:42:42").isIpv6Dst("2001:db8::2")
             .actions().ethSrc(rmac)
             .load(DEPG, 0xa0a).load(BD, 1).load(FD, 0).load(RD, 1)
             .load(OUTPORT, 0x50)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(NAT).done());
        ADDF(Bldr().table(OUT).priority(10).ipv6()
             .reg(RD, 1).reg(OUTPORT, 0x4242)
             .isMdAct(flow::meta::out::NAT).isIpv6Src("2001:db8::2")
             .actions().ethSrc(epmac).ethDst("42:00:42:42:42:42")
             .ipv6Src("fdf1:9f86:d1af:6cc9::5").decTtl()
             .load(PKT_MARK, 1).outPort(42).done());
        ADDF(Bldr().table(OUT).priority(10).ip()
             .reg(RD, 1).reg(OUTPORT, 0x4242)
             .isMdAct(flow::meta::out::NAT).isIpSrc("10.20.44.2")
             .actions().ethSrc(epmac).ethDst("42:00:42:42:42:42")
             .ipSrc("5.5.5.5").decTtl()
             .load(PKT_MARK, 1).outPort(42).done());
    } else {
        ADDF(Bldr().table(OUT).priority(10).ipv6()
             .reg(RD, 1).reg(OUTPORT, 0x4242)
             .isMdAct(flow::meta::out::NAT).isIpv6Src("2001:db8::2")
             .actions().ethSrc(epmac).ethDst(rmac)
             .ipv6Src("fdf1:9f86:d1af:6cc9::5").decTtl()
             .load(SEPG, 0x4242).load(BD, 2).load(FD, 1).load(RD, 2)
             .load(OUTPORT, 0)
             .load64(METADATA, flow::meta::ROUTED).resubmit(BR).done());
        ADDF(Bldr().table(OUT).priority(10).ip()
             .reg(RD, 1).reg(OUTPORT, 0x4242)
             .isMdAct(flow::meta::out::NAT).isIpSrc("10.20.44.2")
             .actions().ethSrc(epmac).ethDst(rmac)
             .ipSrc("5.5.5.5").decTtl()
             .load(SEPG, 0x4242).load(BD, 2).load(FD, 1).load(RD, 2)
             .load(OUTPORT, 0).load64(METADATA, flow::meta::ROUTED)
             .resubmit(BR).done());
    }
}

void BaseIntFlowManagerFixture::initExpRemoteEp() {
    IntFlowManager::EncapType encapType = intFlowManager.getEncapType();
    uint32_t vnid = policyMgr.getVnidForGroup(epg0->getURI()).get();
    address tunDst = address::from_string("5.6.7.8");
    uint8_t rmacArr[6];
    memcpy(rmacArr, intFlowManager.getRouterMacAddr(), sizeof(rmacArr));
    string rmac = MAC(rmacArr).toString();
    string bmac("ff:ff:ff:ff:ff:ff");

    if (encapType != IntFlowManager::ENCAP_VLAN) {
        ADDF(Bldr().table(BR).priority(10).reg(BD, 1)
             .isEthDst("ab:cd:ef:ab:cd:ef").actions().load(DEPG, vnid)
             .load(OUTPORT, tunDst.to_v4().to_ulong())
             .mdAct(flow::meta::out::REMOTE_TUNNEL)
             .go(POL).done());
        ADDF(Bldr().table(RT).priority(500).ip().reg(RD, 1)
             .isEthDst(rmac).isIpDst("1.3.5.7")
             .actions().load(DEPG, vnid)
             .load(OUTPORT, tunDst.to_v4().to_ulong())
             .mdAct(flow::meta::out::REMOTE_TUNNEL)
             .go(POL).done());
        ADDF(Bldr().table(RT).priority(500).ip().reg(RD, 1)
             .isEthDst(rmac).isIpDst("2.4.6.8")
             .actions().load(DEPG, vnid)
             .load(OUTPORT, tunDst.to_v4().to_ulong())
             .mdAct(flow::meta::out::REMOTE_TUNNEL)
             .go(POL).done());
        ADDF(Bldr().table(BR).priority(40).arp()
             .reg(BD, 1).reg(RD, 1)
             .isEthDst(bmac).isTpa("1.3.5.7")
             .isArpOp(1)
             .actions().move(ETHSRC, ETHDST)
             .load(ETHSRC, "0xabcdefabcdef").load(ARPOP, 2)
             .move(ARPSHA, ARPTHA).load(ARPSHA, "0xabcdefabcdef")
             .move(ARPSPA, ARPTPA).load(ARPSPA, "0x1030507")
             .inport().done());
        ADDF(Bldr().table(BR).priority(40).arp()
             .reg(BD, 1).reg(RD, 1)
             .isEthDst(bmac).isTpa("2.4.6.8")
             .isArpOp(1)
             .actions().move(ETHSRC, ETHDST)
             .load(ETHSRC, "0xabcdefabcdef").load(ARPOP, 2)
             .move(ARPSHA, ARPTHA).load(ARPSHA, "0xabcdefabcdef")
             .move(ARPSPA, ARPTPA).load(ARPSPA, "0x2040608")
             .inport().done());
    }
}

void BaseIntFlowManagerFixture::initExpAnycastService(Service &as, int nextHop) {
    string mac = "ed:84:da:ef:16:96";
    string bmac("ff:ff:ff:ff:ff:ff");
    uint8_t rmacArr[6];
    memcpy(rmacArr, intFlowManager.getRouterMacAddr(), sizeof(rmacArr));
    string rmac = MAC(rmacArr).toString();
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    if (nextHop) {
        std::stringstream mss;
        mss << "symmetric_l3l4+udp,1024,iter_hash,"
            << nextHop << ",32,NXM_NX_REG7[]";
        ADDF(Bldr().table(BR).priority(50)
             .ip().reg(RD, 1).isIpDst("169.254.169.254")
             .actions()
             .ethSrc(rmac).ethDst(mac)
             .multipath(mss.str())
             .go(SVH).done());
        ADDF(Bldr().table(BR).priority(50)
             .ipv6().reg(RD, 1).isIpv6Dst("fe80::a9:fe:a9:fe")
             .actions()
             .ethSrc(rmac).ethDst(mac)
             .multipath(mss.str())
             .go(SVH).done());

        ADDF(Bldr().table(SVH).priority(99)
             .ip().reg(RD, 1)
             .isIpDst("169.254.169.254")
             .actions()
             .ipDst("169.254.169.1").decTtl()
             .outPort(17).done());
        ADDF(Bldr().table(SVH).priority(99)
             .ipv6().reg(RD, 1)
             .isIpv6Dst("fe80::a9:fe:a9:fe")
             .actions()
             .ipv6Dst("fe80::a9:fe:a9:1")
             .decTtl()
             .outPort(17).done());

        ADDF(Bldr().table(SEC).priority(100).ip().in(17)
             .isEthSrc("ed:84:da:ef:16:96")
             .isIpSrc("169.254.169.1")
             .actions().load(RD, 1).ipSrc("169.254.169.254")
             .decTtl()
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(SVD).done());
        ADDF(Bldr().table(SEC).priority(100).arp().in(17)
             .isEthSrc("ed:84:da:ef:16:96").isSpa("169.254.169.1")
             .actions().load(RD, 1).go(SVD).done());
        ADDF(Bldr().table(SEC).priority(100).ipv6().in(17)
             .isEthSrc("ed:84:da:ef:16:96")
             .isIpv6Src("fe80::a9:fe:a9:1")
             .actions().load(RD, 1).ipv6Src("fe80::a9:fe:a9:fe")
             .decTtl()
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(SVD).done());

        if (nextHop >= 2) {
            ADDF(Bldr().table(SVH).priority(100)
                 .ip().reg(RD, 1).reg(OUTPORT, 1)
                 .isIpDst("169.254.169.254")
                 .actions()
                 .ipDst("169.254.169.2").decTtl()
                 .outPort(17).done());
            ADDF(Bldr().table(SVH).priority(100)
                 .ipv6().reg(RD, 1).reg(OUTPORT, 1)
                 .isIpv6Dst("fe80::a9:fe:a9:fe")
                 .actions()
                 .ipv6Dst("fe80::a9:fe:a9:2")
                 .decTtl()
                 .outPort(17).done());

            ADDF(Bldr().table(SEC).priority(100).ip().in(17)
                 .isEthSrc("ed:84:da:ef:16:96")
                 .isIpSrc("169.254.169.2")
                 .actions().load(RD, 1).ipSrc("169.254.169.254")
                 .decTtl()
                 .meta(flow::meta::ROUTED, flow::meta::ROUTED)
                 .go(SVD).done());
            ADDF(Bldr().table(SEC).priority(100).arp().in(17)
                 .isEthSrc("ed:84:da:ef:16:96").isSpa("169.254.169.2")
                 .actions().load(RD, 1).go(SVD).done());
            ADDF(Bldr().table(SEC).priority(100).ipv6().in(17)
                 .isEthSrc("ed:84:da:ef:16:96")
                 .isIpv6Src("fe80::a9:fe:a9:2")
                 .actions().load(RD, 1).ipv6Src("fe80::a9:fe:a9:fe")
                 .decTtl()
                 .meta(flow::meta::ROUTED, flow::meta::ROUTED)
                 .go(SVD).done());
        }
    } else {
        ADDF(Bldr().table(BR).priority(50)
             .ip().reg(RD, 1).isIpDst("169.254.169.254")
             .actions()
             .ethSrc(rmac).ethDst(mac).decTtl()
             .outPort(17).done());
        ADDF(Bldr().table(BR).priority(50)
             .ipv6().reg(RD, 1).isIpv6Dst("fe80::a9:fe:a9:fe")
             .actions()
             .ethSrc(rmac).ethDst(mac).decTtl()
             .outPort(17).done());

        ADDF(Bldr().table(SEC).priority(100).ip().in(17)
             .isEthSrc("ed:84:da:ef:16:96")
             .isIpSrc("169.254.169.254")
             .actions().load(RD, 1)
             .go(SVD).done());
        ADDF(Bldr().table(SEC).priority(100).arp().in(17)
             .isEthSrc("ed:84:da:ef:16:96").isSpa("169.254.169.254")
             .actions().load(RD, 1).go(SVD).done());
        ADDF(Bldr().table(SEC).priority(100).ipv6().in(17)
             .isEthSrc("ed:84:da:ef:16:96")
             .isIpv6Src("fe80::a9:fe:a9:fe")
             .actions().load(RD, 1)
             .go(SVD).done());
    }

    ADDF(Bldr().table(BR).priority(51).arp()
         .reg(RD, 1)
         .isEthDst(bmac).isTpa("169.254.169.254")
         .isArpOp(1)
         .actions().move(ETHSRC, ETHDST)
         .load(ETHSRC, "0xed84daef1696").load(ARPOP, 2)
         .move(ARPSHA, ARPTHA).load(ARPSHA, "0xed84daef1696")
         .move(ARPSPA, ARPTPA).load(ARPSPA, "0xa9fea9fe")
         .inport().done());
    ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::NEIGH_DISC))
         .table(BR).priority(51).icmp6()
         .reg(RD, 1).isEthDst(mmac)
         .icmp_type(135).icmp_code(0)
         .isNdTarget("fe80::a9:fe:a9:fe")
         .actions()
         .load64(METADATA, 0x1009616efda84edll)
         .controller(65535).done());

    ADDF(Bldr().table(SVD).priority(31).arp()
         .reg(RD, 1)
         .isEthSrc("ed:84:da:ef:16:96")
         .isEthDst(bmac).isTpa("169.254.1.1")
         .isArpOp(1)
         .actions().move(ETHSRC, ETHDST)
         .load(ETHSRC, "0xaabbccddeeff").load(ARPOP, 2)
         .move(ARPSHA, ARPTHA).load(ARPSHA, "0xaabbccddeeff")
         .move(ARPSPA, ARPTPA).load(ARPSPA, "0xa9fe0101")
         .inport().done());
    ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::NEIGH_DISC))
         .table(SVD).priority(31).icmp6()
         .reg(RD, 1).isEthSrc("ed:84:da:ef:16:96")
         .isEthDst(mmac)
         .icmp_type(135).icmp_code(0)
         .isNdTarget("fe80::1")
         .actions()
         .load64(METADATA, 0x300ffeeddccbbaall)
         .controller(65535).done());
}

void BaseIntFlowManagerFixture::initExpPodServiceStats (const string& svc_ip,
                                                        shared_ptr<Endpoint>& ep,
                                                        Service &as,
                                                        const string& proto,
                                                        uint16_t svc_port,
                                                        uint16_t nh_port)
{
    address svc_ipa = address::from_string(svc_ip);

    // Check if podtosvc and vice-versa are created, indirectly ensuring
    // cookie is created for these flows
    auto epUuid = ep->getUUID();
    auto svcUuid = as.getUUID();
    auto epSvcUuid = epUuid + ":" + svcUuid;
    auto epToSvcUuid = "eptosvc:"+epSvcUuid;
    auto svcToEpUuid = "svctoep:"+epSvcUuid;
    optional<shared_ptr<SvcStatUniverse> > su =
                SvcStatUniverse::resolve(agent.getFramework());
    BOOST_CHECK(su);
    auto aUuid = boost::lexical_cast<std::string>(agent.getUuid());
    WAIT_FOR_DO_ONFAIL(su.get()->resolveGbpeEpToSvcCounter(aUuid, epToSvcUuid), 500,
                        ,LOG(ERROR) << "ep2svc obj not resolved uuid: " << epToSvcUuid;);
    WAIT_FOR_DO_ONFAIL(su.get()->resolveGbpeSvcToEpCounter(aUuid, svcToEpUuid), 500,
                        ,LOG(ERROR) << "svc2ep obj not resolved uuid: " << svcToEpUuid;);

    auto expMatchProto = [&proto] (Bldr& b,
                                   bool is_v4) -> Bldr& {
        if (proto == "") {
            if (is_v4)
                b.ip();
            else
                b.ipv6();
        } else if (proto == "tcp") {
            if (is_v4)
                b.tcp();
            else
                b.tcp6();
        } else if (proto == "udp") {
            if (is_v4)
                b.udp();
            else
                b.udp6();
        }
        return b;
    };

    auto expMatchPort =
            [&svc_port, &nh_port] (Bldr& b,
                                   bool fwd) -> Bldr& {
        if (!svc_port)
            return b;

        if (fwd)
            b.isTpDst(nh_port);
        else
            b.isTpSrc(svc_port);
        return b;
    };

    for (const string& ep_ip : ep->getIPs()) {
        bool skip_flow = false;
        for (auto const& sm : as.getServiceMappings()) {
            // Dont create EPIP <--> SVCIP flows if EPIP is one of the
            // next hops of this service.
            const auto& nhips = sm.getNextHopIPs();
            if (nhips.find(ep_ip) != nhips.end()) {
                skip_flow = true;
                break;
            }
        }

        if (skip_flow)
            continue;

        address ep_ipa = address::from_string(ep_ip);
        uint64_t ep_to_svc_cookie=0, svc_to_ep_cookie=0;
        intFlowManager.getPodSvcUuidCookie(epSvcUuid,true,ep_to_svc_cookie);
        intFlowManager.getPodSvcUuidCookie(epSvcUuid,false,svc_to_ep_cookie);

        // ensure flows are either v4 or v6 - no mix-n-match
        if (svc_ipa.is_v4() != ep_ipa.is_v4())
            continue;

        if (ep_ipa.is_v4()) {
            // to svc
            ADDF(expMatchPort(expMatchProto(
                 Bldr(SEND_FLOW_REM).table(STAT)
                 .cookie(ep_to_svc_cookie)
                 .priority(100), true)
                 .reg(SVCADDR1, svc_ipa.to_v4().to_ulong())
                 .isIpSrc(ep_ip), true)
                 .actions().go(OUT).done());
            // from svc
            ADDF(expMatchPort(expMatchProto(
                 Bldr(SEND_FLOW_REM).table(STAT)
                 .cookie(svc_to_ep_cookie)
                 .priority(100), true)
                 .isIpSrc(svc_ip)
                 .isIpDst(ep_ip), false)
                 .actions().go(OUT).done());
        } else {
            // to svc
            uint32_t pAddr[4];
            IntFlowManager::in6AddrToLong(svc_ipa, &pAddr[0]);
            ADDF(expMatchPort(expMatchProto(
                 Bldr(SEND_FLOW_REM).table(STAT)
                 .cookie(ep_to_svc_cookie)
                 .priority(100), false)
                 .reg(SVCADDR1, pAddr[0])
                 .reg(SVCADDR2, pAddr[1])
                 .reg(SVCADDR3, pAddr[2])
                 .reg(SVCADDR4, pAddr[3])
                 .isIpv6Src(ep_ip), true)
                 .actions().go(OUT).done());
            // from svc
            ADDF(expMatchPort(expMatchProto(
                 Bldr(SEND_FLOW_REM).table(STAT)
                 .cookie(svc_to_ep_cookie)
                 .priority(100), false)
                 .isIpv6Src(svc_ip)
                 .isIpv6Dst(ep_ip), false)
                 .actions().go(OUT).done());
        }
    }
}

void BaseIntFlowManagerFixture::initExpAnySvcStats (const string& nh_ip,
                                                    uint16_t nh_port,
                                                    const string& uuid,
                                                    uint64_t rxCookie,
                                                    uint64_t txCookie)
{
    address ipa = address::from_string(nh_ip);
    if (ipa.is_v4()) {
        // to svc
        ADDF(Bldr(SEND_FLOW_REM).table(STAT)
             .cookie(rxCookie)
             .priority(97).udp()
             .reg(CTMARK, "0/0x80000000")
             .isIpDst(nh_ip).isTpDst(nh_port)
             .actions().go(OUT).done());
        // from svc
        ADDF(Bldr(SEND_FLOW_REM).table(STAT)
             .cookie(txCookie)
             .priority(97).udp()
             .reg(CTMARK, "0/0x80000000")
             .isIpSrc(nh_ip).isTpSrc(nh_port)
             .actions().go(OUT).done());
    } else {
        // to svc
        ADDF(Bldr(SEND_FLOW_REM).table(STAT)
             .cookie(rxCookie)
             .priority(97).tcp6()
             .reg(CTMARK, "0/0x80000000")
             .isIpv6Dst(nh_ip).isTpDst(nh_port)
             .actions().go(OUT).done());
        // from svc
        ADDF(Bldr(SEND_FLOW_REM).table(STAT)
             .cookie(txCookie)
             .priority(97).tcp6()
             .reg(CTMARK, "0/0x80000000")
             .isIpv6Src(nh_ip).isTpSrc(nh_port)
             .actions().go(OUT).done());
    }
}

void BaseIntFlowManagerFixture::initExpLBService(Service &as1,
                                                 Service &as2,
                                                 bool conntrack,
                                                 bool exposed) {
    string mac = "13:37:13:37:13:37";
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");
    string bmac("ff:ff:ff:ff:ff:ff");
    uint8_t rmacArr[6];
    memcpy(rmacArr, intFlowManager.getRouterMacAddr(), sizeof(rmacArr));
    string rmac = MAC(rmacArr).toString();

    uint64_t rxCookie1 = 0;
    uint64_t txCookie1 = 0;
    uint64_t rxCookie2 = 0;
    uint64_t txCookie2 = 0;
    if (!exposed) {
        initExpPodServiceStats("169.254.169.254", ep0, as1, "udp", 53, 5353);
        initExpPodServiceStats("169.254.169.254", ep2, as1, "udp", 53, 5353);
        initExpPodServiceStats("169.254.169.254", ep3, as1, "udp", 53, 5353);
        initExpPodServiceStats("169.254.169.254", ep4, as1, "udp", 53, 5353);
        initExpPodServiceStats("fe80::a9:fe:a9:fe", ep0, as2, "tcp", 80, 80);

        const string& anyToSvcKey1 = "antosvc:svc-tgt:"+as1.getUUID()+":10.20.44.2";
        const string& svcToAnyKey1 = "svctoan:svc-tgt:"+as1.getUUID()+":10.20.44.2";
        const string& anyToSvcKey2 = "antosvc:svc-tgt:"+as2.getUUID()+":2001:db8::2";
        const string& svcToAnyKey2 = "svctoan:svc-tgt:"+as2.getUUID()+":2001:db8::2";
        WAIT_FOR_DO_ONFAIL((idGen.getIdNoAlloc("svcstats", anyToSvcKey1) != (uint32_t)-1),
                            500,,
                            LOG(ERROR) << "cookie not yet alloc'd for: " << anyToSvcKey1;);
        WAIT_FOR_DO_ONFAIL((idGen.getIdNoAlloc("svcstats", svcToAnyKey1) != (uint32_t)-1),
                            500,,
                            LOG(ERROR) << "cookie not yet alloc'd for: " << svcToAnyKey1;);
        WAIT_FOR_DO_ONFAIL((idGen.getIdNoAlloc("svcstats", anyToSvcKey2) != (uint32_t)-1),
                            500,,
                            LOG(ERROR) << "cookie not yet alloc'd for: " << anyToSvcKey2;);
        WAIT_FOR_DO_ONFAIL((idGen.getIdNoAlloc("svcstats", svcToAnyKey2) != (uint32_t)-1),
                            500,,
                            LOG(ERROR) << "cookie not yet alloc'd for: " << svcToAnyKey2;);

        rxCookie1 = idGen.getIdNoAlloc("svcstats", anyToSvcKey1);
        txCookie1 = idGen.getIdNoAlloc("svcstats", svcToAnyKey1);
        rxCookie2 = idGen.getIdNoAlloc("svcstats", anyToSvcKey2);
        txCookie2 = idGen.getIdNoAlloc("svcstats", svcToAnyKey2);

        initExpAnySvcStats("10.20.44.2", 5353, as1.getUUID(), rxCookie1, txCookie1);
        initExpAnySvcStats("2001:db8::2", 80, as2.getUUID(), rxCookie2, txCookie2);
    } else {
        const string& extToSvcKey1 = "extosvc:svc-ext:"+as1.getUUID()+":10.20.44.2";
        const string& svcToExtKey1 = "svctoex:svc-ext:"+as1.getUUID()+":10.20.44.2";
        const string& extToSvcKey2 = "extosvc:svc-ext:"+as2.getUUID()+":2001:db8::2";
        const string& svcToExtKey2 = "svctoex:svc-ext:"+as2.getUUID()+":2001:db8::2";
        WAIT_FOR_DO_ONFAIL((idGen.getIdNoAlloc("svcstats", extToSvcKey1) != (uint32_t)-1),
                            500,,
                            LOG(ERROR) << "cookie not yet alloc'd for: " << extToSvcKey1;);
        WAIT_FOR_DO_ONFAIL((idGen.getIdNoAlloc("svcstats", svcToExtKey1) != (uint32_t)-1),
                            500,,
                            LOG(ERROR) << "cookie not yet alloc'd for: " << svcToExtKey1;);
        WAIT_FOR_DO_ONFAIL((idGen.getIdNoAlloc("svcstats", extToSvcKey2) != (uint32_t)-1),
                            500,,
                            LOG(ERROR) << "cookie not yet alloc'd for: " << extToSvcKey2;);
        WAIT_FOR_DO_ONFAIL((idGen.getIdNoAlloc("svcstats", svcToExtKey2) != (uint32_t)-1),
                            500,,
                            LOG(ERROR) << "cookie not yet alloc'd for: " << svcToExtKey2;);

        rxCookie1 = idGen.getIdNoAlloc("svcstats", extToSvcKey1);
        txCookie1 = idGen.getIdNoAlloc("svcstats", svcToExtKey1);
        rxCookie2 = idGen.getIdNoAlloc("svcstats", extToSvcKey2);
        txCookie2 = idGen.getIdNoAlloc("svcstats", svcToExtKey2);
    }

    if (exposed) {
        ADDF(Bldr().table(SEC).priority(90).in(17).isVlan(4003)
             .isEthDst(mac)
             .actions()
             .popVlan().load(SEPG, 4003).load(RD, 1)
             .meta(flow::meta::POLICY_APPLIED |
                   flow::meta::FROM_SERVICE_INTERFACE,
                   flow::meta::POLICY_APPLIED |
                   flow::meta::FROM_SERVICE_INTERFACE)
             .go(BR).done());
        ADDF(Bldr().table(SEC).priority(90).arp().in(17).isVlan(4003)
             .isEthDst(bmac).isArpOp(1)
             .actions()
             .popVlan().load(SEPG, 4003).load(RD, 1)
             .meta(flow::meta::POLICY_APPLIED |
                   flow::meta::FROM_SERVICE_INTERFACE,
                   flow::meta::POLICY_APPLIED |
                   flow::meta::FROM_SERVICE_INTERFACE)
             .go(BR).done());
        ADDF(Bldr().table(BR).priority(52).arp()
             .reg(RD, 1).in(17)
             .isEthDst(bmac).isTpa("1.1.1.1")
             .isArpOp(1)
             .actions().move(ETHSRC, ETHDST)
             .load(ETHSRC, "0x133713371337").load(ARPOP, 2)
             .move(ARPSHA, ARPTHA).load(ARPSHA, "0x133713371337")
             .move(ARPSPA, ARPTPA).load(ARPSPA, "0x1010101")
             .pushVlan().move(SEPG12, VLAN)
             .inport().done());
        ADDF(Bldr().table(BR).priority(51).arp()
             .reg(RD, 1)
             .isEthDst(bmac).isTpa("1.1.1.1")
             .isArpOp(1)
             .actions().move(ETHSRC, ETHDST)
             .load(ETHSRC, "0x133713371337").load(ARPOP, 2)
             .move(ARPSHA, ARPTHA).load(ARPSHA, "0x133713371337")
             .move(ARPSPA, ARPTPA).load(ARPSPA, "0x1010101")
             .inport().done());
        ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::NEIGH_DISC))
             .table(BR).priority(51).icmp6()
             .reg(RD, 1).isEthDst(mmac)
             .icmp_type(135).icmp_code(0)
             .isNdTarget("fd82:3372:749c:631b::42")
             .actions()
             .load(SEPG, 0xfa3).load64(METADATA, 0x101371337133713ll)
             .controller(65535).done());

        ADDF(Bldr().table(BR).priority(51)
             .cookie(ovs_ntohll(flow::cookie::ICMP_ECHO_V4))
             .icmp().reg(RD, 1)
             .isIpDst("1.1.1.1").icmp_type(8).icmp_code(0)
             .actions().load64(METADATA, ovs_htonll(0x100ll))
             .controller(65535).done());
        ADDF(Bldr().table(BR).priority(51)
             .cookie(ovs_ntohll(flow::cookie::ICMP_ECHO_V6))
             .icmp6().reg(RD, 1)
             .isIpv6Dst("fd82:3372:749c:631b::42").icmp_type(128).icmp_code(0)
             .actions().load64(METADATA, ovs_htonll(0x100ll))
             .controller(65535).done());
    }

    auto expIsEthDst = [&exposed, &mac](Bldr& b) -> Bldr& {
        if (exposed)
            b.isEthDst(mac);
        return b;
    };
    ADDF(expIsEthDst(Bldr().table(BR).priority(50)
                     .udp().reg(RD, 1))
         .isIpDst("169.254.169.254").isTpDst(53)
         .actions()
         .ethDst(rmac)
         .multipath("symmetric_l3l4+udp,1024,iter_hash,2,32,NXM_NX_REG7[]")
         .go(SVH).done());
    ADDF(expIsEthDst(Bldr().table(BR).priority(50)
                     .tcp6().reg(RD, 1))
         .isIpv6Dst("fe80::a9:fe:a9:fe").isTpDst(80)
         .actions()
         .ethDst(rmac)
         .multipath("symmetric_l3l4+udp,1024,iter_hash,2,32,NXM_NX_REG7[]")
         .go(SVH).done());

    if (conntrack) {
        string commit1 = string("commit,zone=1,exec(load:") +
            (exposed ? "0x80000001" : "0x1") + "->NXM_NX_CT_MARK[])";
        string commit2 = string("commit,zone=1,exec(load:") +
            (exposed ? "0x80000002" : "0x2") + "->NXM_NX_CT_MARK[])";

        ADDF(Bldr(SEND_FLOW_REM).table(SVH)
             .cookie(rxCookie1)
             .priority(99)
             .udp().reg(RD, 1).isFromServiceIface(exposed)
             .isIpDst("169.254.169.254").isTpDst(53)
             .actions()
             .tpDst(5353).ipDst("10.20.44.2")
             .decTtl()
             .load(SVCADDR1, 0xa9fea9fe)
             .load(CTMARK, exposed?0x80000001:0x1)
             .ct(commit1)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
        ADDF(Bldr().table(SVH).priority(100)
             .udp().reg(RD, 1).reg(OUTPORT, 1).isFromServiceIface(exposed)
             .isIpDst("169.254.169.254").isTpDst(53)
             .actions()
             .tpDst(5353).ipDst("169.254.169.2")
             .decTtl()
             .load(SVCADDR1, 0xa9fea9fe)
             .load(CTMARK, exposed?0x80000001:0x1)
             .ct(commit1)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
        ADDF(Bldr(SEND_FLOW_REM).table(SVH)
             .cookie(rxCookie2)
             .priority(99)
             .tcp6().reg(RD, 1).isFromServiceIface(exposed)
             .isIpv6Dst("fe80::a9:fe:a9:fe").isTpDst(80)
             .actions()
             .ipv6Dst("2001:db8::2")
             .decTtl()
             .load(SVCADDR1, 0xfe800000)
             .load(SVCADDR2, 0x0)
             .load(SVCADDR3, 0xa900fe)
             .load(SVCADDR4, 0xa900fe)
             .load(CTMARK, exposed?0x80000002:0x2)
             .ct(commit2)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
        ADDF(Bldr().table(SVH).priority(100)
             .tcp6().reg(RD, 1).reg(OUTPORT, 1).isFromServiceIface(exposed)
             .isIpv6Dst("fe80::a9:fe:a9:fe").isTpDst(80)
             .actions()
             .ipv6Dst("fe80::a9:fe:a9:2")
             .decTtl()
             .load(SVCADDR1, 0xfe800000)
             .load(SVCADDR2, 0x0)
             .load(SVCADDR3, 0xa900fe)
             .load(SVCADDR4, 0xa900fe)
             .load(CTMARK, exposed?0x80000002:0x2)
             .ct(commit2)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
    } else {
        ADDF(Bldr(SEND_FLOW_REM).table(SVH)
             .cookie(rxCookie1)
             .priority(99)
             .udp().reg(RD, 1)
             .isIpDst("169.254.169.254").isTpDst(53)
             .actions()
             .tpDst(5353).ipDst("10.20.44.2")
             .decTtl()
             .load(SVCADDR1, 0xa9fea9fe)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
        ADDF(Bldr().table(SVH).priority(100)
             .udp().reg(RD, 1).reg(OUTPORT, 1)
             .isIpDst("169.254.169.254").isTpDst(53)
             .actions()
             .tpDst(5353).ipDst("169.254.169.2")
             .decTtl()
             .load(SVCADDR1, 0xa9fea9fe)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
        ADDF(Bldr(SEND_FLOW_REM).table(SVH)
             .cookie(rxCookie2)
             .priority(99)
             .tcp6().reg(RD, 1)
             .isIpv6Dst("fe80::a9:fe:a9:fe").isTpDst(80)
             .actions()
             .ipv6Dst("2001:db8::2")
             .decTtl()
             .load(SVCADDR1, 0xfe800000)
             .load(SVCADDR2, 0x0)
             .load(SVCADDR3, 0xa900fe)
             .load(SVCADDR4, 0xa900fe)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
        ADDF(Bldr().table(SVH).priority(100)
             .tcp6().reg(RD, 1).reg(OUTPORT, 1)
             .isIpv6Dst("fe80::a9:fe:a9:fe").isTpDst(80)
             .actions()
             .ipv6Dst("fe80::a9:fe:a9:2")
             .decTtl()
             .load(SVCADDR1, 0xfe800000)
             .load(SVCADDR2, 0x0)
             .load(SVCADDR3, 0xa900fe)
             .load(SVCADDR4, 0xa900fe)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
    }

    if (conntrack) {
        if (IntFlowManager::ENCAP_VLAN == intFlowManager.getEncapType()) {
            uint32_t tunPort = intFlowManager.getTunnelPort();
            ADDF(Bldr().table(SVR).priority(101)
                 .isCtState("-trk").udp().reg(RD, 1).in(tunPort)
                 .isIpSrc("10.20.44.2").isTpSrc(5353)
                 .actions().pushVlan().move(SEPG12, VLAN)
                 .ct("table=2,zone=1").done());
            ADDF(Bldr().table(SVR).priority(101)
                 .isCtState("-trk").udp().reg(RD, 1).in(tunPort)
                 .isIpSrc("169.254.169.2").isTpSrc(5353)
                 .actions().pushVlan().move(SEPG12, VLAN)
                 .ct("table=2,zone=1").done());
            ADDF(Bldr().table(SVR).priority(101)
                 .isCtState("-trk").tcp6().reg(RD, 1).in(tunPort)
                 .isIpv6Src("2001:db8::2").isTpSrc(80)
                 .actions().pushVlan().move(SEPG12, VLAN)
                 .ct("table=2,zone=1").done());
            ADDF(Bldr().table(SVR).priority(101)
                 .isCtState("-trk").tcp6().reg(RD, 1).in(tunPort)
                 .isIpv6Src("fe80::a9:fe:a9:2").isTpSrc(80)
                 .actions().pushVlan().move(SEPG12, VLAN)
                 .ct("table=2,zone=1").done());
        }
        ADDF(Bldr().table(SVR).priority(100)
             .isCtState("-trk").udp().reg(RD, 1)
             .isIpSrc("10.20.44.2").isTpSrc(5353)
             .actions().ct("table=2,zone=1").done());
        ADDF(Bldr().table(SVR).priority(100)
             .isCtState("-trk").udp().reg(RD, 1)
             .isIpSrc("169.254.169.2").isTpSrc(5353)
             .actions().ct("table=2,zone=1").done());
        ADDF(Bldr().table(SVR).priority(100)
             .isCtState("-trk").tcp6().reg(RD, 1)
             .isIpv6Src("2001:db8::2").isTpSrc(80)
             .actions().ct("table=2,zone=1").done());
        ADDF(Bldr().table(SVR).priority(100)
             .isCtState("-trk").tcp6().reg(RD, 1)
             .isIpv6Src("fe80::a9:fe:a9:2").isTpSrc(80)
             .actions().ct("table=2,zone=1").done());

        if (exposed) {
            ADDF(Bldr(SEND_FLOW_REM).table(SVR)
                 .cookie(txCookie1)
                 .priority(100)
                 .isCtState("-new+est-inv+trk").isCtMark("0x80000001")
                 .udp().reg(RD, 1).isIpSrc("10.20.44.2").isTpSrc(5353)
                 .actions()
                 .tpSrc(53).ethSrc(mac).ipSrc("169.254.169.254")
                 .decTtl().pushVlan().setVlan(4003)
                 .ethDst(rmac).outPort(17).done());
            ADDF(Bldr().table(SVR).priority(100)
                 .isCtState("-new+est-inv+trk").isCtMark("0x80000001")
                 .udp().reg(RD, 1).isIpSrc("169.254.169.2").isTpSrc(5353)
                 .actions()
                 .tpSrc(53).ethSrc(mac).ipSrc("169.254.169.254")
                 .decTtl().pushVlan().setVlan(4003)
                 .ethDst(rmac).outPort(17).done());
            ADDF(Bldr(SEND_FLOW_REM).table(SVR)
                 .cookie(txCookie2)
                 .priority(100)
                 .isCtState("-new+est-inv+trk").isCtMark("0x80000002")
                 .tcp6().reg(RD, 1).isIpv6Src("2001:db8::2").isTpSrc(80)
                 .actions()
                 .ethSrc(mac).ipv6Src("fe80::a9:fe:a9:fe")
                 .decTtl().pushVlan().setVlan(4003)
                 .ethDst(rmac).outPort(17).done());
            ADDF(Bldr().table(SVR).priority(100)
                 .isCtState("-new+est-inv+trk").isCtMark("0x80000002")
                 .tcp6().reg(RD, 1).isIpv6Src("fe80::a9:fe:a9:2").isTpSrc(80)
                 .actions()
                 .ethSrc(mac).ipv6Src("fe80::a9:fe:a9:fe")
                 .decTtl().pushVlan().setVlan(4003)
                 .ethDst(rmac).outPort(17).done());
        } else {
            ADDF(Bldr(SEND_FLOW_REM).table(SVR)
                 .cookie(txCookie1)
                 .priority(100)
                 .isCtState("-new+est-inv+trk").isCtMark("0x1")
                 .udp().reg(RD, 1).isIpSrc("10.20.44.2").isTpSrc(5353)
                 .actions()
                 .tpSrc(53).ethSrc(rmac).ipSrc("169.254.169.254")
                 .decTtl().meta(flow::meta::ROUTED, flow::meta::ROUTED)
                 .go(BR).done());
            ADDF(Bldr().table(SVR).priority(100)
                 .isCtState("-new+est-inv+trk").isCtMark("0x1")
                 .udp().reg(RD, 1).isIpSrc("169.254.169.2").isTpSrc(5353)
                 .actions()
                 .tpSrc(53).ethSrc(rmac).ipSrc("169.254.169.254")
                 .decTtl().meta(flow::meta::ROUTED, flow::meta::ROUTED)
                 .go(BR).done());
            ADDF(Bldr(SEND_FLOW_REM).table(SVR)
                 .cookie(txCookie2)
                 .priority(100)
                 .isCtState("-new+est-inv+trk").isCtMark("0x2")
                 .tcp6().reg(RD, 1).isIpv6Src("2001:db8::2").isTpSrc(80)
                 .actions()
                 .ethSrc(rmac).ipv6Src("fe80::a9:fe:a9:fe")
                 .decTtl().meta(flow::meta::ROUTED, flow::meta::ROUTED)
                 .go(BR).done());
            ADDF(Bldr().table(SVR).priority(100)
                 .isCtState("-new+est-inv+trk").isCtMark("0x2")
                 .tcp6().reg(RD, 1).isIpv6Src("fe80::a9:fe:a9:2").isTpSrc(80)
                 .actions()
                 .ethSrc(rmac).ipv6Src("fe80::a9:fe:a9:fe")
                 .decTtl().meta(flow::meta::ROUTED, flow::meta::ROUTED)
                 .go(BR).done());
        }
    } else {
        if (exposed) {
            ADDF(Bldr(SEND_FLOW_REM).table(SVR)
                 .cookie(txCookie1)
                 .priority(100)
                 .udp().reg(RD, 1)
                 .isIpSrc("10.20.44.2").isTpSrc(5353)
                 .actions()
                 .tpSrc(53).ethSrc(mac).ipSrc("169.254.169.254")
                 .decTtl().pushVlan().setVlan(4003)
                 .ethDst(rmac).outPort(17).done());
            ADDF(Bldr().table(SVR).priority(100)
                 .udp().reg(RD, 1)
                 .isIpSrc("169.254.169.2").isTpSrc(5353)
                 .actions()
                 .tpSrc(53).ethSrc(mac).ipSrc("169.254.169.254")
                 .decTtl().pushVlan().setVlan(4003)
                 .ethDst(rmac).outPort(17).done());
            ADDF(Bldr(SEND_FLOW_REM).table(SVR)
                 .cookie(txCookie2)
                 .priority(100)
                 .tcp6().reg(RD, 1)
                 .isIpv6Src("2001:db8::2").isTpSrc(80)
                 .actions()
                 .ethSrc(mac).ipv6Src("fe80::a9:fe:a9:fe")
                 .decTtl().pushVlan().setVlan(4003)
                 .ethDst(rmac).outPort(17).done());
            ADDF(Bldr().table(SVR).priority(100)
                 .tcp6().reg(RD, 1)
                 .isIpv6Src("fe80::a9:fe:a9:2").isTpSrc(80)
                 .actions()
                 .ethSrc(mac).ipv6Src("fe80::a9:fe:a9:fe")
                 .decTtl().pushVlan().setVlan(4003)
                 .ethDst(rmac).outPort(17).done());
        } else {
            ADDF(Bldr(SEND_FLOW_REM).table(SVR)
                 .cookie(txCookie1)
                 .priority(100)
                 .udp().reg(RD, 1)
                 .isIpSrc("10.20.44.2").isTpSrc(5353)
                 .actions()
                 .tpSrc(53).ethSrc(rmac).ipSrc("169.254.169.254")
                 .decTtl().meta(flow::meta::ROUTED, flow::meta::ROUTED)
                 .go(BR).done());
            ADDF(Bldr().table(SVR).priority(100)
                 .udp().reg(RD, 1)
                 .isIpSrc("169.254.169.2").isTpSrc(5353)
                 .actions()
                 .tpSrc(53).ethSrc(rmac).ipSrc("169.254.169.254")
                 .decTtl().meta(flow::meta::ROUTED, flow::meta::ROUTED)
                 .go(BR).done());
            ADDF(Bldr(SEND_FLOW_REM).table(SVR)
                 .cookie(txCookie2)
                 .priority(100)
                 .tcp6().reg(RD, 1)
                 .isIpv6Src("2001:db8::2").isTpSrc(80)
                 .actions()
                 .ethSrc(rmac).ipv6Src("fe80::a9:fe:a9:fe")
                 .decTtl().meta(flow::meta::ROUTED, flow::meta::ROUTED)
                 .go(BR).done());
            ADDF(Bldr().table(SVR).priority(100)
                 .tcp6().reg(RD, 1)
                 .isIpv6Src("fe80::a9:fe:a9:2").isTpSrc(80)
                 .actions()
                 .ethSrc(rmac).ipv6Src("fe80::a9:fe:a9:fe")
                 .decTtl().meta(flow::meta::ROUTED, flow::meta::ROUTED)
                 .go(BR).done());
        }
    }
}

void BaseIntFlowManagerFixture::initExpVirtualIp() {
    uint32_t port = portmapper.FindPort(ep0->getInterfaceName().get());
    ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::VIRTUAL_IP_V4))
         .table(SEC).priority(60).arp().in(port)
         .isEthSrc("42:42:42:42:42:42").isSpa("42.42.42.42")
         .actions().controller(65535).go(SRC).done());
    ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::VIRTUAL_IP_V4))
         .table(SEC).priority(60).arp().in(port)
         .isEthSrc("42:42:42:42:42:43").isSpa("42.42.42.16/28")
         .actions().controller(65535).go(SRC).done());
    ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::VIRTUAL_IP_V6))
         .table(SEC).priority(60).icmp6().in(port)
         .isEthSrc("42:42:42:42:42:42")
         .icmp_type(136).icmp_code(0).isNdTarget("42::42")
         .actions().controller(65535).go(SRC).done());
    ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::VIRTUAL_IP_V6))
         .table(SEC).priority(60).icmp6().in(port)
         .isEthSrc("42:42:42:42:42:43")
         .icmp_type(136).icmp_code(0).isNdTarget("42::10/124")
         .actions().controller(65535).go(SRC).done());
    ADDF(Bldr().table(SEC).priority(61).arp().in(port)
         .isEthSrc("00:00:00:00:80:00").isSpa("10.20.44.3")
         .actions().go(SRC).done());
    ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::VIRTUAL_IP_V4))
         .table(SEC).priority(60).arp().in(port)
         .isEthSrc("00:00:00:00:80:00").isSpa("10.20.44.3")
         .actions().controller(65535).go(SRC).done());
}

void BaseIntFlowManagerFixture::initExpVirtualDhcp(bool virtIp,
                                                   bool serverOverride) {
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");
    string bmac("ff:ff:ff:ff:ff:ff");
    uint32_t tunPort = intFlowManager.getTunnelPort();

    uint32_t port = portmapper.FindPort(ep0->getInterfaceName().get());
    ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::DHCP_V4))
         .table(SEC).priority(35).udp().in(port)
         .isEthSrc("00:00:00:00:80:00").isTpSrc(68).isTpDst(67)
         .actions().controller(65535).done());

    string serverIp = "169.254.32.32";
    string hexServerIp = "0xa9fe2020";
    string serverMac = "0xaabbccddeeff";
    if (serverOverride) {
        serverIp = "1.2.3.4";
        hexServerIp = "0x1020304";
        serverMac = "0xffbbffddeeff";
    }
    ADDF(Bldr().table(BR).priority(52).arp()
         .reg(BD, 1)
         .reg(RD, 1)
         .in(tunPort)
         .isEthDst(bmac).isTpa(serverIp)
         .isArpOp(1)
         .actions().drop().done());
    ADDF(Bldr().table(BR).priority(51).arp()
         .reg(BD, 1)
         .reg(RD, 1)
         .isEthDst(bmac).isTpa(serverIp)
         .isArpOp(1)
         .actions().move(ETHSRC, ETHDST)
         .load(ETHSRC, serverMac).load(ARPOP, 2)
         .move(ARPSHA, ARPTHA).load(ARPSHA, serverMac)
         .move(ARPSPA, ARPTPA).load(ARPSPA, hexServerIp)
         .inport().done());

    ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::DHCP_V6))
         .table(SEC).priority(35).udp6().in(port)
         .isEthSrc("00:00:00:00:80:00").isTpSrc(546).isTpDst(547)
         .actions().controller(65535).done());
    ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::NEIGH_DISC))
         .table(BR).priority(51).icmp6()
         .reg(BD, 1).reg(RD, 1).isEthDst(mmac)
         .icmp_type(135).icmp_code(0)
         .isNdTarget("fe80::a8bb:ccff:fedd:eeff")
         .actions()
         .load(SEPG, 0xa0a).load64(METADATA, 0x100ffeeddccbbaall)
         .controller(65535).done());

    if (virtIp) {
        ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::DHCP_V4))
             .table(SEC).priority(35).udp().in(port)
             .isEthSrc("42:42:42:42:42:42").isTpSrc(68).isTpDst(67)
             .actions().controller(65535).done());
        ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::DHCP_V4))
             .table(SEC).priority(35).udp().in(port)
             .isEthSrc("42:42:42:42:42:43").isTpSrc(68).isTpDst(67)
             .actions().controller(65535).done());
        ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::DHCP_V6))
             .table(SEC).priority(35).udp6().in(port)
             .isEthSrc("42:42:42:42:42:42").isTpSrc(546).isTpDst(547)
             .actions().controller(65535).done());
        ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::DHCP_V6))
             .table(SEC).priority(35).udp6().in(port)
             .isEthSrc("42:42:42:42:42:43").isTpSrc(546).isTpDst(547)
             .actions().controller(65535).done());
    }
}

void BaseIntFlowManagerFixture::initExpLearningBridge() {
    struct sec {
        uint16_t port;
        uint16_t tci;
        uint16_t mask;
    };

    std::vector<sec> secs {
        {100, 0x1ffd, 0x1fff},
        {100, 0x1400, 0x1f00},
        {101, 0x1ffd, 0x1fff},
        {101, 0x1020, 0x1ff0},
        {102, 0x1ffd, 0x1fff},
        {102, 0x1020, 0x1ff0},
        {102, 0x1400, 0x1f00},
    };

    for (auto& s : secs) {
        ADDF(Bldr().table(SEC).priority(501).in(s.port)
             .isVlanTci(s.tci, s.mask)
             .isEthDst("01:80:c2:00:00:00/ff:ff:ff:ff:ff:f0")
             .actions()
             .drop().done());
        ADDF(Bldr().table(SEC).priority(501).in(s.port)
             .isVlanTci(s.tci, s.mask)
             .isEthSrc("01:00:00:00:00:00/01:00:00:00:00:00")
             .actions()
             .drop().done());

        uint64_t cookie = (uint64_t)s.port << 32 |
            ((uint64_t)s.tci) | ((uint64_t)s.mask << 16);
        ADDF(Bldr().table(SEC).priority(500).in(s.port)
             .isVlanTci(s.tci, s.mask)
             .actions()
             .learn("table=10,idle_timeout=300,delete_learned,cookie="
                    + (boost::format("0x%lx") % (ovs_htonll(cookie))).str() +
                    ",NXM_OF_VLAN_TCI[0..12],"
                    "NXM_OF_ETH_DST[]=NXM_OF_ETH_SRC[],"
                    "output:NXM_OF_IN_PORT[]")
             .go(LRN).done());
    }

    ADDF(Bldr().table(LRN).priority(1)
         .isVlan(4093)
         .actions().outPort(100).outPort(101).outPort(102).done());
    ADDF(Bldr().table(LRN).priority(1)
         .isVlanTci("0x1400/0x1f00")
         .actions().outPort(100).outPort(102).done());
    ADDF(Bldr().table(LRN).priority(1)
         .isVlanTci("0x1020/0x1ff0")
         .actions().outPort(101).outPort(102).done());

}

/**
 * Create group mod entries for use in tests
 */
void
BaseIntFlowManagerFixture::
createGroupEntries(IntFlowManager::EncapType encapType) {
    uint32_t tunPort = intFlowManager.getTunnelPort();
    uint32_t ep0_port = portmapper.FindPort(ep0->getInterfaceName().get());

    /* Group entries */
    ge_fd0 = "group_id=1,type=all";
    ge_fd1 = "group_id=2,type=all";

    boost::format epBktFormat(",bucket=bucket_id:%1%,actions=output:%2%");
    ge_bkt_ep0 = (epBktFormat % ep0_port % ep0_port).str();
    ge_bkt_ep2 = (epBktFormat % ep2_port % ep2_port).str();
    ge_bkt_ep4 = (epBktFormat % ep4_port % ep4_port).str();

    boost::format tunBktFormat;
    switch (encapType) {
    case IntFlowManager::ENCAP_VLAN:
        tunBktFormat =
            boost::format(",bucket=bucket_id:%1%,"
                          "actions=push_vlan:0x8100,"
                          "move:NXM_NX_REG0[0..11]->OXM_OF_VLAN_VID[],"
                          "output:%2%");
        break;
    case IntFlowManager::ENCAP_VXLAN:
    case IntFlowManager::ENCAP_IVXLAN:
    default:
        tunBktFormat =
            boost::format(",bucket=bucket_id:%1%,"
                          "actions=move:NXM_NX_REG0[]->NXM_NX_TUN_ID[0..31],"
                          "move:NXM_NX_REG7[]->NXM_NX_TUN_IPV4_DST[],"
                          "output:%2%");
        break;
    }
    ge_bkt_tun = (tunBktFormat % tunPort % tunPort).str();
    ge_bkt_tun_new = (tunBktFormat % tun_port_new % tun_port_new).str();

    /* Group entries when flooding scope is ENDPOINT_GROUP */
    ge_epg0 = "group_id=1,type=all";
    ge_epg2 = "group_id=2,type=all";
}

void BaseIntFlowManagerFixture::
createOnConnectEntries(IntFlowManager::EncapType encapType,
                       FlowEntryList& flows,
                       GroupEdit::EntryList& groups) {
    // Create a pre-existing table as read from a switch
    flows.clear();
    groups.clear();

    uint32_t epg4_vnid = policyMgr.getVnidForGroup(epg4->getURI()).get();

    FlowBuilder().table(SEC).cookie(ovs_htonll(0xabcd)).build(flows);

    FlowBuilder e1;
    e1.table(SRC).priority(149).inPort(intFlowManager.getTunnelPort());
    switch (encapType) {
    case IntFlowManager::ENCAP_VLAN:
        e1.vlan(epg4_vnid);
        break;
    case IntFlowManager::ENCAP_VXLAN:
    case IntFlowManager::ENCAP_IVXLAN:
    default:
        e1.tunId(epg4_vnid);
        break;
    }
    if (encapType == IntFlowManager::ENCAP_VLAN)
        e1.action().popVlan();

    e1.action()
        .reg(MFF_REG0, epg4_vnid)
        .reg(MFF_REG4, uint32_t(0))
        .reg(MFF_REG5, uint32_t(0))
        .reg(MFF_REG6, uint32_t(1))
        .metadata(0x100, 0x100)
        .go(SVR);
    e1.build(flows);

    FlowBuilder().table(POL).priority(8292)
        .reg(0, epg4_vnid).reg(2, epg4_vnid).build(flows);

    GroupEdit::Entry entryIn(new GroupEdit::GroupMod());
    entryIn->mod->command = OFPGC11_ADD;
    entryIn->mod->group_id = 10;
    groups.push_back(entryIn);

    // Flow mods that we expect after diffing against the table
    fe_connect_1 = Bldr().table(SEC).cookie(0xabcd).actions().drop().done();
    fe_connect_2 = Bldr().table(POL).priority(8292).reg(SEPG, epg4_vnid)
            .reg(DEPG, epg4_vnid).actions().go(STAT).done();
}

// Initialize EPG-scoped flow entries
void BaseIntFlowManagerFixture::initExpSviBD(const URI& sviBDURI,
                                     uint32_t fdId, uint32_t bdId) {
    uint32_t uplink = intFlowManager.getUplinkPort();
    uint32_t vnid = agent.getEndpointManager().getExtEncapId(sviBDURI);
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");
    uint32_t rdId = 0;
    uint32_t fixedVnid = ((1<<30) + vnid);

    ADDF(Bldr().table(BR).priority(10)
         .reg(SEPG, fixedVnid).reg(FD, fdId).isEthDst(mmac).actions()
         .mdAct(flow::meta::out::FLOOD)
         .go(STAT).done());
    ADDF(Bldr().table(BR).priority(2)
         .actions().mdAct(flow::meta::out::TUNNEL)
         .go(STAT).done());
    if(uplink != OFPP_NONE) {
        ADDF(Bldr().table(SRC).priority(149)
             .in(uplink).isVlan(vnid)
             .actions().popVlan()
             .load(SEPG, fixedVnid).load(BD, bdId)
             .load(FD, fdId).load(RD, rdId)
             .polApplied().go(SVR).done());
        ADDF(Bldr().table(OUT).priority(10).reg(SEPG, fixedVnid)
             .isMdAct(flow::meta::out::TUNNEL)
             .actions().pushVlan().move(SEPG12, VLAN)
             .outPort(uplink).done());
    }
    ADDF(Bldr().table(POL).priority(8292).reg(SEPG, fixedVnid)
         .reg(DEPG, fixedVnid).actions().go(STAT).done());
    ADDF(Bldr().table(OUT).priority(10)
         .reg(OUTPORT, fixedVnid).isMdAct(flow::meta::out::RESUBMIT_DST)
         .actions().load(SEPG, fixedVnid).load(BD, bdId)
         .load(FD, fdId).load(RD, rdId)
         .load(OUTPORT, 0).load64(METADATA, flow::meta::ROUTED)
         .resubmit(BR).done());
}

// Initialize endpoint-scoped flow entries
void BaseIntFlowManagerFixture::initExpExtSviEp(shared_ptr<Endpoint>& ep,
                                          uint32_t fdId, uint32_t bdId) {
    uint32_t uplink = intFlowManager.getUplinkPort();
    string mac = ep->getMAC().get().toString();
    uint32_t port = portmapper.FindPort(ep->getInterfaceName().get());
    unordered_set<string> ips(ep->getIPs());
    string lladdr =
        network::construct_link_local_ip_addr(ep->getMAC().get()).to_string();
    ips.insert(lladdr);
    const unordered_set<string>* acastIps = &ep->getAnycastReturnIPs();
    if (acastIps->size() == 0) acastIps = &ips;
    uint32_t vnid = ep->getExtEncapId();
    uint32_t rdId =0;
    uint32_t fixedVnid = ((1<<30) + vnid);

    string bmac("ff:ff:ff:ff:ff:ff");
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    // source rules
    if (port != OFPP_NONE) {
        ADDF(Bldr().table(SEC).priority(20).in(port).isEthSrc(mac)
             .actions().go(SRC).done());
        ADDF(Bldr().table(SEC).priority(30).ipv6().in(port)
                             .isEthSrc(mac).actions().go(SRC).done());
        ADDF(Bldr().table(SEC).priority(30).ip().in(port)
                             .isEthSrc(mac).actions().go(SRC).done());
        for (const string& ip : ips) {
            address ipa = address::from_string(ip);
            if (ipa.is_v4()) {
                ADDF(Bldr().table(SEC).priority(40).arp().in(port)
                     .isEthSrc(mac).isSpa(ip).actions().go(SRC).done());
            } else {
                ADDF(Bldr().table(SEC).priority(40).icmp6().in(port)
                     .isEthSrc(mac).icmp_type(136).icmp_code(0).isNdTarget(ip)
                     .actions().go(SRC).done());
            }
        }

        ADDF(Bldr().table(SRC).priority(140).in(port)
             .isEthSrc(mac).actions().load(SEPG, fixedVnid).load(BD, bdId)
             .load(FD, fdId).load(RD, rdId).go(SVR).done());
    }

    // dest rules
    if (port != OFPP_NONE) {
        // bridge
        ADDF(Bldr().table(BR).priority(10).reg(BD, bdId)
             .isEthDst(mac).actions().load(DEPG, fixedVnid)
             .load(OUTPORT, port).go(POL).done());

    }

    // hairpin output rule
    if (port != OFPP_NONE) {
        ADDF(Bldr().table(OUT).priority(2)
             .reg(OUTPORT, port).isMd("0x400/0x4ff").in(port)
             .actions().inport().done());
        ADDF(Bldr().table(OUT).priority(2)
             .reg(OUTPORT, port).isMd("0x403/0x4ff").in(port)
             .actions().inport().done());
    }

    /* Group entries */
    ge_extsvi1 = "group_id=1,type=all";

    boost::format epBktFormat(",bucket=bucket_id:%1%,actions=output:%2%");
    ge_bkt_ep5 = (epBktFormat % port % port).str();
    if(uplink != OFPP_NONE) {
        boost::format tunBktFormat =
                boost::format(",bucket=bucket_id:%1%,"
                              "actions=push_vlan:0x8100,"
                              "move:NXM_NX_REG0[0..11]->OXM_OF_VLAN_VID[],"
                              "output:%2%");
        ge_bkt_up = (tunBktFormat % uplink % uplink).str();
    }
}

BOOST_FIXTURE_TEST_CASE(extSvi, VxlanIntFlowManagerFixture) {
    setConnected();
    URI extSVIBD("/tenant0/extSvi1");
    if(ep5->getInterfaceName()) {
        portmapper.setPort(ep5->getInterfaceName().get(), 105);
        portmapper.setPort(105, ep5->getInterfaceName().get());
    }
    initExpStatic();
    initExpSviBD(extSVIBD,1,1);
    initExpFd(1);
    initExpExtSviEp(ep5,1,1);
    exec.Clear();
    exec.ExpectGroup(FlowEdit::ADD, ge_extsvi1 + ge_bkt_ep5 + ge_bkt_up);
    intFlowManager.localExternalDomainUpdated(extSVIBD);
    WAIT_FOR(exec.IsGroupEmpty(), 500);
    WAIT_FOR_TABLES("create", 500);

    epSrc.removeEndpoint(ep5->getUUID());
    exec.Clear();
    exec.ExpectGroup(FlowEdit::DEL, ge_extsvi1);
    intFlowManager.endpointUpdated(ep5->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);
    intFlowManager.localExternalDomainUpdated(extSVIBD);
    clearExpFlowTables();
    initExpStatic();
    WAIT_FOR_TABLES("delete", 500);
}

BOOST_FIXTURE_TEST_CASE(extSvi2, VxlanIntFlowManagerFixture) {
    setConnected();
    URI extSVIBD("/tenant0/extSvi1");
    portmapper.erasePort("uplink");
    if(ep5->getInterfaceName()) {
        portmapper.setPort(ep5->getInterfaceName().get(), 105);
        portmapper.setPort(105, ep5->getInterfaceName().get());
    }
    epSrc.updateEndpoint(*ep5);
    initExpStatic();
    initExpSviBD(extSVIBD,1,1);
    initExpFd(1);
    initExpExtSviEp(ep5,1,1);
    exec.Clear();
    exec.ExpectGroup(FlowEdit::ADD, ge_extsvi1 + ge_bkt_ep5);
    intFlowManager.portStatusUpdate("uplink",1024, false);
    WAIT_FOR(exec.IsGroupEmpty(), 500);
    WAIT_FOR_TABLES("create_without_uplink", 500);

    portmapper.setPort("uplink", 1024);
    portmapper.setPort(1024, "uplink");
    initExpStatic();
    initExpSviBD(extSVIBD,1,1);
    initExpFd(1);
    initExpExtSviEp(ep5,1,1);
    exec.Clear();
    exec.ExpectGroup(FlowEdit::MOD, ge_extsvi1 + ge_bkt_up + ge_bkt_ep5);
    intFlowManager.portStatusUpdate("uplink",1024, false);
    WAIT_FOR(exec.IsGroupEmpty(), 500);
    WAIT_FOR_TABLES("uplink_up", 500);

    epSrc.removeEndpoint(ep5->getUUID());
    exec.Clear();
    exec.ExpectGroup(FlowEdit::DEL, ge_extsvi1);
    intFlowManager.endpointUpdated(ep5->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);
    intFlowManager.localExternalDomainUpdated(extSVIBD);
    clearExpFlowTables();
    initExpStatic();
    WAIT_FOR_TABLES("delete_final", 500);
}

BOOST_AUTO_TEST_SUITE_END()
