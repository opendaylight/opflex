/*
 * Test suite for class VppManager
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
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

#include "logging.h"
#include "VppManager.h"

#include "ModbFixture.h"
#include "RangeMask.h"

#include "SwitchManager.h"
#include "MockPortMapper.h"
#include "CtZoneManager.h"

using namespace boost::assign;
using namespace opflex::modb;
using namespace modelgbp::gbp;
using namespace vppagent;
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

BOOST_AUTO_TEST_SUITE(VppManager_test)

class VppManagerFixture : public ModbFixture {
public:
    VppManagerFixture()
        : vppManager(agent, switchManager, idGen, ctZoneManager),
          policyMgr(agent.getPolicyManager()),
          switchManager(agent, portMapper),
          ctZoneManager(idGen),
          ep2_port(11), ep4_port(22) {
        createObjects();

        tunIf = "br0_vxlan0";
        vppManager.setEncapIface(tunIf);
        vppManager.setTunnel("10.11.12.13", 4789);
        vppManager.setVirtualRouter(true, true, "aa:bb:cc:dd:ee:ff");
        vppManager.setVirtualDHCP(true, "aa:bb:cc:dd:ee:ff");

        portMapper.ports[ep0->getInterfaceName().get()] = 80;
        portMapper.RPortMap[80] = ep0->getInterfaceName().get();
        portMapper.ports[tunIf] = 2048;
        portMapper.RPortMap[2048] = tunIf;
        tun_port_new = 4096;

        WAIT_FOR(policyMgr.groupExists(epg0->getURI()), 500);
        WAIT_FOR(policyMgr.getBDForGroup(epg0->getURI()) != boost::none, 500);

        WAIT_FOR(policyMgr.groupExists(epg1->getURI()), 500);
        WAIT_FOR(policyMgr.getRDForGroup(epg1->getURI()) != boost::none, 500);

        WAIT_FOR(policyMgr.groupExists(epg3->getURI()), 500);
        WAIT_FOR(policyMgr.getRDForGroup(epg3->getURI()) != boost::none, 500);

        switchManager.registerStateHandler(&vppManager);
        vppManager.enableConnTrack();
        start();
    }
    virtual ~VppManagerFixture() {
        vppManager.stop();
        stop();
    }

    virtual void start() {
        using boost::asio::io_service;
        using std::thread;
        agent.getAgentIOService().reset();
        io_service& io = agent.getAgentIOService();
        ioWork.reset(new io_service::work(io));
        ioThread.reset(new thread([&io] { io.run(); }));
        switchManager.start("placeholder");
    }

    virtual void stop() {
        switchManager.stop();
        ioWork.reset();
        ioThread->join();
        ioThread.reset();
    }

    void setConnected() {
        switchManager.enableSync();
        switchManager.connect();
    }

    void createGroupEntries(VppManager::EncapType encapType);
    // void createOnConnectEntries(VppManager::EncapType encapType,
    //                             FlowEntryList& flows,
    //                             GroupEdit::EntryList& groups);

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
    void initExpLocalService(int nextHop = 0);

    /** Initialize service-scoped flow entries for load balanced
        remote services */
    void initExpLBRemoteService(bool conntrack = false);

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

    VppManager vppManager;
    PolicyManager& policyMgr;
    IdGenerator idGen;
    CtZoneManager ctZoneManager;
    MockPortMapper portMapper;
    SwitchManager switchManager;

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

    std::unique_ptr<std::thread> ioThread;
    std::unique_ptr<boost::asio::io_service::work> ioWork;

};

class VxlanVppManagerFixture : public VppManagerFixture {
public:
    VxlanVppManagerFixture() : VppManagerFixture() {
        vppManager.setEncapType(VppManager::ENCAP_VXLAN);
        vppManager.start();
    }

    virtual ~VxlanVppManagerFixture() {}
};

class VlanVppManagerFixture : public VppManagerFixture {
public:
    VlanVppManagerFixture() : VppManagerFixture() {
        vppManager.setEncapType(VppManager::ENCAP_VLAN);
        vppManager.start();
    }

    virtual ~VlanVppManagerFixture() {}
};

/************************************************************
 *
 * Completed tests commented out for expediency
 *
 **********************************************************/
// void VppManagerFixture::connectTest() {
//     vppManager.egDomainUpdated(epg4->getURI());
//     setConnected();

// }

// BOOST_FIXTURE_TEST_CASE(connect_vxlan, VxlanVppManagerFixture) {
//     connectTest();
// }

// // alagalah: TODO
// // BOOST_FIXTURE_TEST_CASE(connect_vlan, VlanVppManagerFixture) {
// //     connectTest();
// // }

void VppManagerFixture::fdTest() {
    setConnected();

    /* create */
    portMapper.ports[ep2->getInterfaceName().get()] = ep2_port;
    portMapper.ports[ep4->getInterfaceName().get()] = ep4_port;
    vppManager.endpointUpdated(ep0->getUUID());
    vppManager.endpointUpdated(ep2->getUUID());

    /* Set FD & update EP0 */
    {
        Mutator m1(framework, policyOwner);
        epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetFloodDomain(fd0->getURI());
        m1.commit();
    }
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);

    vppManager.endpointUpdated(ep0->getUUID());

    /* update EP2 */
    vppManager.endpointUpdated(ep2->getUUID());

    /* remove port-mapping for ep2 */
    portMapper.ports.erase(ep2->getInterfaceName().get());
    vppManager.endpointUpdated(ep2->getUUID());

    /* remove ep0 */
    epSrc.removeEndpoint(ep0->getUUID());
    vppManager.endpointUpdated(ep0->getUUID());

    /* check promiscous flood */
    WAIT_FOR(policyMgr.getFDForGroup(epg2->getURI()) != boost::none, 500);
    vppManager.endpointUpdated(ep4->getUUID());

    /* group changes on tunnel port change */
    portMapper.ports[tunIf] = tun_port_new;
    vppManager.portStatusUpdate(tunIf, tun_port_new, false);

}

BOOST_FIXTURE_TEST_CASE(fd_vxlan, VxlanVppManagerFixture) {
    fdTest();
}

// BOOST_FIXTURE_TEST_CASE(fd_vlan, VlanVppManagerFixture) {
//     fdTest();
// }


/************************************************************
 *
 * TODO
 *
 **********************************************************/

// void VppManagerFixture::epgTest() {
//     setConnected();

//     /* create */
//     vppManager.egDomainUpdated(epg0->getURI());
//     vppManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

//     /* forwarding object change */
//     Mutator m1(framework, policyOwner);
//     epg0->addGbpEpGroupToNetworkRSrc()
//         ->setTargetFloodDomain(fd0->getURI());
//     m1.commit();
//     WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);
//     PolicyManager::subnet_vector_t sns;
//     WAIT_FOR_DO(sns.size() == 4, 500, sns.clear();
//                 policyMgr.getSubnetsForGroup(epg0->getURI(), sns));

//     vppManager.egDomainUpdated(epg0->getURI());

//     /* Change flood domain to isolated */
//     fd0->setBcastFloodMode(BcastFloodModeEnumT::CONST_ISOLATED);
//     m1.commit();
//     optional<shared_ptr<modelgbp::gbp::FloodDomain> > fdptr;
//     WAIT_FOR((fdptr = policyMgr.getFDForGroup(epg2->getURI()))
//              ? (fdptr.get()
//                 ->getBcastFloodMode(BcastFloodModeEnumT::CONST_NORMAL) ==
//                 BcastFloodModeEnumT::CONST_ISOLATED) : false, 500);
//     vppManager.egDomainUpdated(epg0->getURI());


//     /* remove domain objects */
//     PolicyManager::subnet_vector_t subnets, subnets_copy;
//     policyMgr.getSubnetsForGroup(epg0->getURI(), subnets);
//     BOOST_CHECK(!subnets.empty());
//     subnets_copy = subnets;

//     rd0->remove();
//     for (shared_ptr<Subnet>& sn : subnets) {
//         sn->remove();
//     }
//     m1.commit();
//     WAIT_FOR(!policyMgr.getRDForGroup(epg0->getURI()), 500);
//     WAIT_FOR_DO(subnets.empty(), 500,
//         subnets.clear(); policyMgr.getSubnetsForGroup(epg0->getURI(), subnets));

//     vppManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());
//     for (shared_ptr<Subnet>& sn : subnets_copy) {
//         vppManager.domainUpdated(Subnet::CLASS_ID, sn->getURI());
//     }


//     /* remove */

//     // XXX TODO there seems to be bugs around removing the epg but not
//     // the endpoint with flows left behind

//     epSrc.removeEndpoint(ep0->getUUID());
//     epSrc.removeEndpoint(ep2->getUUID());

//     Mutator m2(framework, policyOwner);
//     epg0->remove();
//     bd0->remove();
//     m2.commit();
//     WAIT_FOR(policyMgr.groupExists(epg0->getURI()) == false, 500);


//     vppManager.domainUpdated(BridgeDomain::CLASS_ID, bd0->getURI());
//     vppManager.egDomainUpdated(epg0->getURI());
//     vppManager.endpointUpdated(ep0->getUUID());
//     vppManager.endpointUpdated(ep2->getUUID());

// }

// BOOST_FIXTURE_TEST_CASE(epg_vxlan, VxlanVppManagerFixture) {
//     epgTest();
// }

// BOOST_FIXTURE_TEST_CASE(epg_vlan, VlanVppManagerFixture) {
//     epgTest();
// }

// void VppManagerFixture::routeModeTest() {
//     setConnected();
//     vppManager.egDomainUpdated(epg0->getURI());

//     /* Disable routing on bridge domain */
//     {
//         Mutator mutator(framework, policyOwner);
//         bd0->setRoutingMode(RoutingModeEnumT::CONST_DISABLED);
//         mutator.commit();
//     }
//     WAIT_FOR(policyMgr.getBDForGroup(epg0->getURI()).get()
//              ->getRoutingMode(RoutingModeEnumT::CONST_ENABLED) ==
//              RoutingModeEnumT::CONST_DISABLED, 500);
//     vppManager.egDomainUpdated(epg0->getURI());

//     /* Enable routing */
//     {
//         Mutator mutator(framework, policyOwner);
//         bd0->setRoutingMode(RoutingModeEnumT::CONST_ENABLED);
//         mutator.commit();
//     }
//     WAIT_FOR(policyMgr.getBDForGroup(epg0->getURI()).get()
//              ->getRoutingMode(RoutingModeEnumT::CONST_ENABLED) ==
//              RoutingModeEnumT::CONST_ENABLED, 500);
//     vppManager.egDomainUpdated(epg0->getURI());


// }

// BOOST_FIXTURE_TEST_CASE(routemode_vxlan, VxlanVppManagerFixture) {
//     routeModeTest();
// }

// BOOST_FIXTURE_TEST_CASE(routemode_vlan, VlanVppManagerFixture) {
//     routeModeTest();
// }

// void VppManagerFixture::arpModeTest() {
//     /* setup entries for epg0 connected to fd0 */
//     setConnected();


//     Mutator m1(framework, policyOwner);
//     epg0->addGbpEpGroupToNetworkRSrc()
//         ->setTargetFloodDomain(fd0->getURI());
//     m1.commit();
//     WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);
//     PolicyManager::subnet_vector_t sns;
//     WAIT_FOR_DO(sns.size() == 4, 500, sns.clear();
//                 policyMgr.getSubnetsForGroup(epg0->getURI(), sns));
//     vppManager.egDomainUpdated(epg0->getURI());


//     /* change arp mode to flood */
//     fd0->setArpMode(AddressResModeEnumT::CONST_FLOOD)
//         .setNeighborDiscMode(AddressResModeEnumT::CONST_FLOOD);
//     m1.commit();
//     WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()).get()
//              ->getArpMode(AddressResModeEnumT::CONST_UNICAST)
//              == AddressResModeEnumT::CONST_FLOOD, 500);
//     WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()).get()
//              ->getNeighborDiscMode(AddressResModeEnumT::CONST_UNICAST)
//              == AddressResModeEnumT::CONST_FLOOD, 500);
//     vppManager.egDomainUpdated(epg0->getURI());


//     /* set discovery proxy mode on for ep0 */
//     ep0->setDiscoveryProxyMode(true);
//     epSrc.updateEndpoint(*ep0);
//     vppManager.endpointUpdated(ep0->getUUID());

// }

// BOOST_FIXTURE_TEST_CASE(arpmode_vxlan, VxlanVppManagerFixture) {
//     arpModeTest();
// }

// BOOST_FIXTURE_TEST_CASE(arpmode_vlan, VlanVppManagerFixture) {
//     arpModeTest();
// }

// BOOST_FIXTURE_TEST_CASE(localEp, VxlanVppManagerFixture) {
//     setConnected();

//     /* created */
//     vppManager.endpointUpdated(ep0->getUUID());


//     /* endpoint group change */
//     ep0->setEgURI(epg1->getURI());
//     epSrc.updateEndpoint(*ep0);
//     vppManager.endpointUpdated(ep0->getUUID());

//     /* endpoint group changes back to old one */
//     ep0->setEgURI(epg0->getURI());
//     epSrc.updateEndpoint(*ep0);
//     vppManager.endpointUpdated(ep0->getUUID());

//     /* port-mapping change */
//     portMapper.ports[ep0->getInterfaceName().get()] = 180;
//     vppManager.portStatusUpdate(ep0->getInterfaceName().get(),
//                                  180, false);
//     /* remove endpoint */
//     epSrc.removeEndpoint(ep0->getUUID());
//     vppManager.endpointUpdated(ep0->getUUID());

// }

// BOOST_FIXTURE_TEST_CASE(noifaceEp, VxlanVppManagerFixture) {
//     setConnected();

//     /* create */
//     vppManager.endpointUpdated(ep2->getUUID());

//     /* endpoint group change */
//     ep2->setEgURI(epg1->getURI());
//     epSrc.updateEndpoint(*ep2);
//     vppManager.endpointUpdated(ep2->getUUID());

// }


// void VppManagerFixture::groupFloodTest() {
//     vppManager.setFloodScope(VppManager::ENDPOINT_GROUP);
//     setConnected();

//     /* "create" local endpoints ep0 and ep2 */
//     portMapper.ports[ep2->getInterfaceName().get()] = ep2_port;
//     vppManager.endpointUpdated(ep0->getUUID());
//     vppManager.endpointUpdated(ep2->getUUID());


//     /* Move ep2 to epg2, add epg0 to fd0. Now fd0 has 2 epgs */
//     ep2->setEgURI(epg2->getURI());
//     epSrc.updateEndpoint(*ep2);
//     Mutator m1(framework, policyOwner);
//     epg0->addGbpEpGroupToNetworkRSrc()
//             ->setTargetFloodDomain(fd0->getURI());
//     m1.commit();
//     WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);
//     WAIT_FOR(policyMgr.getFDForGroup(epg2->getURI()) != boost::none, 500);

//     vppManager.endpointUpdated(ep0->getUUID());

//     vppManager.endpointUpdated(ep2->getUUID());

//     /* remove ep0 */
//     epSrc.removeEndpoint(ep0->getUUID());
//     vppManager.endpointUpdated(ep0->getUUID());

// }

// BOOST_FIXTURE_TEST_CASE(group_flood_vxlan, VxlanVppManagerFixture) {
//     groupFloodTest();
// }

// BOOST_FIXTURE_TEST_CASE(group_flood_vlan, VlanVppManagerFixture) {
//     groupFloodTest();
// }

// BOOST_FIXTURE_TEST_CASE(policy, VxlanVppManagerFixture) {
//     setConnected();

//     createPolicyObjects();

//     PolicyManager::uri_set_t egs;
//     WAIT_FOR_DO(egs.size() == 2, 1000, egs.clear();
//                 policyMgr.getContractProviders(con1->getURI(), egs));
//     egs.clear();
//     WAIT_FOR_DO(egs.size() == 2, 500, egs.clear();
//                 policyMgr.getContractConsumers(con1->getURI(), egs));

//     /* add con2 */
//     vppManager.contractUpdated(con2->getURI());

//     /* add con1 */
//     vppManager.contractUpdated(con1->getURI());

//     /* remove */
//     Mutator m2(framework, policyOwner);
//     con2->remove();
//     m2.commit();
//     PolicyManager::rule_list_t rules;
//     policyMgr.getContractRules(con2->getURI(), rules);
//     WAIT_FOR_DO(rules.empty(), 500,
//         rules.clear(); policyMgr.getContractRules(con2->getURI(), rules));
//     vppManager.contractUpdated(con2->getURI());
// }

// BOOST_FIXTURE_TEST_CASE(policy_portrange, VxlanVppManagerFixture) {
//     setConnected();

//     createPolicyObjects();

//     PolicyManager::uri_set_t egs;
//     WAIT_FOR_DO(egs.size() == 1, 1000, egs.clear();
//                 policyMgr.getContractProviders(con3->getURI(), egs));
//     egs.clear();
//     WAIT_FOR_DO(egs.size() == 1, 500, egs.clear();
//                 policyMgr.getContractConsumers(con3->getURI(), egs));

//     /* add con3 */
//     vppManager.contractUpdated(con3->getURI());
// }


// void VppManagerFixture::portStatusTest() {
//     setConnected();

//     /* create entries for epg0, ep0 and ep2 with original tunnel port */
//     vppManager.egDomainUpdated(epg0->getURI());
//     vppManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

//     /* delete all groups except epg0, then update tunnel port */
//     PolicyManager::uri_set_t epgURIs;
//     policyMgr.getGroups(epgURIs);
//     epgURIs.erase(epg0->getURI());
//     Mutator m2(framework, policyOwner);
//     for (const URI& u : epgURIs) {
//         EpGroup::resolve(agent.getFramework(), u).get()->remove();
//     }
//     m2.commit();
//     epgURIs.clear();
//     WAIT_FOR_DO(epgURIs.size() == 1, 500,
//                 epgURIs.clear(); policyMgr.getGroups(epgURIs));
//     portMapper.ports[tunIf] = tun_port_new;
//     for (const URI& u : epgURIs) {
//         vppManager.egDomainUpdated(u);
//     }
//     vppManager.portStatusUpdate(tunIf, tun_port_new, false);

//     /* remove mapping for tunnel port */
//     portMapper.ports.erase(tunIf);
//     vppManager.portStatusUpdate(tunIf, tun_port_new, false);

// }

// BOOST_FIXTURE_TEST_CASE(portstatus_vxlan, VxlanVppManagerFixture) {
//     portStatusTest();
// }

// BOOST_FIXTURE_TEST_CASE(portstatus_vlan, VlanVppManagerFixture) {
//     portStatusTest();
// }

// static unordered_set<string> readMcast(const std::string& filePath) {
//     try {
//         unordered_set<string> ips;
//         pt::ptree properties;
//         pt::read_json(filePath, properties);
//         optional<pt::ptree&> groups =
//             properties.get_child_optional("multicast-groups");
//         if (groups) {
//             for (const pt::ptree::value_type &v : groups.get())
//                 ips.insert(v.second.data());
//         }
//         return ips;
//     } catch (pt::json_parser_error e) {
//         LOG(DEBUG) << "Could not parse: " << e.what();
//         return unordered_set<string>();
//     }
// }

// BOOST_FIXTURE_TEST_CASE(mcast, VxlanVppManagerFixture) {

//     TempGuard tg;
//     fs::path temp = tg.temp_dir / "mcast.json";

//     MockSwitchConnection conn;
//     vppManager.setMulticastGroupFile(temp.string());

//     string prmBr = " " + conn.getSwitchName() + " 4789 ";

//     WAIT_FOR(config->isMulticastGroupIPSet(), 500);
//     string mcast1 = config->getMulticastGroupIP().get();
//     string mcast2 = "224.1.1.2";
//     string mcast3 = "224.5.1.1";
//     string mcast4 = "224.5.1.2";

//     unordered_set<string> expected;
//     expected.insert(mcast1);

//     vppManager.configUpdated(config->getURI());
//     setConnected();
// #define CHECK_MCAST                                                     \
//     WAIT_FOR_ONFAIL(readMcast(temp.string()) == expected, 500,          \
//             { for (const std::string& ip : readMcast(temp.string()))    \
//                     LOG(ERROR) << ip; })
//     CHECK_MCAST;
//     {
//         Mutator mutator(framework, policyOwner);
//         config->setMulticastGroupIP(mcast2);
//         mutator.commit();
//     }

//     vppManager.configUpdated(config->getURI());
//     vppManager.egDomainUpdated(epg2->getURI());
//     expected.clear();
//     expected.insert(mcast2);
//     expected.insert(mcast3);

//     CHECK_MCAST;

//     {
//         Mutator mutator(framework, policyOwner);
//         fd0ctx->setMulticastGroupIP(mcast4);
//         mutator.commit();
//     }
//     WAIT_FOR_DO(fd0ctx->getMulticastGroupIP("") == mcast4, 500,
//         fd0ctx = policyMgr.getFloodContextForGroup(epg2->getURI()).get());

//     vppManager.configUpdated(config->getURI());
//     vppManager.egDomainUpdated(epg2->getURI());
//     expected.insert(mcast4);

//     CHECK_MCAST;

//     {
//         Mutator mutator(framework, policyOwner);
//         fd0ctx->unsetMulticastGroupIP();
//         mutator.commit();
//     }
//     WAIT_FOR_DO(fd0ctx->isMulticastGroupIPSet() == false, 500,
//         fd0ctx = policyMgr.getFloodContextForGroup(epg2->getURI()).get());
//     vppManager.egDomainUpdated(epg2->getURI());

//     expected.erase(mcast4);

//     CHECK_MCAST;
// #undef CHECK_MCAST
// }

// BOOST_FIXTURE_TEST_CASE(ipMapping, VxlanVppManagerFixture) {
//     setConnected();
//     vppManager.egDomainUpdated(epg0->getURI());

//     shared_ptr<modelgbp::policy::Space> common;
//     shared_ptr<FloodDomain> fd_ext;
//     shared_ptr<EpGroup> eg_nat;
//     shared_ptr<RoutingDomain> rd_ext;
//     shared_ptr<BridgeDomain> bd_ext;
//     shared_ptr<Subnets> subnets_ext;
//     shared_ptr<L3ExternalDomain> l3ext;
//     shared_ptr<L3ExternalNetwork> l3ext_net;
//     {
//         Mutator mutator(framework, policyOwner);
//         common = universe->addPolicySpace("common");
//         bd_ext = common->addGbpBridgeDomain("bd_ext");
//         rd_ext = common->addGbpRoutingDomain("rd_ext");
//         fd_ext = common->addGbpFloodDomain("fd_ext");

//         fd_ext->addGbpFloodDomainToNetworkRSrc()
//             ->setTargetBridgeDomain(bd_ext->getURI());
//         bd_ext->addGbpBridgeDomainToNetworkRSrc()
//             ->setTargetRoutingDomain(rd_ext->getURI());

//         subnets_ext = common->addGbpSubnets("subnets_ext");
//         subnets_ext->addGbpSubnet("subnet_ext4")
//             ->setAddress("5.5.5.0")
//             .setPrefixLen(24);
//         subnets_ext->addGbpSubnet("subnet_ext6")
//             ->setAddress("fdf1:9f86:d1af:6cc9::")
//             .setPrefixLen(64)
//             .setVirtualRouterIp("fdf1:9f86:d1af:6cc9::1");
//         bd_ext->addGbpForwardingBehavioralGroupToSubnetsRSrc()
//             ->setTargetSubnets(subnets_ext->getURI());
//         rd_ext->addGbpRoutingDomainToIntSubnetsRSrc(subnets_ext->
//                                                     getURI().toString());

//         eg_nat = common->addGbpEpGroup("nat-epg");
//         eg_nat->addGbpeInstContext()->setEncapId(0x4242);
//         eg_nat->addGbpEpGroupToNetworkRSrc()
//             ->setTargetFloodDomain(fd_ext->getURI());

//         l3ext = rd0->addGbpL3ExternalDomain("ext");
//         l3ext_net = l3ext->addGbpL3ExternalNetwork("outside");
//         l3ext_net->addGbpExternalSubnet("outside")
//             ->setAddress("5.5.0.0")
//             .setPrefixLen(8);
//         l3ext_net->addGbpExternalSubnet("outside_v6")
//             ->setAddress("fdf1::ffff")
//             .setPrefixLen(16);
//         mutator.commit();
//     }

//     Endpoint::IPAddressMapping ipm4("91c5b217-d244-432c-922d-533c6c036ab3");
//     ipm4.setMappedIP("10.20.44.2");
//     ipm4.setFloatingIP("5.5.5.5");
//     ipm4.setEgURI(eg_nat->getURI());
//     ep0->addIPAddressMapping(ipm4);
//     Endpoint::IPAddressMapping ipm6("c54b8503-1f14-4529-a0b3-cf704e1dfeb7");
//     ipm6.setMappedIP("2001:db8::2");
//     ipm6.setFloatingIP("fdf1:9f86:d1af:6cc9::5");
//     ipm6.setEgURI(eg_nat->getURI());
//     ep0->addIPAddressMapping(ipm6);
//     epSrc.updateEndpoint(*ep0);

//     WAIT_FOR(policyMgr.getBDForGroup(eg_nat->getURI()) != boost::none, 500);
//     WAIT_FOR(policyMgr.getFDForGroup(eg_nat->getURI()) != boost::none, 500);
//     WAIT_FOR(policyMgr.getRDForGroup(eg_nat->getURI()) != boost::none, 500);
//     PolicyManager::subnet_vector_t sns;
//     WAIT_FOR_DO(sns.size() == 2, 500, sns.clear();
//                 policyMgr.getSubnetsForGroup(eg_nat->getURI(), sns));

//     vppManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());
//     vppManager.egDomainUpdated(epg0->getURI());
//     vppManager.egDomainUpdated(eg_nat->getURI());
//     vppManager.endpointUpdated(ep0->getUUID());
//     vppManager.endpointUpdated(ep2->getUUID());

//     {
//         Mutator mutator(framework, policyOwner);
//         l3ext_net->addGbpL3ExternalNetworkToNatEPGroupRSrc()
//             ->setTargetEpGroup(eg_nat->getURI());
//         mutator.commit();
//     }
//     WAIT_FOR(policyMgr.getVnidForGroup(eg_nat->getURI())
//              .get_value_or(0) == 0x4242, 500);
//     vppManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

//     // Add next hop for mapping
//     portMapper.ports["nexthop"] = 42;
//     portMapper.RPortMap[42] = "nexthop";
//     ep0->clearIPAddressMappings();
//     ipm4.setNextHopIf("nexthop");
//     ipm4.setNextHopMAC(MAC("42:00:42:42:42:42"));
//     ep0->addIPAddressMapping(ipm4);
//     ipm6.setNextHopIf("nexthop");
//     ipm6.setNextHopMAC(MAC("42:00:42:42:42:42"));
//     ep0->addIPAddressMapping(ipm6);
//     epSrc.updateEndpoint(*ep0);

//     unordered_set<string> eps;
//     agent.getEndpointManager().getEndpointsByIpmNextHopIf("nexthop", eps);
//     BOOST_CHECK_EQUAL(1, eps.size());
//     vppManager.endpointUpdated(ep0->getUUID());

// }

// BOOST_FIXTURE_TEST_CASE(localService, VxlanVppManagerFixture) {
//     setConnected();
//     vppManager.egDomainUpdated(epg0->getURI());
//     vppManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());
//     portMapper.ports["service-iface"] = 17;
//     portMapper.RPortMap[17] = "service-iface";

//     AnycastService as;
//     as.setUUID("ed84daef-1696-4b98-8c80-6b22d85f4dc2");
//     as.setServiceMAC(MAC("ed:84:da:ef:16:96"));
//     as.setDomainURI(URI(rd0->getURI()));
//     as.setInterfaceName("service-iface");

//     AnycastService::ServiceMapping sm1;
//     sm1.setServiceIP("169.254.169.254");
//     sm1.setGatewayIP("169.254.1.1");
//     as.addServiceMapping(sm1);

//     AnycastService::ServiceMapping sm2;
//     sm2.setServiceIP("fe80::a9:fe:a9:fe");
//     sm2.setGatewayIP("fe80::1");
//     as.addServiceMapping(sm2);

//     servSrc.updateAnycastService(as);
//     vppManager.anycastServiceUpdated(as.getUUID());

//     as.clearServiceMappings();
//     sm1.addNextHopIP("169.254.169.1");
//     as.addServiceMapping(sm1);
//     sm2.addNextHopIP("fe80::a9:fe:a9:1");
//     as.addServiceMapping(sm2);

//     servSrc.updateAnycastService(as);
//     vppManager.anycastServiceUpdated(as.getUUID());

//     as.clearServiceMappings();
//     sm1.addNextHopIP("169.254.169.2");
//     as.addServiceMapping(sm1);
//     sm2.addNextHopIP("fe80::a9:fe:a9:2");
//     as.addServiceMapping(sm2);

//     servSrc.updateAnycastService(as);
//     vppManager.anycastServiceUpdated(as.getUUID());

// }

// BOOST_FIXTURE_TEST_CASE(loadBalancedService, VxlanVppManagerFixture) {
//     setConnected();
//     vppManager.egDomainUpdated(epg0->getURI());
//     vppManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

//     AnycastService as;
//     as.setUUID("ed84daef-1696-4b98-8c80-6b22d85f4dc2");
//     as.setDomainURI(URI(rd0->getURI()));

//     AnycastService::ServiceMapping sm1;
//     sm1.setServiceIP("169.254.169.254");
//     sm1.setServiceProto("udp");
//     sm1.addNextHopIP("169.254.169.1");
//     sm1.addNextHopIP("169.254.169.2");
//     sm1.setServicePort(53);
//     sm1.setNextHopPort(5353);
//     as.addServiceMapping(sm1);

//     AnycastService::ServiceMapping sm2;
//     sm2.setServiceIP("fe80::a9:fe:a9:fe");
//     sm2.setServiceProto("tcp");
//     sm2.addNextHopIP("fe80::a9:fe:a9:1");
//     sm2.addNextHopIP("fe80::a9:fe:a9:2");
//     sm2.setServicePort(80);
//     as.addServiceMapping(sm2);

//     as.clearServiceMappings();
//     sm1.addNextHopIP("169.254.169.2");
//     as.addServiceMapping(sm1);
//     sm2.addNextHopIP("fe80::a9:fe:a9:2");
//     as.addServiceMapping(sm2);

//     servSrc.updateAnycastService(as);
//     vppManager.anycastServiceUpdated(as.getUUID());

//     as.clearServiceMappings();
//     sm1.setConntrackMode(true);
//     as.addServiceMapping(sm1);
//     sm2.setConntrackMode(true);
//     as.addServiceMapping(sm2);

//     servSrc.updateAnycastService(as);
//     vppManager.anycastServiceUpdated(as.getUUID());

// }

// BOOST_FIXTURE_TEST_CASE(vip, VxlanVppManagerFixture) {
//     setConnected();
//     vppManager.egDomainUpdated(epg0->getURI());
//     vppManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

//     ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:42"), "42.42.42.42"));
//     ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:43"), "42.42.42.16/28"));
//     ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:42"), "42::42"));
//     ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:43"), "42::10/124"));
//     ep0->addVirtualIP(make_pair(MAC("00:00:00:00:80:00"), "10.20.44.3"));
//     epSrc.updateEndpoint(*ep0);
//     vppManager.endpointUpdated(ep0->getUUID());
// }

// BOOST_FIXTURE_TEST_CASE(virtDhcp, VxlanVppManagerFixture) {
//     setConnected();
//     vppManager.egDomainUpdated(epg0->getURI());
//     vppManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

//     Endpoint::DHCPv4Config v4;
//     Endpoint::DHCPv6Config v6;

//     ep0->setDHCPv4Config(v4);
//     ep0->setDHCPv6Config(v6);
//     epSrc.updateEndpoint(*ep0);
//     vppManager.endpointUpdated(ep0->getUUID());


//     ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:42"), "42.42.42.42"));
//     ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:43"), "42.42.42.16/28"));
//     ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:42"), "42::42"));
//     ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:43"), "42::10/124"));
//     ep0->addVirtualIP(make_pair(MAC("00:00:00:00:80:00"), "10.20.44.3"));
//     epSrc.updateEndpoint(*ep0);
//     vppManager.endpointUpdated(ep0->getUUID());

// }


typedef unordered_map<uint32_t, uint32_t>::value_type IdKeyValue;

BOOST_AUTO_TEST_SUITE_END()
