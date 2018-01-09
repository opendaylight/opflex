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
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <modelgbp/gbp/AddressResModeEnumT.hpp>
#include <modelgbp/gbp/BcastFloodModeEnumT.hpp>
#include <modelgbp/gbp/RoutingModeEnumT.hpp>

#include <opflexagent/logging.h>
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

class IntFlowManagerFixture : public FlowManagerFixture {
public:
    IntFlowManagerFixture()
        : FlowManagerFixture(),
          intFlowManager(agent, switchManager, idGen,
                         ctZoneManager, pktInHandler),
          pktInHandler(agent, intFlowManager),
          policyMgr(agent.getPolicyManager()),
          ep2_port(11), ep4_port(22) {
        createObjects();
        expTables.resize(IntFlowManager::NUM_FLOW_TABLES);

        tunIf = "br0_vxlan0";
        intFlowManager.setEncapIface(tunIf);
        intFlowManager.setTunnel("10.11.12.13", 4789);
        intFlowManager.setVirtualRouter(true, true, "aa:bb:cc:dd:ee:ff");
        intFlowManager.setVirtualDHCP(true, "aa:bb:cc:dd:ee:ff");

        portmapper.ports[ep0->getInterfaceName().get()] = 80;
        portmapper.RPortMap[80] = ep0->getInterfaceName().get();
        portmapper.ports[tunIf] = 2048;
        portmapper.RPortMap[2048] = tunIf;
        tun_port_new = 4096;

        WAIT_FOR(policyMgr.groupExists(epg0->getURI()), 500);
        WAIT_FOR(policyMgr.getBDForGroup(epg0->getURI()) != boost::none, 500);

        WAIT_FOR(policyMgr.groupExists(epg1->getURI()), 500);
        WAIT_FOR(policyMgr.getRDForGroup(epg1->getURI()) != boost::none, 500);

        WAIT_FOR(policyMgr.groupExists(epg3->getURI()), 500);
        WAIT_FOR(policyMgr.getRDForGroup(epg3->getURI()) != boost::none, 500);

        WAIT_FOR(policyMgr.groupExists(epg4->getURI()), 500);
        WAIT_FOR(policyMgr.getRDForGroup(epg4->getURI()) != boost::none, 500);

        switchManager.registerStateHandler(&intFlowManager);
        intFlowManager.enableConnTrack();

        pktInHandler.registerConnection(switchManager.getConnection(), NULL);
        pktInHandler.setPortMapper(&switchManager.getPortMapper(), NULL);
        pktInHandler.setFlowReader(&switchManager.getFlowReader());
        start();
    }
    virtual ~IntFlowManagerFixture() {
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
    void initExpStatic();

    /** Initialize EPG-scoped flow entries */
    void initExpEpg(shared_ptr<EpGroup>& epg,
                    uint32_t fdId = 0, uint32_t bdId = 1, uint32_t rdId = 1,
                    bool isolated = false);

    /** Initialize flood domain-scoped flow entries */
    void initExpFd(uint32_t fdId = 0);

    /** Initialize bridge domain-scoped flow entries */
    void initExpBd(uint32_t bdId = 1, uint32_t rdId = 1,
                   bool routeOn = true);

    /** Initialize routing domain-scoped flow entries */
    void initExpRd(uint32_t rdId = 1);

    /** Initialize endpoint-scoped flow entries */
    void initExpEp(shared_ptr<Endpoint>& ep,
                   shared_ptr<EpGroup>& epg,
                   uint32_t fdId = 0, uint32_t bdId = 1, uint32_t rdId = 1,
                   bool arpOn = true, bool routeOn = true);

    /** Initialize flows related to IP address mapping */
    void initExpIpMapping(bool natEpgMap = true, bool nextHop = false);

    /** Initialize contract 1 flows */
    void initExpCon1();

    /** Initialize contract 2 flows */
    void initExpCon2();

    /** Initialize contract 3 flows */
    void initExpCon3();

    /** Initialize subnet-scoped flow entries */
    void initSubnets(PolicyManager::subnet_vector_t sns,
                     uint32_t bdId = 1, uint32_t rdId = 1);

    /** Initialize service-scoped flow entries for local services */
    void initExpAnycastService(int nextHop = 0);

    /** Initialize service-scoped flow entries for load balanced
        services */
    void initExpLBService(bool conntrack = false, bool remote = false);

    /** Initialize virtual IP flow entries */
    void initExpVirtualIp();

    /** Initialize DHCP flow entries */
    void initExpVirtualDhcp(bool virtIp);

    // Test drivers
    void epgTest();
    void routeModeTest();
    void arpModeTest();
    void fdTest();
    void groupFloodTest();
    void connectTest();
    void portStatusTest();
    void anycastServiceTest();

    IntFlowManager intFlowManager;
    PacketInHandler pktInHandler;
    PolicyManager& policyMgr;

    string tunIf;

    vector<string> fe_connect_learn;
    string fe_connect_1, fe_connect_2;
    string ge_fd0, ge_bkt_ep0, ge_bkt_ep2, ge_bkt_tun;
    string ge_fd0_prom;
    string ge_fd1, ge_bkt_ep4;
    string ge_fd1_prom;
    string ge_bkt_tun_new;
    string ge_epg0, ge_epg0_prom, ge_epg2, ge_epg2_prom;
    uint32_t ep2_port;
    uint32_t ep4_port;
    uint32_t tun_port_new;
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

void IntFlowManagerFixture::epgTest() {
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
    exec.ExpectGroup(FlowEdit::ADD, ge_fd0_prom + ge_bkt_tun);
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

void IntFlowManagerFixture::routeModeTest() {
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

void IntFlowManagerFixture::arpModeTest() {
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
    portmapper.ports[ep0->getInterfaceName().get()] = 180;
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

void IntFlowManagerFixture::fdTest() {
    setConnected();

    /* create */
    portmapper.ports[ep2->getInterfaceName().get()] = ep2_port;
    portmapper.ports[ep4->getInterfaceName().get()] = ep4_port;
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
    exec.ExpectGroup(FlowEdit::ADD, ge_fd0_prom + ge_bkt_tun);

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
    exec.ExpectGroup(FlowEdit::MOD, ge_fd0_prom + ge_bkt_tun);
    intFlowManager.endpointUpdated(ep2->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpStatic();
    initExpFd(1);
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg0, 1, 1, 1);
    WAIT_FOR_TABLES("ep2", 500);

    /* remove port-mapping for ep2 */
    portmapper.ports.erase(ep2->getInterfaceName().get());
    exec.Clear();
    exec.ExpectGroup(FlowEdit::MOD, ge_fd0 + ge_bkt_ep0 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::MOD, ge_fd0_prom + ge_bkt_tun);
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
    exec.ExpectGroup(FlowEdit::DEL, ge_fd0_prom);
    intFlowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpStatic();
    WAIT_FOR_TABLES("removeep", 500);

    /* check promiscous flood */
    WAIT_FOR(policyMgr.getFDForGroup(epg3->getURI()) != boost::none, 500);
    exec.Clear();
    exec.ExpectGroup(FlowEdit::ADD, ge_fd1 + ge_bkt_ep4 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::ADD, ge_fd1_prom + ge_bkt_ep4 + ge_bkt_tun);
    intFlowManager.endpointUpdated(ep4->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    /* group changes on tunnel port change */
    exec.Clear();
    exec.ExpectGroup(FlowEdit::MOD, ge_fd1 + ge_bkt_ep4 + ge_bkt_tun_new);
    exec.ExpectGroup(FlowEdit::MOD, ge_fd1_prom + ge_bkt_ep4 + ge_bkt_tun_new);
    portmapper.ports[tunIf] = tun_port_new;
    intFlowManager.portStatusUpdate(tunIf, tun_port_new, false);
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    /* Clear second group */
    epSrc.removeEndpoint(ep4->getUUID());
    exec.Clear();
    exec.ExpectGroup(FlowEdit::DEL, ge_fd1);
    exec.ExpectGroup(FlowEdit::DEL, ge_fd1_prom);
    intFlowManager.endpointUpdated(ep4->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);
}

BOOST_FIXTURE_TEST_CASE(fd_vxlan, VxlanIntFlowManagerFixture) {
    fdTest();
}

BOOST_FIXTURE_TEST_CASE(fd_vlan, VlanIntFlowManagerFixture) {
    fdTest();
}

void IntFlowManagerFixture::groupFloodTest() {
    intFlowManager.setFloodScope(IntFlowManager::ENDPOINT_GROUP);
    setConnected();

    /* "create" local endpoints ep0 and ep2 */
    portmapper.ports[ep2->getInterfaceName().get()] = ep2_port;
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
    exec.ExpectGroup(FlowEdit::ADD, ge_epg0_prom + ge_bkt_tun);
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
    exec.ExpectGroup(FlowEdit::ADD, ge_epg2_prom + ge_bkt_tun);
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
    exec.ExpectGroup(FlowEdit::DEL, ge_epg0_prom);
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

void IntFlowManagerFixture::connectTest() {
    exec.ignoredFlowMods.insert(FlowEdit::ADD);
    exec.Expect(FlowEdit::DEL, fe_connect_1);
    exec.Expect(FlowEdit::DEL, fe_connect_learn);
    exec.Expect(FlowEdit::MOD, fe_connect_2);
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

void IntFlowManagerFixture::portStatusTest() {
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
    portmapper.ports[tunIf] = tun_port_new;
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
    portmapper.ports.erase(tunIf);
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
    } catch (pt::json_parser_error e) {
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
    portmapper.ports["nexthop"] = 42;
    portmapper.RPortMap[42] = "nexthop";
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

BOOST_FIXTURE_TEST_CASE(anycastService_vxlan, VxlanIntFlowManagerFixture) {
	anycastServiceTest();
}

BOOST_FIXTURE_TEST_CASE(anycastService_vlan, VlanIntFlowManagerFixture) {
	anycastServiceTest();
}

void IntFlowManagerFixture::anycastServiceTest() {
    setConnected();
    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());
    portmapper.ports["service-iface"] = 17;
    portmapper.RPortMap[17] = "service-iface";

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
    initExpAnycastService();
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
    initExpAnycastService(1);
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
    initExpAnycastService(2);
    WAIT_FOR_TABLES("nexthop lb", 500);
}

BOOST_FIXTURE_TEST_CASE(loadBalancedService, VxlanIntFlowManagerFixture) {
    setConnected();
    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    Service as;
    as.setUUID("ed84daef-1696-4b98-8c80-6b22d85f4dc2");
    as.setDomainURI(URI(rd0->getURI()));
    as.setServiceMode(Service::LOADBALANCER);

    Service::ServiceMapping sm1;
    sm1.setServiceIP("169.254.169.254");
    sm1.setServiceProto("udp");
    sm1.addNextHopIP("169.254.169.1");
    sm1.addNextHopIP("169.254.169.2");
    sm1.setServicePort(53);
    sm1.setNextHopPort(5353);
    as.addServiceMapping(sm1);

    Service::ServiceMapping sm2;
    sm2.setServiceIP("fe80::a9:fe:a9:fe");
    sm2.setServiceProto("tcp");
    sm2.addNextHopIP("fe80::a9:fe:a9:1");
    sm2.addNextHopIP("fe80::a9:fe:a9:2");
    sm2.setServicePort(80);
    as.addServiceMapping(sm2);

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
    initExpLBService();
    WAIT_FOR_TABLES("create", 500);

    as.clearServiceMappings();
    sm1.setConntrackMode(true);
    as.addServiceMapping(sm1);
    sm2.setConntrackMode(true);
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
    initExpLBService(true);
    WAIT_FOR_TABLES("conntrack", 500);

    portmapper.ports["service-iface"] = 17;
    portmapper.RPortMap[17] = "service-iface";
    as.setServiceMAC(MAC("13:37:13:37:13:37"));
    as.setInterfaceName("service-iface");
    as.setIfaceIP("1.1.1.1");
    as.setIfaceVlan(4003);

    // TODO
    //Service asv6;
    //asv6.setUUID("ed84daef-1696-4b98-8c80-6b22d85f4dc3");
    //asv6.setDomainURI(URI(rd0->getURI()));
    //asv6.setServiceMode(Service::LOADBALANCER);

    as.clearServiceMappings();
    sm1.setConntrackMode(false);
    as.addServiceMapping(sm1);
    sm2.setConntrackMode(false);
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
    initExpLBService(false, true);
    WAIT_FOR_TABLES("exposed", 500);

    as.clearServiceMappings();
    sm1.setConntrackMode(true);
    as.addServiceMapping(sm1);
    sm2.setConntrackMode(true);
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
    initExpLBService(true, true);
    WAIT_FOR_TABLES("exposed conntrack", 500);
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
    initExpVirtualDhcp(false);
    WAIT_FOR_TABLES("create", 500);

    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:42"), "42.42.42.42"));
    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:43"), "42.42.42.16/28"));
    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:42"), "42::42"));
    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:43"), "42::10/124"));
    ep0->addVirtualIP(make_pair(MAC("00:00:00:00:80:00"), "10.20.44.3"));
    epSrc.updateEndpoint(*ep0);
    intFlowManager.endpointUpdated(ep0->getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpVirtualIp();
    initExpVirtualDhcp(true);
    WAIT_FOR_TABLES("virtip", 500);
}

#define ADDF(flow) addExpFlowEntry(expTables, flow)
enum TABLE {
    SEC, SRC, SVR, BR, SVH, RT, NAT, LRN, SVD, POL, OUT
};

void IntFlowManagerFixture::initExpStatic() {
    uint32_t tunPort = intFlowManager.getTunnelPort();
    uint8_t rmacArr[6];
    memcpy(rmacArr, intFlowManager.getRouterMacAddr(), sizeof(rmacArr));
    string rmac = MAC(rmacArr).toString();

    ADDF(Bldr().table(SEC).priority(25).arp().actions().drop().done());
    ADDF(Bldr().table(SEC).priority(25).ip().actions().drop().done());
    ADDF(Bldr().table(SEC).priority(25).ipv6().actions().drop().done());
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
         .actions().go(OUT).done());
    ADDF(Bldr().table(POL).priority(10).arp().actions().go(OUT).done());
    ADDF(Bldr().table(POL).priority(10).icmp6().icmp_type(135).icmp_code(0)
         .actions().go(OUT).done());
    ADDF(Bldr().table(POL).priority(10).icmp6().icmp_type(136).icmp_code(0)
         .actions().go(OUT).done());
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
        ADDF(Bldr().table(BR).priority(1)
             .actions().mdAct(flow::meta::out::TUNNEL)
             .go(OUT).done());
        ADDF(Bldr().table(RT).priority(1)
             .actions().mdAct(flow::meta::out::TUNNEL)
             .go(OUT).done());
    }
}

// Initialize EPG-scoped flow entries
void IntFlowManagerFixture::initExpEpg(shared_ptr<EpGroup>& epg,
                                    uint32_t fdId, uint32_t bdId,
                                    uint32_t rdId, bool isolated) {
    IntFlowManager::EncapType encapType = intFlowManager.getEncapType();
    uint32_t tunPort = intFlowManager.getTunnelPort();
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
                 .go(OUT).done());
        } else {
            ADDF(Bldr().table(BR).priority(10)
                 .reg(SEPG, vnid).reg(FD, fdId).isEthDst(mmac).actions()
                 .mdAct(flow::meta::out::FLOOD)
                 .go(OUT).done());
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
                 .go(OUT).done());
        } else {
            ADDF(Bldr().table(BR).priority(10)
                 .reg(SEPG, vnid).reg(FD, fdId).isEthDst(mmac).actions()
                 .load(OUTPORT, mcast.to_v4().to_ulong())
                 .mdAct(flow::meta::out::FLOOD)
                 .go(OUT).done());
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
            ADDF(Bldr().table(OUT).priority(10).reg(SEPG, vnid)
                 .isMdAct(flow::meta::out::TUNNEL)
                 .actions().move(SEPG, TUNID)
                 .load(TUNDST, mcast.to_v4().to_ulong())
                 .outPort(tunPort).done());
            ADDF(Bldr().table(OUT).priority(11).reg(SEPG, vnid)
                 .isMdAct(flow::meta::out::TUNNEL)
                 .isEthDst(rmac)
                 .actions().move(SEPG, TUNID)
                 .load(TUNDST, intFlowManager.getTunnelDst().to_v4().to_ulong())
                 .outPort(tunPort).done());
            break;
        }
    }
    ADDF(Bldr().table(POL).priority(8292).reg(SEPG, vnid)
         .reg(DEPG, vnid).actions().go(OUT).done());
    ADDF(Bldr().table(OUT).priority(10)
         .reg(OUTPORT, vnid).isMdAct(flow::meta::out::RESUBMIT_DST)
         .actions().load(SEPG, vnid).load(BD, bdId)
         .load(FD, fdId).load(RD, rdId)
         .load(OUTPORT, 0).load64(METADATA, flow::meta::ROUTED)
         .resubmit(BR).done());
}

// Initialize flood domain-scoped flow entries
void IntFlowManagerFixture::initExpFd(uint32_t fdId) {
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    if (fdId != 0) {
        ADDF(Bldr().table(OUT).priority(10).reg(FD, fdId)
             .isMdAct(flow::meta::out::FLOOD)
             .actions().group(fdId).done());
    }
}

// Initialize bridge domain-scoped flow entries
void IntFlowManagerFixture::initExpBd(uint32_t bdId, uint32_t rdId,
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
void IntFlowManagerFixture::initExpRd(uint32_t rdId) {
    uint32_t tunPort = intFlowManager.getTunnelPort();

    if (tunPort != OFPP_NONE) {
        ADDF(Bldr().table(RT).priority(324)
             .ip().reg(RD, rdId).isIpDst("10.20.44.0/24")
             .actions().mdAct(flow::meta::out::TUNNEL)
             .go(OUT).done());
        ADDF(Bldr().table(RT).priority(324)
             .ip().reg(RD, rdId).isIpDst("10.20.45.0/24")
             .actions().mdAct(flow::meta::out::TUNNEL)
             .go(OUT).done());
        ADDF(Bldr().table(RT).priority(332)
             .ipv6().reg(RD, rdId).isIpv6Dst("2001:db8::/32")
             .actions().mdAct(flow::meta::out::TUNNEL)
             .go(OUT).done());
    } else {
        ADDF(Bldr().table(RT).priority(324)
             .ip().reg(RD, rdId).isIpDst("10.20.44.0/24")
             .actions().drop().done());
        ADDF(Bldr().table(RT).priority(324)
             .ip().reg(RD, rdId).isIpDst("10.20.45.0/24")
             .actions().drop().done());
        ADDF(Bldr().table(RT).priority(332)
             .ipv6().reg(RD, rdId).isIpv6Dst("2001:db8::/32")
             .actions().drop().done());
    }
    ADDF(Bldr().table(POL).priority(1).reg(RD, rdId).actions().drop().done());
}

// Initialize endpoint-scoped flow entries
void IntFlowManagerFixture::initExpEp(shared_ptr<Endpoint>& ep,
                                      shared_ptr<EpGroup>& epg,
                                      uint32_t fdId, uint32_t bdId,
                                      uint32_t rdId, bool arpOn, bool routeOn) {
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

void IntFlowManagerFixture::initSubnets(PolicyManager::subnet_vector_t sns,
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

void IntFlowManagerFixture::initExpCon1() {
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
            const opflex::modb::URI& ruleURI_1 = classifier1.get()->getURI();
            uint32_t con1_cookie = intFlowManager.getId(
                         classifier1->getClassId(), ruleURI_1);
            ADDF(Bldr("", SEND_FLOW_REM).table(POL)
                 .priority(prio)
                 .cookie(con1_cookie).tcp()
                 .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpDst(80)
                 .actions()
                 .go(OUT)
                 .done());
            /* classifier 2  */
            const opflex::modb::URI& ruleURI_2 = classifier2.get()->getURI();
            con1_cookie = intFlowManager.getId(classifier2->getClassId(),
                                               ruleURI_2);
            ADDF(Bldr("", SEND_FLOW_REM).table(POL)
                 .priority(prio-128)
                 .cookie(con1_cookie).arp()
                 .reg(SEPG, pvnid).reg(DEPG, cvnid)
                 .actions().go(OUT).done());
            /* classifier 6 */
            const opflex::modb::URI& ruleURI_6 = classifier6.get()->getURI();
            con1_cookie = intFlowManager.getId(classifier6->getClassId(),
                                               ruleURI_6);
            ADDF(Bldr("", SEND_FLOW_REM).table(POL)
                 .priority(prio-256)
                 .cookie(con1_cookie).tcp()
                 .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpSrc(22)
                 .isTcpFlags("+syn+ack").actions().go(OUT).done());
            /* classifier 7 */
            const opflex::modb::URI& ruleURI_7 = classifier7.get()->getURI();
            con1_cookie = intFlowManager.getId(classifier7->getClassId(),
                                               ruleURI_7);
            ADDF(Bldr("", SEND_FLOW_REM).table(POL)
                 .priority(prio-384)
                 .cookie(con1_cookie).tcp()
                 .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpSrc(21)
                 .isTcpFlags("+ack").actions().go(OUT).done());
            ADDF(Bldr("", SEND_FLOW_REM).table(POL)
                 .priority(prio-384)
                 .cookie(con1_cookie).tcp()
                 .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpSrc(21)
                 .isTcpFlags("+rst").actions().go(OUT).done());
        }
    }
}

void IntFlowManagerFixture::initExpCon2() {
    uint16_t prio = PolicyManager::MAX_POLICY_RULE_PRIORITY;
    PolicyManager::uri_set_t ps, cs;
    unordered_set<uint32_t> ivnids;
    const opflex::modb::URI& ruleURI_5 = classifier5.get()->getURI();
    uint32_t con2_cookie = intFlowManager.getId(
                         classifier5->getClassId(), ruleURI_5);

    policyMgr.getContractIntra(con2->getURI(), ps);
    intFlowManager.getGroupVnid(ps, ivnids);
    for (const uint32_t& ivnid : ivnids) {
        ADDF(Bldr().table(POL).priority(prio).cookie(con2_cookie)
             .reg(SEPG, ivnid).reg(DEPG, ivnid).isEth(0x8906)
             .actions().go(OUT).done());
    }
}

void IntFlowManagerFixture::initExpCon3() {
    uint32_t epg0_vnid = policyMgr.getVnidForGroup(epg0->getURI()).get();
    uint32_t epg1_vnid = policyMgr.getVnidForGroup(epg1->getURI()).get();
    uint16_t prio = PolicyManager::MAX_POLICY_RULE_PRIORITY;
    PolicyManager::uri_set_t ps, cs;

    const opflex::modb::URI& ruleURI_3 = classifier3.get()->getURI();
    uint32_t con3_cookie = intFlowManager.getId(
                         classifier3->getClassId(), ruleURI_3);
    const opflex::modb::URI& ruleURI_4 = classifier4.get()->getURI();
    uint32_t con4_cookie = intFlowManager.getId(
                         classifier4->getClassId(), ruleURI_4);
    const opflex::modb::URI& ruleURI_10 = classifier10.get()->getURI();
    uint32_t con10_cookie = intFlowManager.getId(
        classifier10->getClassId(), ruleURI_10);
    MaskList ml_80_85 = list_of<Mask>(0x0050, 0xfffc)(0x0054, 0xfffe);
    MaskList ml_66_69 = list_of<Mask>(0x0042, 0xfffe)(0x0044, 0xfffe);
    MaskList ml_94_95 = list_of<Mask>(0x005e, 0xfffe);
    for (const Mask& mk : ml_80_85) {
        ADDF(Bldr("", SEND_FLOW_REM).table(POL).priority(prio)
             .cookie(con3_cookie).tcp()
             .reg(SEPG, epg1_vnid).reg(DEPG, epg0_vnid)
             .isTpDst(mk.first, mk.second).actions().drop().done());
    }
    for (const Mask& mks : ml_66_69) {
        for (const Mask& mkd : ml_94_95) {
            ADDF(Bldr("", SEND_FLOW_REM).table(POL).priority(prio-128)
                 .cookie(con4_cookie).tcp()
                 .reg(SEPG, epg1_vnid).reg(DEPG, epg0_vnid)
                 .isTpSrc(mks.first, mks.second).isTpDst(mkd.first, mkd.second)
                 .actions().go(OUT).done());
        }
    }
    ADDF(Bldr("", SEND_FLOW_REM).table(POL).priority(prio-256)
        .cookie(con10_cookie).icmp().reg(SEPG, epg1_vnid).reg(DEPG, epg0_vnid)
        .icmp_type(10).icmp_code(5).actions().go(OUT).done());
}

// Initialize flows related to IP address mapping/NAT
void IntFlowManagerFixture::initExpIpMapping(bool natEpgMap, bool nextHop) {
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
             .go(OUT).done());
        ADDF(Bldr().table(RT).priority(158).ip().reg(RD, 1)
             .isIpDst("5.0.0.0/8")
             .actions().mdAct(flow::meta::out::TUNNEL)
             .go(OUT).done());
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

void IntFlowManagerFixture::initExpAnycastService(int nextHop) {
    string mac = "ed:84:da:ef:16:96";
    string bmac("ff:ff:ff:ff:ff:ff");
    uint8_t rmacArr[6];
    memcpy(rmacArr, intFlowManager.getRouterMacAddr(), sizeof(rmacArr));
    string rmac = MAC(rmacArr).toString();
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");
    bool vlan = (IntFlowManager::ENCAP_VLAN == intFlowManager.getEncapType());

    if (nextHop) {
        std::stringstream mss;
        mss << "symmetric_l3l4+udp,1024,iter_hash,"
            << nextHop << ",32,NXM_NX_REG7[]";
        Bldr nhb = Bldr().table(BR).priority(50).ip().reg(RD, 1)
            .isIpDst("169.254.169.254").actions();
        if (vlan) {
            nhb.popVlan();
        }
        ADDF(nhb.ethSrc(rmac).ethDst(mac).multipath(mss.str()).go(SVH).done());
        nhb = Bldr().table(BR).priority(50).ipv6().reg(RD, 1)
            .isIpv6Dst("fe80::a9:fe:a9:fe").actions();
        if (vlan) {
            nhb.popVlan();
        }
        ADDF(nhb.ethSrc(rmac).ethDst(mac).multipath(mss.str()).go(SVH).done());

        ADDF(Bldr().table(SVH).priority(99)
             .ip().reg(RD, 1)
             .isIpDst("169.254.169.254")
             .actions()
             .ipDst("169.254.169.1")
             .decTtl().outPort(17).done());
        ADDF(Bldr().table(SVH).priority(99)
             .ipv6().reg(RD, 1)
             .isIpv6Dst("fe80::a9:fe:a9:fe")
             .actions()
             .ipv6Dst("fe80::a9:fe:a9:1")
             .decTtl().outPort(17).done());

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
                 .ipDst("169.254.169.2")
                 .decTtl().outPort(17).done());
            ADDF(Bldr().table(SVH).priority(100)
                 .ipv6().reg(RD, 1).reg(OUTPORT, 1)
                 .isIpv6Dst("fe80::a9:fe:a9:fe")
                 .actions()
                 .ipv6Dst("fe80::a9:fe:a9:2")
                 .decTtl().outPort(17).done());

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
        Bldr nhb = Bldr().table(BR).priority(50).ip().reg(RD, 1)
            .isIpDst("169.254.169.254").actions();
        if (vlan) {
            nhb.popVlan();
        }
        ADDF(nhb.ethSrc(rmac).ethDst(mac).decTtl().outPort(17).done());
        nhb = Bldr().table(BR).priority(50).ipv6().reg(RD, 1)
           .isIpv6Dst("fe80::a9:fe:a9:fe").actions();
        if (vlan) {
           nhb.popVlan();
        }
        ADDF(nhb.ethSrc(rmac).ethDst(mac).decTtl().outPort(17).done());

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
         .load64(METADATA, 0x101ffeeddccbbaall)
         .controller(65535).done());
}

void IntFlowManagerFixture::initExpLBService(bool conntrack, bool exposed) {
    string mac = "13:37:13:37:13:37";
    string bmac("ff:ff:ff:ff:ff:ff");
    uint8_t rmacArr[6];
    memcpy(rmacArr, intFlowManager.getRouterMacAddr(), sizeof(rmacArr));
    string rmac = MAC(rmacArr).toString();

    if (exposed) {
        ADDF(Bldr().table(SEC).priority(90).in(17).isVlan(4003)
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
         .ethSrc(rmac).ethDst(rmac)
         .multipath("symmetric_l3l4+udp,1024,iter_hash,2,32,NXM_NX_REG7[]")
         .go(SVH).done());
    ADDF(expIsEthDst(Bldr().table(BR).priority(50)
                     .tcp6().reg(RD, 1))
         .isIpv6Dst("fe80::a9:fe:a9:fe").isTpDst(80)
         .actions()
         .ethSrc(rmac).ethDst(rmac)
         .multipath("symmetric_l3l4+udp,1024,iter_hash,2,32,NXM_NX_REG7[]")
         .go(SVH).done());

    if (conntrack) {
        string commit = string("commit,zone=1,exec(load:") +
            (exposed ? "0x80000001" : "0x1") + "->NXM_NX_CT_MARK[])";

        ADDF(Bldr().table(SVH).priority(99)
             .udp().reg(RD, 1).isFromServiceIface(exposed)
             .isIpDst("169.254.169.254").isTpDst(53)
             .actions()
             .tpDst(5353).ipDst("169.254.169.1")
             .decTtl()
             .ct(commit).meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
        ADDF(Bldr().table(SVH).priority(100)
             .udp().reg(RD, 1).reg(OUTPORT, 1).isFromServiceIface(exposed)
             .isIpDst("169.254.169.254").isTpDst(53)
             .actions()
             .tpDst(5353).ipDst("169.254.169.2")
             .decTtl().ct(commit)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
        ADDF(Bldr().table(SVH).priority(99)
             .tcp6().reg(RD, 1).isFromServiceIface(exposed)
             .isIpv6Dst("fe80::a9:fe:a9:fe").isTpDst(80)
             .actions()
             .ipv6Dst("fe80::a9:fe:a9:1")
             .decTtl().ct(commit)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
        ADDF(Bldr().table(SVH).priority(100)
             .tcp6().reg(RD, 1).reg(OUTPORT, 1).isFromServiceIface(exposed)
             .isIpv6Dst("fe80::a9:fe:a9:fe").isTpDst(80)
             .actions()
             .ipv6Dst("fe80::a9:fe:a9:2")
             .decTtl().ct(commit)
             .meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
    } else {
        ADDF(Bldr().table(SVH).priority(99)
             .udp().reg(RD, 1)
             .isIpDst("169.254.169.254").isTpDst(53)
             .actions()
             .tpDst(5353).ipDst("169.254.169.1")
             .decTtl().meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
        ADDF(Bldr().table(SVH).priority(100)
             .udp().reg(RD, 1).reg(OUTPORT, 1)
             .isIpDst("169.254.169.254").isTpDst(53)
             .actions()
             .tpDst(5353).ipDst("169.254.169.2")
             .decTtl().meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
        ADDF(Bldr().table(SVH).priority(99)
             .tcp6().reg(RD, 1)
             .isIpv6Dst("fe80::a9:fe:a9:fe").isTpDst(80)
             .actions()
             .ipv6Dst("fe80::a9:fe:a9:1")
             .decTtl().meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
        ADDF(Bldr().table(SVH).priority(100)
             .tcp6().reg(RD, 1).reg(OUTPORT, 1)
             .isIpv6Dst("fe80::a9:fe:a9:fe").isTpDst(80)
             .actions()
             .ipv6Dst("fe80::a9:fe:a9:2")
             .decTtl().meta(flow::meta::ROUTED, flow::meta::ROUTED)
             .go(RT).done());
    }

    if (conntrack) {
        ADDF(Bldr().table(SVR).priority(100)
             .isCtState("-trk").udp().reg(RD, 1)
             .isIpSrc("169.254.169.1").isTpSrc(5353)
             .actions().ct("table=1,zone=1").done());
        ADDF(Bldr().table(SVR).priority(100)
             .isCtState("-trk").udp().reg(RD, 1)
             .isIpSrc("169.254.169.2").isTpSrc(5353)
             .actions().ct("table=1,zone=1").done());
        ADDF(Bldr().table(SVR).priority(100)
             .isCtState("-trk").tcp6().reg(RD, 1)
             .isIpv6Src("fe80::a9:fe:a9:1").isTpSrc(80)
             .actions().ct("table=1,zone=1").done());
        ADDF(Bldr().table(SVR).priority(100)
             .isCtState("-trk").tcp6().reg(RD, 1)
             .isIpv6Src("fe80::a9:fe:a9:2").isTpSrc(80)
             .actions().ct("table=1,zone=1").done());

        if (exposed) {
            ADDF(Bldr().table(SVR).priority(100)
                 .isCtState("-new+est-inv+trk").isCtMark("0x80000001")
                 .udp().reg(RD, 1).isIpSrc("169.254.169.1").isTpSrc(5353)
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
            ADDF(Bldr().table(SVR).priority(100)
                 .isCtState("-new+est-inv+trk").isCtMark("0x80000001")
                 .tcp6().reg(RD, 1).isIpv6Src("fe80::a9:fe:a9:1").isTpSrc(80)
                 .actions()
                 .ethSrc(mac).ipv6Src("fe80::a9:fe:a9:fe")
                 .decTtl().pushVlan().setVlan(4003)
                 .ethDst(rmac).outPort(17).done());
            ADDF(Bldr().table(SVR).priority(100)
                 .isCtState("-new+est-inv+trk").isCtMark("0x80000001")
                 .tcp6().reg(RD, 1).isIpv6Src("fe80::a9:fe:a9:2").isTpSrc(80)
                 .actions()
                 .ethSrc(mac).ipv6Src("fe80::a9:fe:a9:fe")
                 .decTtl().pushVlan().setVlan(4003)
                 .ethDst(rmac).outPort(17).done());
        } else {
            ADDF(Bldr().table(SVR).priority(100)
                 .isCtState("-new+est-inv+trk").isCtMark("0x1")
                 .udp().reg(RD, 1).isIpSrc("169.254.169.1").isTpSrc(5353)
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
            ADDF(Bldr().table(SVR).priority(100)
                 .isCtState("-new+est-inv+trk").isCtMark("0x1")
                 .tcp6().reg(RD, 1).isIpv6Src("fe80::a9:fe:a9:1").isTpSrc(80)
                 .actions()
                 .ethSrc(rmac).ipv6Src("fe80::a9:fe:a9:fe")
                 .decTtl().meta(flow::meta::ROUTED, flow::meta::ROUTED)
                 .go(BR).done());
            ADDF(Bldr().table(SVR).priority(100)
                 .isCtState("-new+est-inv+trk").isCtMark("0x1")
                 .tcp6().reg(RD, 1).isIpv6Src("fe80::a9:fe:a9:2").isTpSrc(80)
                 .actions()
                 .ethSrc(rmac).ipv6Src("fe80::a9:fe:a9:fe")
                 .decTtl().meta(flow::meta::ROUTED, flow::meta::ROUTED)
                 .go(BR).done());
        }
    } else {
        if (exposed) {
            ADDF(Bldr().table(SVR).priority(100)
                 .udp().reg(RD, 1)
                 .isIpSrc("169.254.169.1").isTpSrc(5353)
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
            ADDF(Bldr().table(SVR).priority(100)
                 .tcp6().reg(RD, 1)
                 .isIpv6Src("fe80::a9:fe:a9:1").isTpSrc(80)
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
            ADDF(Bldr().table(SVR).priority(100)
                 .udp().reg(RD, 1)
                 .isIpSrc("169.254.169.1").isTpSrc(5353)
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
            ADDF(Bldr().table(SVR).priority(100)
                 .tcp6().reg(RD, 1)
                 .isIpv6Src("fe80::a9:fe:a9:1").isTpSrc(80)
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

void IntFlowManagerFixture::initExpVirtualIp() {
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

void IntFlowManagerFixture::initExpVirtualDhcp(bool virtIp) {
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");
    string bmac("ff:ff:ff:ff:ff:ff");

    uint32_t port = portmapper.FindPort(ep0->getInterfaceName().get());
    ADDF(Bldr().cookie(ovs_ntohll(flow::cookie::DHCP_V4))
         .table(SEC).priority(35).udp().in(port)
         .isEthSrc("00:00:00:00:80:00").isTpSrc(68).isTpDst(67)
         .actions().controller(65535).done());
    ADDF(Bldr().table(BR).priority(51).arp()
         .reg(BD, 1)
         .reg(RD, 1)
         .isEthDst(bmac).isTpa("169.254.32.32")
         .isArpOp(1)
         .actions().move(ETHSRC, ETHDST)
         .load(ETHSRC, "0xaabbccddeeff").load(ARPOP, 2)
         .move(ARPSHA, ARPTHA).load(ARPSHA, "0xaabbccddeeff")
         .move(ARPSPA, ARPTPA).load(ARPSPA, "0xa9fe2020")
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

/**
 * Create group mod entries for use in tests
 */
void
IntFlowManagerFixture::createGroupEntries(IntFlowManager::EncapType encapType) {
    uint32_t tunPort = intFlowManager.getTunnelPort();
    uint32_t ep0_port = portmapper.FindPort(ep0->getInterfaceName().get());

    /* Group entries */
    string bktInit = ",bucket=";
    ge_fd0 = "group_id=1,type=all";
    ge_bkt_ep0 = Bldr(bktInit).bktId(ep0_port).bktActions().outPort(ep0_port)
            .done();
    ge_bkt_ep2 = Bldr(bktInit).bktId(ep2_port).bktActions().outPort(ep2_port)
            .done();
    switch (encapType) {
    case IntFlowManager::ENCAP_VLAN:
        ge_bkt_tun = Bldr(bktInit).bktId(tunPort).bktActions()
            .pushVlan().move(SEPG12, VLAN).outPort(tunPort).done();
        break;
    case IntFlowManager::ENCAP_VXLAN:
    case IntFlowManager::ENCAP_IVXLAN:
    default:
        ge_bkt_tun = Bldr(bktInit).bktId(tunPort).bktActions()
            .move(SEPG, TUNID).move(OUTPORT, TUNDST).outPort(tunPort).done();
        break;
    }
    ge_bkt_tun_new = Bldr(ge_bkt_tun).bktId(tun_port_new)
                     .outPort(tun_port_new).done();
    ge_fd0_prom = "group_id=2147483649,type=all";

    ge_bkt_ep4 = Bldr(bktInit).bktId(ep4_port).bktActions().outPort(ep4_port)
            .done();
    ge_fd1 = "group_id=2,type=all";
    ge_fd1_prom = "group_id=2147483650,type=all";

    /* Group entries when flooding scope is ENDPOINT_GROUP */
    ge_epg0 = "group_id=1,type=all";
    ge_epg0_prom = "group_id=2147483649,type=all";
    ge_epg2 = "group_id=2,type=all";
    ge_epg2_prom = "group_id=2147483650,type=all";
}

void IntFlowManagerFixture::
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

    // invalid learn entry with invalid mac and port
    FlowBuilder().table(LRN).priority(150)
        .cookie(flow::cookie::LEARN)
        .ethDst(MAC("de:ad:be:ef:1:2"))
        .action().reg(MFF_REG7, 999)
        .controller().parent().build(flows);

    // valid learn entry for ep0
    FlowBuilder().table(LRN).priority(150)
        .cookie(flow::cookie::LEARN)
        .ethDst(ep0->getMAC().get())
        .action().reg(MFF_REG7, 80)
        .controller().parent().build(flows);

    // invalid learn entry with invalid mac and valid port
    FlowBuilder().table(LRN).priority(150)
        .cookie(flow::cookie::LEARN)
        .ethDst(MAC("de:ad:be:ef:1:3"))
        .action().reg(MFF_REG7, 80)
        .controller().parent().build(flows);

    // invalid learn entry with invalid port and valid mac
    FlowBuilder().table(LRN).priority(150)
        .cookie(flow::cookie::LEARN)
        .ethDst(ep1->getMAC().get())
        .action().reg(MFF_REG7, 999)
        .controller().parent().build(flows);

    // spurious entry in learn table, should be deleted
    FlowBuilder().table(LRN).priority(8192)
        .cookie(ovs_htonll(0xabcd)).build(flows);

    GroupEdit::Entry entryIn(new GroupEdit::GroupMod());
    entryIn->mod->command = OFPGC11_ADD;
    entryIn->mod->group_id = 10;
    groups.push_back(entryIn);

    // Flow mods that we expect after diffing against the table
    fe_connect_1 = Bldr().table(SEC).priority(0).cookie(0xabcd)
            .actions().drop().done();
    fe_connect_2 = Bldr().table(POL).priority(8292).reg(SEPG, epg4_vnid)
            .reg(DEPG, epg4_vnid).actions().go(OUT).done();

    fe_connect_learn
        .push_back(Bldr().table(LRN).priority(8192).cookie(0xabcd)
                   .actions().drop().done());
    fe_connect_learn
        .push_back(Bldr().table(LRN).priority(150)
                   .cookie(ovs_ntohll(flow::cookie::LEARN))
                   .isEthDst(ep1->getMAC().get().toString())
                   .actions().load(OUTPORT, 999).controller(0xffff)
                   .done());
    fe_connect_learn
        .push_back(Bldr().table(LRN).priority(150)
                   .cookie(ovs_ntohll(flow::cookie::LEARN))
                   .isEthDst("de:ad:be:ef:01:02")
                   .actions().load(OUTPORT, 999).controller(0xffff)
                   .done());
    fe_connect_learn
        .push_back(Bldr().table(LRN).priority(150)
                   .cookie(ovs_ntohll(flow::cookie::LEARN))
                   .isEthDst("de:ad:be:ef:01:03")
                   .actions().load(OUTPORT, 80).controller(0xffff)
                   .done());

}

BOOST_AUTO_TEST_SUITE_END()
