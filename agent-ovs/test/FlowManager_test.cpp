/*
 * Test suite for class FlowManager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <sstream>
#include <boost/test/unit_test.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <modelgbp/gbp/AddressResModeEnumT.hpp>
#include <modelgbp/gbp/RoutingModeEnumT.hpp>

#include "logging.h"
#include "ovs.h"
#include "FlowManager.h"
#include "FlowExecutor.h"

#include "ModbFixture.h"
#include "MockSwitchConnection.h"
#include "MockPortMapper.h"
#include "TableState.h"
#include "ActionBuilder.h"
#include "RangeMask.h"

using namespace std;
using namespace boost::assign;
using namespace opflex::modb;
using namespace modelgbp::gbp;
using namespace ovsagent;
using boost::asio::ip::address;
using boost::bind;
using boost::thread;
using boost::ref;
using boost::asio::io_service;

typedef pair<FlowEdit::TYPE, string> MOD;

static string CanonicalizeGroupEntryStr(const string& entryStr);

class MockFlowExecutor : public FlowExecutor {
public:
    MockFlowExecutor() : ignoreFlowMods(true), ignoreGroupMods(true) {}
    ~MockFlowExecutor() {}

    bool Execute(const FlowEdit& flowEdits) {
        if (ignoreFlowMods) return true;

        const char *modStr[] = {"ADD", "MOD", "DEL"};
        struct ds strBuf;
        ds_init(&strBuf);

        BOOST_FOREACH(const FlowEdit::Entry& ed, flowEdits.edits) {
            if (ignoredFlowMods.find(ed.first) != ignoredFlowMods.end())
                continue;

            ofp_print_flow_stats(&strBuf, ed.second->entry);
            string str = (const char*)(ds_cstr(&strBuf)+1); // trim space

            BOOST_CHECK_MESSAGE(!flowMods.empty(), "\nexp:\ngot: " << ed);
            if (!flowMods.empty()) {
                MOD exp = flowMods.front();
                flowMods.pop_front();
                BOOST_CHECK_MESSAGE(exp.first == ed.first,
                        "\nexp: " << modStr[exp.first] <<
                        "\ngot: " << ed);
                BOOST_CHECK_MESSAGE(exp.second == str,
                        "\nexp: " << modStr[exp.first] << "|" << exp.second <<
                        "\ngot: " << ed);
            }
            ds_clear(&strBuf);
        }
        ds_destroy(&strBuf);
        return true;
    }
    bool Execute(const GroupEdit& groupEdits) {
        if (ignoreGroupMods) return true;

        BOOST_FOREACH(const GroupEdit::Entry& ed, groupEdits.edits) {
            LOG(DEBUG) << "*** GroupMod " << ed;
            stringstream ss;
            ss << ed;
            string edStr = CanonicalizeGroupEntryStr(ss.str());

            BOOST_CHECK_MESSAGE(!groupMods.empty(), "\nexp:\ngot: " << edStr);
            if (!groupMods.empty()) {
                string exp = groupMods.front();
                groupMods.pop_front();
                BOOST_CHECK_MESSAGE(exp == edStr,
                        "\nexp: " << exp << "\ngot: " << edStr);
            }
        }
        return true;
    }
    void Expect(FlowEdit::TYPE mod, const string& fe) {
        ignoreFlowMods = false;
        flowMods.push_back(MOD(mod, fe));
    }
    void Expect(FlowEdit::TYPE mod, const vector<string>& fe) {
        ignoreFlowMods = false;
        BOOST_FOREACH(const string& s, fe)
            flowMods.push_back(MOD(mod, s));
     }
    void ExpectGroup(FlowEdit::TYPE mod, const string& ge) {
        ignoreGroupMods = false;
        const char *modStr[] = {"ADD", "MOD", "DEL"};
        groupMods.push_back(CanonicalizeGroupEntryStr(
                string(modStr[mod]) + "|" + ge));
    }
    void IgnoreFlowMods() {
        ignoreFlowMods = true;
        flowMods.clear();
    }
    void IgnoreGroupMods() {
        ignoreGroupMods = true;
        groupMods.clear();
    }
    bool IsEmpty() { return flowMods.empty() || ignoreFlowMods; }
    bool IsGroupEmpty() {
        return groupMods.empty() || ignoreGroupMods;
    }
    void Clear() {
        flowMods.clear();
        groupMods.clear();
    }

    std::list<MOD> flowMods;
    std::list<string> groupMods;
    bool ignoreFlowMods;
    unordered_set<int> ignoredFlowMods;
    bool ignoreGroupMods;
};

static void addExpFlowEntry(FlowEntryList* tables, const string& flowMod) {
    struct ofputil_flow_mod fm;
    enum ofputil_protocol prots;
    char* error =
        parse_ofp_flow_mod_str(&fm, flowMod.c_str(), OFPFC_ADD, &prots);
    if (error) {
        LOG(ERROR) << "Could not parse: " << flowMod << ": " << error;
        return;
    } else if (fm.table_id >= FlowManager::NUM_FLOW_TABLES) {
        LOG(ERROR) << "Invalid table ID: " << fm.table_id;
        return;
    }

    FlowEntryPtr e(new FlowEntry());
    e->entry->match = fm.match;
    e->entry->cookie = fm.new_cookie;
    e->entry->table_id = fm.table_id;
    e->entry->priority = fm.priority;
    e->entry->idle_timeout = fm.idle_timeout;
    e->entry->hard_timeout = fm.hard_timeout;
    e->entry->ofpacts = fm.ofpacts;
    fm.ofpacts = NULL;
    e->entry->ofpacts_len = fm.ofpacts_len;
    e->entry->flags = fm.flags;
    tables[fm.table_id].push_back(e);

    struct ds strBuf;
    ds_init(&strBuf);
    ofp_print_flow_stats(&strBuf, e->entry);
    string str = (const char*)(ds_cstr(&strBuf)+1); // trim space
    BOOST_CHECK(str == flowMod);
    ds_destroy(&strBuf);
}

static void printAllDiffs(FlowEntryList* expected, FlowEdit* diffs) {
    for (int i = 0; i < FlowManager::NUM_FLOW_TABLES; i++) {
        if (diffs[i].edits.size() != 0) {
            LOG(ERROR) << "== Expected state for table " << i << ": ==";
            for (size_t j = 0; j < expected[i].size(); ++j) {
                LOG(ERROR) << *(expected[i][j]);
            }
            LOG(ERROR) << " == Diffs from expected state to match result table "
                      << i << ": ==";
            for (size_t j = 0; j < diffs[i].edits.size(); ++j) {
                LOG(ERROR) << diffs[i].edits[j];
            }
        }
    }
}

static void doDiffTables(FlowManager* flowManager,
                         FlowEntryList* expected,
                         FlowEdit* diffs,
                         volatile int* fail) {
    for (int i = 0; i < FlowManager::NUM_FLOW_TABLES; i++) {
        diffs[i].edits.clear();
        flowManager->DiffTableState(i, expected[i], diffs[i]);
    }
    int failed = 0;
    for (int i = 0; i < FlowManager::NUM_FLOW_TABLES; i++) {
        if (diffs[i].edits.size() > 0) failed += 1;
    }
    *fail = failed;
}

static void diffTables(FlowManager& flowManager,
                       FlowEntryList* expected,
                       FlowEdit* diffs,
                       volatile int* fail) {
    *fail = 512;
    flowManager.QueueFlowTask(bind(doDiffTables, &flowManager,
                                   expected, diffs, fail));
    WAIT_FOR(*fail != 512, 1000);
}

static void clearTables(FlowEntryList* tables) {
    for (int i = 0; i < FlowManager::NUM_FLOW_TABLES; i++) {
        tables[i].clear();
    }
}

#define WAIT_FOR_TABLES(test, count)                                    \
    {                                                                   \
        FlowEdit _diffs[FlowManager::NUM_FLOW_TABLES];                  \
        volatile int _fail = 512;                                       \
        WAIT_FOR_DO_ONFAIL(_fail == 0, count,                           \
                           diffTables(flowManager, expTables,           \
                                      _diffs, &_fail),                  \
                           LOG(ERROR) << test ": Incorrect tables: "    \
                                      << _fail;                         \
                           printAllDiffs(expTables, _diffs));           \
    }

class MockFlowReader : public FlowReader {
public:
    MockFlowReader() {}

    bool getFlows(uint8_t tableId, const FlowReader::FlowCb& cb) {
        FlowEntryList res;
        for (size_t i = 0; i < flows.size(); ++i) {
            if (flows[i]->entry->table_id == tableId) {
                res.push_back(flows[i]);
            }
        }
        cb(res, true);
        return true;
    }
    bool getGroups(const FlowReader::GroupCb& cb) {
        cb(groups, true);
        return true;
    }

    FlowEntryList flows;
    GroupEdit::EntryList groups;
};

class MockJsonCmdExecutor : public JsonCmdExecutor {
public:
    bool execute(const std::string& command,
                 const std::vector<std::string>& params,
                 std::string& result) {
        string cmd = command;
        BOOST_FOREACH (const string& s, params) {
            cmd.append(" ").append(s);
        }
        BOOST_CHECK_MESSAGE (!expectedCmd.empty(), "\nexp:\ngot: " << cmd);
        if (!expectedCmd.empty()) {
            string exp = expectedCmd.front().first;
            string res = expectedCmd.front().second;
            expectedCmd.pop_front();
            BOOST_CHECK_MESSAGE(exp == cmd,
                "\nexp: " << exp << "\ngot: " << cmd);
            result = res;
        }
        return true;
    }
    void expect(const string& fullCmd, const string& res="") {
        expectedCmd.push_back(pair<string, string>(fullCmd, res));
        boost::algorithm::trim(expectedCmd.back().first);
    }
    bool empty() { return expectedCmd.empty(); }
    void clear() { expectedCmd.clear(); }
    list<pair<string, string> > expectedCmd;
};

BOOST_AUTO_TEST_SUITE(FlowManager_test)

class FlowManagerFixture : public ModbFixture {
public:
    FlowManagerFixture() : ModbFixture(),
                        flowManager(agent),
                        policyMgr(agent.getPolicyManager()),
                           ep2_port(11), ep4_port(22) {
        tunIf = "br0_vxlan0";
        flowManager.SetExecutor(&exec);
        flowManager.SetPortMapper(&portmapper);
        flowManager.SetEncapIface(tunIf);
        flowManager.SetTunnelRemoteIp("10.11.12.13");
        flowManager.SetSyncDelayOnConnect(0);
        flowManager.SetVirtualRouter(true, true, "aa:bb:cc:dd:ee:ff");
        flowManager.SetVirtualDHCP(true, "aa:bb:cc:dd:ee:ff");

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

        agent.getAgentIOService().reset();
        flowManager.Start();
        ioThread.reset(new thread(bind(&io_service::run,
                                       ref(agent.getAgentIOService()))));
    }
    virtual ~FlowManagerFixture() {
        flowManager.Stop();
        ioThread->join();
        ioThread.reset();
    }

    void setConnected() {
        flowManager.PeerConnected();    // pretend that opflex-peer is present
        flowManager.Connected(NULL);    // force flowManager out of sync-ing
    }
    void createGroupEntries(FlowManager::EncapType encapType);
    void createOnConnectEntries(FlowManager::EncapType encapType,
                                FlowEntryList& flows,
                                GroupEdit::EntryList& groups);

    virtual void clearExpFlowTables() {
        clearTables(expTables);
    }

    /**
     * Initialize flow tables with static flow entries not scoped to any
     * object
     */
    void initExpStatic();

    /** Initialize EPG-scoped flow entries */
    void initExpEpg(shared_ptr<EpGroup>& epg,
                    uint32_t fdId = 0, uint32_t bdId = 1, uint32_t rdId = 1);

    /** Initialize flood domain-scoped flow entries */
    void initExpFd(uint32_t fdId = 0);

    /** Initialize bridge domain-scoped flow entries */
    void initExpBd(uint32_t bdId = 1, uint32_t fdId = 0, uint32_t rdId = 1,
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

    /** Initialize service-scoped flow entries */
    void initExpService(bool nextHop = false);

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

    MockFlowExecutor exec;
    MockFlowReader reader;
    FlowManager flowManager;
    MockPortMapper portmapper;
    PolicyManager& policyMgr;
    MockJsonCmdExecutor jsonCmdExecutor;

    string tunIf;

    FlowEntryList expTables[FlowManager::NUM_FLOW_TABLES];

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

    boost::scoped_ptr<boost::thread> ioThread;
};

class VxlanFlowManagerFixture : public FlowManagerFixture {
public:
    VxlanFlowManagerFixture() : FlowManagerFixture() {
        flowManager.SetEncapType(FlowManager::ENCAP_VXLAN);
        createGroupEntries(FlowManager::ENCAP_VXLAN);
    }

    virtual ~VxlanFlowManagerFixture() {}
};

class VlanFlowManagerFixture : public FlowManagerFixture {
public:
    VlanFlowManagerFixture() : FlowManagerFixture() {
        flowManager.SetEncapType(FlowManager::ENCAP_VLAN);
        createGroupEntries(FlowManager::ENCAP_VLAN);
    }

    virtual ~VlanFlowManagerFixture() {}
};

void FlowManagerFixture::epgTest() {
    setConnected();

    /* create */
    flowManager.egDomainUpdated(epg0->getURI());
    flowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("create", 500);

    /* forwarding object change */
    Mutator m1(framework, policyOwner);
    epg0->addGbpEpGroupToNetworkRSrc()
        ->setTargetFloodDomain(fd0->getURI());
    m1.commit();
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);
    PolicyManager::subnet_vector_t sns;
    WAIT_FOR_DO(sns.size() == 4, 500, sns.clear();
                policyMgr.getSubnetsForGroup(epg0->getURI(), sns));

    flowManager.egDomainUpdated(epg0->getURI());

    exec.Clear();
    exec.ExpectGroup(FlowEdit::add, ge_fd0 + ge_bkt_ep0 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::add, ge_fd0_prom + ge_bkt_tun);
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0, 1);
    initExpFd(1);
    initExpBd(1, 1, 1, true);
    initExpRd();
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg0, 1, 1, 1);
    initSubnets(sns);
    WAIT_FOR_TABLES("forwarding change", 500);

    /* remove domain objects */
    PolicyManager::subnet_vector_t subnets, subnets_copy;
    policyMgr.getSubnetsForGroup(epg0->getURI(), subnets);
    BOOST_CHECK(!subnets.empty());
    subnets_copy = subnets;

    rd0->remove();
    BOOST_FOREACH(shared_ptr<Subnet>& sn, subnets) {
        sn->remove();
    }
    m1.commit();
    WAIT_FOR(!policyMgr.getRDForGroup(epg0->getURI()), 500);
    WAIT_FOR_DO(subnets.empty(), 500,
        subnets.clear(); policyMgr.getSubnetsForGroup(epg0->getURI(), subnets));

    flowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());
    BOOST_FOREACH(shared_ptr<Subnet>& sn, subnets_copy) {
        flowManager.domainUpdated(Subnet::CLASS_ID, sn->getURI());
    }

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0, 1);
    initExpFd(1);
    initExpBd(1, 1, 1, true);
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg0, 1, 1, 1);
    WAIT_FOR_TABLES("remove domain", 500);

    /* remove */

    // XXX TODO there seems to be bugs around removing the epg but not
    // the endpoint with flows left behind

    epSrc.removeEndpoint(ep0->getUUID());
    epSrc.removeEndpoint(ep2->getUUID());

    Mutator m2(framework, policyOwner);
    epg0->remove();
    bd0->remove();
    m2.commit();
    WAIT_FOR(policyMgr.groupExists(epg0->getURI()) == false, 500);

    exec.IgnoreGroupMods();

    flowManager.domainUpdated(BridgeDomain::CLASS_ID, bd0->getURI());
    flowManager.egDomainUpdated(epg0->getURI());
    flowManager.endpointUpdated(ep0->getUUID());
    flowManager.endpointUpdated(ep2->getUUID());

    clearExpFlowTables();
    initExpStatic();
    WAIT_FOR_TABLES("remove epg", 500);
}

BOOST_FIXTURE_TEST_CASE(epg_vxlan, VxlanFlowManagerFixture) {
    epgTest();
}

BOOST_FIXTURE_TEST_CASE(epg_vlan, VlanFlowManagerFixture) {
    epgTest();
}

void FlowManagerFixture::routeModeTest() {
    setConnected();
    flowManager.egDomainUpdated(epg0->getURI());
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
    flowManager.egDomainUpdated(epg0->getURI());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd(1, 0, 1, false);
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
    flowManager.egDomainUpdated(epg0->getURI());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("enable", 500);

}

BOOST_FIXTURE_TEST_CASE(routemode_vxlan, VxlanFlowManagerFixture) {
    routeModeTest();
}

BOOST_FIXTURE_TEST_CASE(routemode_vlan, VlanFlowManagerFixture) {
    routeModeTest();
}

void FlowManagerFixture::arpModeTest() {
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
    flowManager.egDomainUpdated(epg0->getURI());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0, 1);
    initExpFd(1);
    initExpBd(1, 1, 1, true);
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
    flowManager.egDomainUpdated(epg0->getURI());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0, 1);
    initExpFd(1);
    initExpBd(1, 1, 1, true);
    initExpEp(ep0, epg0, 1, 1, 1, false);
    initExpEp(ep2, epg0, 1, 1, 1, false);
    initSubnets(sns);
    WAIT_FOR_TABLES("disable", 500);

    /* set discovery proxy mode on for ep0 */
    ep0->setDiscoveryProxyMode(true);
    epSrc.updateEndpoint(*ep0);
    flowManager.endpointUpdated(ep0->getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0, 1);
    initExpFd(1);
    initExpBd(1, 1, 1, true);
    initExpEp(ep0, epg0, 1, 1, 1, false);
    initExpEp(ep2, epg0, 1, 1, 1, false);
    initSubnets(sns);
    WAIT_FOR_TABLES("discoveryproxy", 500);
}

BOOST_FIXTURE_TEST_CASE(arpmode_vxlan, VxlanFlowManagerFixture) {
    arpModeTest();
}

BOOST_FIXTURE_TEST_CASE(arpmode_vlan, VlanFlowManagerFixture) {
    arpModeTest();
}

BOOST_FIXTURE_TEST_CASE(localEp, VxlanFlowManagerFixture) {
    setConnected();

    /* created */
    flowManager.endpointUpdated(ep0->getUUID());

    initExpEp(ep0, epg0);
    WAIT_FOR_TABLES("create", 500);

    /* endpoint group change */
    ep0->setEgURI(epg1->getURI());
    epSrc.updateEndpoint(*ep0);
    flowManager.endpointUpdated(ep0->getUUID());

    clearExpFlowTables();
    initExpEp(ep0, epg1, 0, 2);
    WAIT_FOR_TABLES("change epg", 500);

    /* endpoint group changes back to old one */
    ep0->setEgURI(epg0->getURI());
    epSrc.updateEndpoint(*ep0);
    flowManager.endpointUpdated(ep0->getUUID());

    clearExpFlowTables();
    initExpEp(ep0, epg0);
    WAIT_FOR_TABLES("change epg back", 500);

    /* port-mapping change */
    portmapper.ports[ep0->getInterfaceName().get()] = 180;
    flowManager.portStatusUpdate(ep0->getInterfaceName().get(),
                                 180, false);

    clearExpFlowTables();
    initExpEp(ep0, epg0);
    WAIT_FOR_TABLES("change port", 500);

    /* remove endpoint */
    epSrc.removeEndpoint(ep0->getUUID());
    flowManager.endpointUpdated(ep0->getUUID());

    clearExpFlowTables();
    WAIT_FOR_TABLES("remove", 500);
}

BOOST_FIXTURE_TEST_CASE(noifaceEp, VxlanFlowManagerFixture) {
    setConnected();

    /* create */
    flowManager.endpointUpdated(ep2->getUUID());

    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("create", 500);

    /* endpoint group change */
    ep2->setEgURI(epg1->getURI());
    epSrc.updateEndpoint(*ep2);
    flowManager.endpointUpdated(ep2->getUUID());

    initExpEp(ep2, epg1, 0, 2);
    WAIT_FOR_TABLES("change", 500);
}

void FlowManagerFixture::fdTest() {
    setConnected();

    /* create */
    portmapper.ports[ep2->getInterfaceName().get()] = ep2_port;
    portmapper.ports[ep4->getInterfaceName().get()] = ep4_port;
    flowManager.endpointUpdated(ep0->getUUID());
    flowManager.endpointUpdated(ep2->getUUID());

    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("create", 500);

    /* Set FD & update EP0 */
    Mutator m1(framework, policyOwner);
    epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetFloodDomain(fd0->getURI());
    m1.commit();
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);

    exec.Clear();
    exec.ExpectGroup(FlowEdit::add, ge_fd0 + ge_bkt_ep0 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::add, ge_fd0_prom + ge_bkt_tun);

    flowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpFd(1);
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("ep0", 500);

    /* update EP2 */
    exec.Clear();
    exec.ExpectGroup(FlowEdit::mod, ge_fd0 + ge_bkt_ep0 + ge_bkt_ep2
            + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::mod, ge_fd0_prom + ge_bkt_tun);
    flowManager.endpointUpdated(ep2->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpFd(1);
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg0, 1, 1, 1);
    WAIT_FOR_TABLES("ep2", 500);

    /* remove port-mapping for ep2 */
    portmapper.ports.erase(ep2->getInterfaceName().get());
    exec.Clear();
    exec.ExpectGroup(FlowEdit::mod, ge_fd0 + ge_bkt_ep0 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::mod, ge_fd0_prom + ge_bkt_tun);
    flowManager.endpointUpdated(ep2->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpFd(1);
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg0, 1, 1, 1);
    WAIT_FOR_TABLES("removeport", 500);

    /* remove ep0 */
    epSrc.removeEndpoint(ep0->getUUID());
    exec.Clear();
    exec.ExpectGroup(FlowEdit::del, ge_fd0);
    exec.ExpectGroup(FlowEdit::del, ge_fd0_prom);
    flowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    WAIT_FOR_TABLES("removeep", 500);

    /* check promiscous flood */
    WAIT_FOR(policyMgr.getFDForGroup(epg2->getURI()) != boost::none, 500);
    exec.ExpectGroup(FlowEdit::add, ge_fd1 + ge_bkt_ep4 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::add, ge_fd1_prom + ge_bkt_ep4 + ge_bkt_tun);
    flowManager.endpointUpdated(ep4->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    /* group changes on tunnel port change */
    exec.ExpectGroup(FlowEdit::mod, ge_fd1 + ge_bkt_ep4 + ge_bkt_tun_new);
    exec.ExpectGroup(FlowEdit::mod, ge_fd1_prom + ge_bkt_ep4 + ge_bkt_tun_new);
    portmapper.ports[tunIf] = tun_port_new;
    flowManager.portStatusUpdate(tunIf, tun_port_new, false);
    WAIT_FOR(exec.IsGroupEmpty(), 500);
}

BOOST_FIXTURE_TEST_CASE(fd_vxlan, VxlanFlowManagerFixture) {
    fdTest();
}

BOOST_FIXTURE_TEST_CASE(fd_vlan, VlanFlowManagerFixture) {
    fdTest();
}

void FlowManagerFixture::groupFloodTest() {
    flowManager.SetFloodScope(FlowManager::ENDPOINT_GROUP);
    setConnected();

    /* "create" local endpoints ep0 and ep2 */
    portmapper.ports[ep2->getInterfaceName().get()] = ep2_port;
    flowManager.endpointUpdated(ep0->getUUID());
    flowManager.endpointUpdated(ep2->getUUID());

    clearExpFlowTables();
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
    //exec.Expect(FlowEdit::mod, fe_ep0_fd0_1);
    //exec.Expect(FlowEdit::add, fe_ep0_fd0_2);
    exec.ExpectGroup(FlowEdit::add, ge_epg0 + ge_bkt_ep0 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::add, ge_epg0_prom + ge_bkt_tun);
    flowManager.endpointUpdated(ep0->getUUID());

    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpFd(1);
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("ep0", 500);

    exec.Clear();
    exec.ExpectGroup(FlowEdit::add, ge_epg2 + ge_bkt_ep2 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::add, ge_epg2_prom + ge_bkt_tun);
    flowManager.endpointUpdated(ep2->getUUID());

    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpFd(1);
    initExpFd(2);
    initExpEp(ep0, epg0, 1, 1, 1);
    initExpEp(ep2, epg2, 2, 1, 1);
    WAIT_FOR_TABLES("ep2", 500);

    /* remove ep0 */
    epSrc.removeEndpoint(ep0->getUUID());
    exec.Clear();
    exec.ExpectGroup(FlowEdit::del, ge_epg0);
    exec.ExpectGroup(FlowEdit::del, ge_epg0_prom);
    flowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    clearExpFlowTables();
    initExpFd(2);
    initExpEp(ep2, epg2, 2, 1, 1);
    WAIT_FOR_TABLES("remove", 500);
}

BOOST_FIXTURE_TEST_CASE(group_flood_vxlan, VxlanFlowManagerFixture) {
    groupFloodTest();
}

BOOST_FIXTURE_TEST_CASE(group_flood_vlan, VlanFlowManagerFixture) {
    groupFloodTest();
}

BOOST_FIXTURE_TEST_CASE(policy, VxlanFlowManagerFixture) {
    setConnected();

    createPolicyObjects();

    PolicyManager::uri_set_t egs;
    WAIT_FOR_DO(egs.size() == 2, 1000, egs.clear();
                policyMgr.getContractProviders(con1->getURI(), egs));
    egs.clear();
    WAIT_FOR_DO(egs.size() == 2, 500, egs.clear();
                policyMgr.getContractConsumers(con1->getURI(), egs));

    /* add con2 */
    flowManager.contractUpdated(con2->getURI());
    initExpCon2();
    WAIT_FOR_TABLES("con2", 500);

    /* add con1 */
    flowManager.contractUpdated(con1->getURI());
    initExpCon1();
    WAIT_FOR_TABLES("con1", 500);

    /* remove */
    Mutator m2(framework, policyOwner);
    con2->remove();
    m2.commit();
    WAIT_FOR(policyMgr.contractExists(con2->getURI()) == false, 500);
    flowManager.contractUpdated(con2->getURI());

    clearExpFlowTables();
    initExpCon1();
    WAIT_FOR_TABLES("remove", 500);
}

BOOST_FIXTURE_TEST_CASE(policy_portrange, VxlanFlowManagerFixture) {
    setConnected();

    createPolicyObjects();

    PolicyManager::uri_set_t egs;
    WAIT_FOR_DO(egs.size() == 1, 1000, egs.clear();
                policyMgr.getContractProviders(con3->getURI(), egs));
    egs.clear();
    WAIT_FOR_DO(egs.size() == 1, 500, egs.clear();
                policyMgr.getContractConsumers(con3->getURI(), egs));

    /* add con3 */
    flowManager.contractUpdated(con3->getURI());
    initExpCon3();
    WAIT_FOR_TABLES("con3", 500);
}

void FlowManagerFixture::connectTest() {
    flowManager.SetFlowReader(&reader);

    exec.ignoredFlowMods.insert(FlowEdit::add);
    exec.Expect(FlowEdit::del, fe_connect_1);
    exec.Expect(FlowEdit::del, fe_connect_learn);
    exec.Expect(FlowEdit::mod, fe_connect_2);
    exec.ExpectGroup(FlowEdit::del, "group_id=10,type=all");
    flowManager.egDomainUpdated(epg4->getURI());
    setConnected();

    WAIT_FOR(exec.IsEmpty(), 500);

    initExpStatic();
    initExpEpg(epg4, 0, 0);
    WAIT_FOR_TABLES("connect", 500);
}

BOOST_FIXTURE_TEST_CASE(connect_vxlan, VxlanFlowManagerFixture) {
    createOnConnectEntries(FlowManager::ENCAP_VXLAN,
                           reader.flows, reader.groups);
    connectTest();
}

BOOST_FIXTURE_TEST_CASE(connect_vlan, VlanFlowManagerFixture) {
    createOnConnectEntries(FlowManager::ENCAP_VLAN,
                           reader.flows, reader.groups);
    connectTest();
}

void FlowManagerFixture::portStatusTest() {
    setConnected();

    /* create entries for epg0, ep0 and ep2 with original tunnel port */
    flowManager.egDomainUpdated(epg0->getURI());
    flowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("create", 500);

    /* delete all groups except epg0, then update tunnel port */
    PolicyManager::uri_set_t epgURIs;
    policyMgr.getGroups(epgURIs);
    epgURIs.erase(epg0->getURI());
    Mutator m2(framework, policyOwner);
    BOOST_FOREACH (const URI& u, epgURIs) {
        EpGroup::resolve(agent.getFramework(), u).get()->remove();
    }
    m2.commit();
    epgURIs.clear();
    WAIT_FOR_DO(epgURIs.size() == 1, 500,
                epgURIs.clear(); policyMgr.getGroups(epgURIs));
    portmapper.ports[tunIf] = tun_port_new;
    BOOST_FOREACH (const URI& u, epgURIs) {
        flowManager.egDomainUpdated(u);
    }
    flowManager.portStatusUpdate(tunIf, tun_port_new, false);

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
    flowManager.portStatusUpdate(tunIf, tun_port_new, false);

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    WAIT_FOR_TABLES("remove", 500);
}

BOOST_FIXTURE_TEST_CASE(portstatus_vxlan, VxlanFlowManagerFixture) {
    portStatusTest();
}

BOOST_FIXTURE_TEST_CASE(portstatus_vlan, VlanFlowManagerFixture) {
    portStatusTest();
}

BOOST_FIXTURE_TEST_CASE(mcast, VxlanFlowManagerFixture) {
    exec.IgnoreGroupMods();

    MockSwitchConnection conn;
    flowManager.registerConnection(&conn);
    flowManager.setJsonCmdExecutor(&jsonCmdExecutor);
    flowManager.SetFlowReader(&reader);
    flowManager.setTunnelRemotePort(8472);

    string prmBr = " " + conn.getSwitchName() + " 8472 ";

    WAIT_FOR(config->isMulticastGroupIPSet(), 500);
    string mcast1 = config->getMulticastGroupIP().get();
    string mcast2 = "224.1.1.2";
    string mcast3 = "224.5.1.1";
    string mcast4 = "224.5.1.2";

    jsonCmdExecutor.expect("dpif/tnl/igmp-dump" + prmBr,
                           " 224.10.1.10\n\n" + mcast1);
    jsonCmdExecutor.expect("dpif/tnl/igmp-join" + prmBr + mcast1);
    jsonCmdExecutor.expect("dpif/tnl/igmp-leave" + prmBr + "224.10.1.10");
    flowManager.configUpdated(config->getURI());
    setConnected();
    WAIT_FOR(jsonCmdExecutor.empty(), 500);

    jsonCmdExecutor.clear();
    jsonCmdExecutor.expect("dpif/tnl/igmp-join" + prmBr + mcast3);
    flowManager.egDomainUpdated(epg2->getURI());
    WAIT_FOR(jsonCmdExecutor.empty(), 500);


    jsonCmdExecutor.clear();
    jsonCmdExecutor.expect("dpif/tnl/igmp-leave" + prmBr + mcast1);
    jsonCmdExecutor.expect("dpif/tnl/igmp-join" + prmBr + mcast2);

    {
        Mutator mutator(framework, policyOwner);
        config->setMulticastGroupIP(mcast2);
        mutator.commit();
    }

    flowManager.configUpdated(config->getURI());
    flowManager.egDomainUpdated(epg2->getURI());
    WAIT_FOR(jsonCmdExecutor.empty(), 500);

    jsonCmdExecutor.clear();
    jsonCmdExecutor.expect("dpif/tnl/igmp-join" + prmBr + mcast4);
    {
        Mutator mutator(framework, policyOwner);
        fd0ctx->setMulticastGroupIP(mcast4);
        mutator.commit();
    }
    WAIT_FOR_DO(fd0ctx->getMulticastGroupIP("") == mcast4, 500,
        fd0ctx = policyMgr.getFloodContextForGroup(epg2->getURI()).get());

    flowManager.configUpdated(config->getURI());
    flowManager.egDomainUpdated(epg2->getURI());
    WAIT_FOR(jsonCmdExecutor.empty(), 500);

    jsonCmdExecutor.clear();
    jsonCmdExecutor.expect("dpif/tnl/igmp-leave" + prmBr + mcast4);
    {
        Mutator mutator(framework, policyOwner);
        fd0ctx->unsetMulticastGroupIP();
        mutator.commit();
    }
    WAIT_FOR_DO(fd0ctx->isMulticastGroupIPSet() == false, 500,
        fd0ctx = policyMgr.getFloodContextForGroup(epg2->getURI()).get());
    flowManager.egDomainUpdated(epg2->getURI());
    WAIT_FOR(jsonCmdExecutor.empty(), 500);
}

BOOST_FIXTURE_TEST_CASE(ipMapping, VxlanFlowManagerFixture) {
    setConnected();
    flowManager.egDomainUpdated(epg0->getURI());

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
            .setPrefixLen(64);
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

    flowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());
    flowManager.egDomainUpdated(epg0->getURI());
    flowManager.egDomainUpdated(eg_nat->getURI());
    flowManager.endpointUpdated(ep0->getUUID());
    flowManager.endpointUpdated(ep2->getUUID());

    initExpStatic();
    initExpEpg(epg0);
    initExpEpg(eg_nat, 1, 2, 2);
    initExpBd();
    initExpBd(2, 1, 2);
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
    flowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpEpg(eg_nat, 1, 2, 2);
    initExpBd();
    initExpBd(2, 1, 2);
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
    flowManager.endpointUpdated(ep0->getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpEpg(eg_nat, 1, 2, 2);
    initExpBd();
    initExpBd(2, 1, 2);
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpIpMapping(true, true);
    initSubnets(sns, 2, 2);
    WAIT_FOR_TABLES("nexthop", 500);
}

BOOST_FIXTURE_TEST_CASE(service, VxlanFlowManagerFixture) {
    setConnected();
    flowManager.egDomainUpdated(epg0->getURI());
    flowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());
    portmapper.ports["service-iface"] = 17;
    portmapper.RPortMap[17] = "service-iface";

    AnycastService as;
    as.setUUID("ed84daef-1696-4b98-8c80-6b22d85f4dc2");
    as.setServiceMAC(MAC("ed:84:da:ef:16:96"));
    as.setDomainURI(URI(rd0->getURI()));
    as.setInterfaceName("service-iface");

    AnycastService::ServiceMapping sm1;
    sm1.setServiceIP("169.254.169.254");
    sm1.setGatewayIP("169.254.1.1");
    as.addServiceMapping(sm1);

    AnycastService::ServiceMapping sm2;
    sm2.setServiceIP("fe80::a9:fe:a9:fe");
    sm2.setGatewayIP("fe80::1");
    as.addServiceMapping(sm2);

    servSrc.updateAnycastService(as);
    flowManager.anycastServiceUpdated(as.getUUID());

    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpService();
    WAIT_FOR_TABLES("create", 500);

    as.clearServiceMappings();
    sm1.setNextHopIP("169.254.169.1");
    as.addServiceMapping(sm1);
    sm2.setNextHopIP("fe80::a9:fe:a9:1");
    as.addServiceMapping(sm2);

    servSrc.updateAnycastService(as);
    flowManager.anycastServiceUpdated(as.getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpService(true);
    WAIT_FOR_TABLES("nexthop", 500);
}

BOOST_FIXTURE_TEST_CASE(vip, VxlanFlowManagerFixture) {
    setConnected();
    flowManager.egDomainUpdated(epg0->getURI());
    flowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:42"), "42.42.42.42"));
    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:42"), "42::42"));
    ep0->addVirtualIP(make_pair(MAC("00:00:00:00:80:00"), "10.20.44.3"));
    epSrc.updateEndpoint(*ep0);
    flowManager.endpointUpdated(ep0->getUUID());

    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpEp(ep2, epg0);
    initExpVirtualIp();
    WAIT_FOR_TABLES("create", 500);
}

BOOST_FIXTURE_TEST_CASE(virtDhcp, VxlanFlowManagerFixture) {
    setConnected();
    flowManager.egDomainUpdated(epg0->getURI());
    flowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    Endpoint::DHCPv4Config v4;
    Endpoint::DHCPv6Config v6;

    ep0->setDHCPv4Config(v4);
    ep0->setDHCPv6Config(v6);
    epSrc.updateEndpoint(*ep0);
    flowManager.endpointUpdated(ep0->getUUID());

    initExpStatic();
    initExpEpg(epg0);
    initExpBd();
    initExpRd();
    initExpEp(ep0, epg0);
    initExpVirtualDhcp(false);
    WAIT_FOR_TABLES("create", 500);

    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:42"), "42.42.42.42"));
    ep0->addVirtualIP(make_pair(MAC("42:42:42:42:42:42"), "42::42"));
    epSrc.updateEndpoint(*ep0);
    flowManager.endpointUpdated(ep0->getUUID());

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

enum REG {
    SEPG, SEPG12, DEPG, BD, FD, RD, OUTPORT, TUNID, TUNDST, VLAN,
    ETHSRC, ETHDST, ARPOP, ARPSHA, ARPTHA, ARPSPA, ARPTPA, METADATA,
    PKT_MARK
};
enum TABLE {
    SEC = 0, SRC = 1, BR  = 2, RT  = 3, NAT = 4, LRN = 5,
    SVS = 6, SVD = 7, POL = 8, OUT = 9
};
string rstr[] = {
    "NXM_NX_REG0[]", "NXM_NX_REG0[0..11]", "NXM_NX_REG2[]", "NXM_NX_REG4[]",
    "NXM_NX_REG5[]", "NXM_NX_REG6[]", "NXM_NX_REG7[]", "NXM_NX_TUN_ID[0..31]",
    "NXM_NX_TUN_IPV4_DST[]", "OXM_OF_VLAN_VID[]",
    "NXM_OF_ETH_SRC[]", "NXM_OF_ETH_DST[]", "NXM_OF_ARP_OP[]",
    "NXM_NX_ARP_SHA[]", "NXM_NX_ARP_THA[]", "NXM_OF_ARP_SPA[]", "NXM_OF_ARP_TPA[]",
    "OXM_OF_METADATA[]", "NXM_NX_PKT_MARK[]"
};
string rstr1[] = { "reg0", "reg0", "reg2", "reg4", "reg5", "reg6", "reg7",
                   "", "", "", "", "", "", "" ,"", "", "", "", ""};

/**
 * Helper class to build string representation of a flow entry.
 */
class Bldr {
public:
    Bldr(const string& init="") : entry(init) {
        cntr = 0;
        if (entry.empty()) {
            entry = "cookie=0x0, duration=0s, table=0, n_packets=0, "
                    "n_bytes=0, idle_age=0, priority=0";
        }
    }
    string done() { cntr = 0; return entry; }
    Bldr& table(uint8_t t) { rep(", table=", str(t)); return *this; }
    Bldr& priority(uint16_t p) { rep(", priority=", str(p)); return *this; }
    Bldr& cookie(uint64_t c) { rep("cookie=", str64(c, true)); return *this; }
    Bldr& tunId(uint32_t id) { rep(",tun_id=", str(id, true)); return *this; }
    Bldr& in(uint32_t p) { rep(",in_port=", str(p)); return *this; }
    Bldr& reg(REG r, uint32_t v) {
        rep("," + rstr1[r] + "=", str(v, true)); return *this;
    }
    Bldr& isEthSrc(const string& s) { rep(",dl_src=", s); return *this; }
    Bldr& isEthDst(const string& s) { rep(",dl_dst=", s); return *this; }
    Bldr& ip() { rep(",ip"); return *this; }
    Bldr& ipv6() { rep(",ipv6"); return *this; }
    Bldr& arp() { rep(",arp"); return *this; }
    Bldr& tcp() { rep(",tcp"); return *this; }
    Bldr& udp() { rep(",udp"); return *this; }
    Bldr& udp6() { rep(",udp6"); return *this; }
    Bldr& icmp6() { rep(",icmp6"); return *this; }
    Bldr& icmp_type(uint8_t t) { rep(",icmp_type=", str(t)); return *this; }
    Bldr& icmp_code(uint8_t c) { rep(",icmp_code=", str(c)); return *this; }
    Bldr& isArpOp(uint8_t op) { rep(",arp_op=", str(op)); return *this; }
    Bldr& isSpa(const string& s) { rep(",arp_spa=", s); return *this; }
    Bldr& isTpa(const string& s) { rep(",arp_tpa=", s); return *this; }
    Bldr& isIpSrc(const string& s) { rep(",nw_src=", s); return *this; }
    Bldr& isIpv6Src(const string& s) { rep(",ipv6_src=", s); return *this; }
    Bldr& isIpDst(const string& s) { rep(",nw_dst=", s); return *this; }
    Bldr& isIpv6Dst(const string& s) { rep(",ipv6_dst=", s); return *this; }
    Bldr& isTpSrc(uint16_t p) { rep(",tp_src=", str(p)); return *this; }
    Bldr& isTpDst(uint16_t p) { rep(",tp_dst=", str(p)); return *this; }
    Bldr& isTpSrc(uint16_t p, uint16_t m) {
        rep(",tp_src=", str(p, true) + "/" + str(m, true)); return *this;
    }
    Bldr& isTpDst(uint16_t p, uint16_t m) {
        rep(",tp_dst=", str(p, true) + "/" + str(m, true)); return *this;
    }
    Bldr& isVlan(uint16_t v) { rep(",dl_vlan=", str(v)); return *this; }
    Bldr& isNdTarget(const string& t) { rep(",nd_target=", t); return *this; }
    Bldr& isEth(uint16_t t)  { rep(",dl_type=", str(t, true)); return *this; }
    Bldr& isTcpFlags(const string& s)  { rep(",tcp_flags=", s); return *this; }
    Bldr& connState(const string& s) { rep(",conn_state=" + s); return *this; }
    Bldr& isMdAct(uint8_t a) { rep(",metadata=", str(a, true), "/0xff"); return *this; }
    Bldr& isPktMark(uint32_t m) { rep(",pkt_mark=", str(m, true)); return *this; }
    Bldr& actions() { rep(" actions="); cntr = 1; return *this; }
    Bldr& drop() { rep("drop"); return *this; }
    Bldr& load64(REG r, uint64_t v) {
        rep("load:", str64(v, true), "->" + rstr[r]); return *this;
    }
    Bldr& load(REG r, uint32_t v) {
        rep("load:", str(v, true), "->" + rstr[r]); return *this;
    }
    Bldr& load(REG r, const string& v) {
        rep("load:", v, "->" + rstr[r]); return *this;
    }
    Bldr& move(REG s, REG d) {
        rep("move:"+rstr[s]+"->"+rstr[d]); return *this;
    }
    Bldr& ethSrc(const string& s) { rep("set_field:", s, "->eth_src"); return *this; }
    Bldr& ethDst(const string& s) { rep("set_field:", s, "->eth_dst"); return *this; }
    Bldr& ipSrc(const string& s) { rep("mod_nw_src:", s); return *this; }
    Bldr& ipDst(const string& s) { rep("mod_nw_dst:", s); return *this; }
    Bldr& ipv6Src(const string& s) { rep("set_field:", s, "->ipv6_src"); return *this; }
    Bldr& ipv6Dst(const string& s) { rep("set_field:", s, "->ipv6_dst"); return *this; }
    Bldr& go(uint8_t t) { rep("goto_table:", str(t)); return *this; }
    Bldr& out(REG r) { rep("output:" + rstr[r]); return *this; }
    Bldr& decTtl() { rep("dec_ttl"); return *this; }
    Bldr& conntrack(const string& f, uint16_t z) {
        rep("ct(", f + ",zone=" + str(z), ")"); return *this;
    }
    Bldr& group(uint32_t g) { rep("group:", str(g)); return *this; }
    Bldr& bktId(uint32_t b) { rep("bucket_id:", str(b)); return *this; }
    Bldr& bktActions() { rep(",actions="); cntr = 1; return *this; }
    Bldr& outPort(uint32_t p) { rep("output:", str(p)); return *this; }
    Bldr& pushVlan() { rep("push_vlan:0x8100"); return *this; }
    Bldr& popVlan() { rep("pop_vlan"); return *this; }
    Bldr& inport() { rep("IN_PORT"); return *this; }
    Bldr& controller(uint16_t len) { rep("CONTROLLER:", str(len)); return *this; }
    Bldr& mdAct(uint8_t a) { rep("write_metadata:", str(a, true), "/0xff"); return *this; }
    Bldr& resubmit(uint8_t t) { rep("resubmit(,", str(t), ")"); return *this; }

private:
    /**
     * Matches a field in the entry string using prefix and optional
     * suffix, and replaces the value-part with the given value. If
     * field is not found, appends it to the end.
     */
    void rep(const string& pfx, const string& val="", const string& sfx="") {
        string sep(cntr == 2 ? "," : "");
        if (cntr == 1) {
            cntr = 2;
        }
        size_t pos2 = 0;
        do {
            size_t pos1 = entry.find(pfx, pos2);
            if (pos1 == string::npos) {
                entry.append(sep).append(pfx).append(val).append(sfx);
                return;
            }
            pos2 = pos1+pfx.size();
            while (pos2 < entry.size() && entry[pos2] != ',' &&
                   entry[pos2] != ' ' && entry[pos2] != '-') {
                ++pos2;
            }
            if (!sfx.empty()) {
                if (pos2 < entry.size() &&
                    entry.compare(pos2, sfx.size(), sfx)) {
                    continue;
                }
            }
            string tmp;
            if (pos1 > 0)
                tmp += entry.substr(0, pos1) + sep;
            tmp += pfx + val + entry.substr(pos2);
            entry = tmp;
            return;
        } while(true);
    }
    string str(int i, bool hex = false) {
        char buf[20];
        sprintf(buf, hex && i != 0 ? "0x%x" : "%d", i);
        return buf;
    }
    string str64(uint64_t i, bool hex = false) {
        char buf[20];
        sprintf(buf, hex && i != 0 ? "0x%lx" : "%lu", i);
        return buf;
    }
    string strpad(int i) {
        char buf[10];
        sprintf(buf, "0x%04x", i);
        return buf;
    }

    string entry;
    int cntr;
};

#define ADDF(flow) addExpFlowEntry(expTables, flow)

void FlowManagerFixture::initExpStatic() {
    FlowManager::EncapType encapType = flowManager.GetEncapType();
    uint32_t tunPort = flowManager.GetTunnelPort();
    address tunDstAddr = flowManager.GetTunnelDst();
    uint32_t tunDst = tunDstAddr.to_v4().to_ulong();

    ADDF(Bldr().table(SEC).priority(25).arp().actions().drop().done());
    ADDF(Bldr().table(SEC).priority(25).ip().actions().drop().done());
    ADDF(Bldr().table(SEC).priority(25).ipv6().actions().drop().done());
    ADDF(Bldr().table(SEC).priority(27).udp().isTpSrc(68).isTpDst(67)
         .actions().go(SRC).done());
    ADDF(Bldr().table(SEC).priority(27).udp6().isTpSrc(546).isTpDst(547)
         .actions().go(SRC).done());
    ADDF(Bldr().table(SEC).priority(27).icmp6().icmp_type(133).icmp_code(0)
         .actions().go(SRC).done());
    ADDF(Bldr().table(OUT).priority(1).isMdAct(0)
         .actions().out(OUTPORT).done());

    if (tunPort != OFPP_NONE)
        ADDF(Bldr().table(SEC).priority(50).in(tunPort)
             .actions().go(SRC).done());

    if (tunPort != OFPP_NONE) {
        switch (encapType) {
        case FlowManager::ENCAP_VLAN:
            ADDF(Bldr().table(BR).priority(1)
                 .actions().pushVlan().move(SEPG12, VLAN).outPort(tunPort).done());
            ADDF(Bldr().table(RT).priority(1)
                 .actions().pushVlan().move(SEPG12, VLAN).outPort(tunPort).done());
            break;
        case FlowManager::ENCAP_VXLAN:
        case FlowManager::ENCAP_IVXLAN:
        default:
            ADDF(Bldr().table(BR).priority(1)
                 .actions().move(SEPG, TUNID).load(TUNDST, tunDst)
                 .outPort(tunPort).done());
            ADDF(Bldr().table(RT).priority(1)
                 .actions().move(SEPG, TUNID).load(TUNDST, tunDst)
                 .outPort(tunPort).done());
            break;
        }
    }
}

// Initialize EPG-scoped flow entries
void FlowManagerFixture::initExpEpg(shared_ptr<EpGroup>& epg,
                                    uint32_t fdId, uint32_t bdId, uint32_t rdId) {
    FlowManager::EncapType encapType = flowManager.GetEncapType();
    uint32_t tunPort = flowManager.GetTunnelPort();
    uint32_t vnid = policyMgr.getVnidForGroup(epg->getURI()).get();

    if (tunPort != OFPP_NONE) {
        switch (encapType) {
        case FlowManager::ENCAP_VLAN:
            ADDF(Bldr().table(SRC).priority(149)
                 .in(tunPort).isVlan(vnid)
                 .actions().popVlan()
                 .load(SEPG, vnid).load(BD, bdId)
                 .load(FD, fdId).load(RD, rdId).go(BR).done());
            break;
        case FlowManager::ENCAP_VXLAN:
        case FlowManager::ENCAP_IVXLAN:
        default:
            ADDF(Bldr().table(SRC).priority(149).tunId(vnid)
                 .in(tunPort).actions().load(SEPG, vnid).load(BD, bdId)
                 .load(FD, fdId).load(RD, rdId).go(BR).done());
            break;
        }
    }
    ADDF(Bldr().table(POL).priority(100).reg(SEPG, vnid)
         .reg(DEPG, vnid).actions().go(OUT).done());
    ADDF(Bldr().table(OUT).priority(10)
         .reg(OUTPORT, vnid).isMdAct(0x1)
         .actions().load(SEPG, vnid).load(BD, bdId).load(FD, fdId).load(RD, rdId)
         .load(OUTPORT, 0).load(METADATA, 0).resubmit(BR).done());
}

// Initialize flood domain-scoped flow entries
void FlowManagerFixture::initExpFd(uint32_t fdId) {
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    if (fdId != 0)
        ADDF(Bldr().table(BR).priority(10).reg(FD, fdId)
             .isEthDst(mmac).actions().group(fdId).done());
}

// Initialize bridge domain-scoped flow entries
void FlowManagerFixture::initExpBd(uint32_t bdId, uint32_t fdId,
                                   uint32_t rdId, bool routeOn) {
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    if (routeOn) {
        /* Go to routing table */
        ADDF(Bldr().table(BR).priority(2).reg(BD, bdId).actions().go(RT).done());

        /* router solicitation */
        ADDF(Bldr().cookie(htonll(FlowManager::GetNDCookie()))
             .table(BR).priority(20).icmp6().reg(BD, bdId).reg(RD, rdId)
             .isEthDst(mmac).icmp_type(133).icmp_code(0)
             .actions().controller(65535)
             .done());
    }
}

// Initialize routing domain-scoped flow entries
void FlowManagerFixture::initExpRd(uint32_t rdId) {
    FlowManager::EncapType encapType = flowManager.GetEncapType();
    uint32_t tunPort = flowManager.GetTunnelPort();
    address tunDstAddr = flowManager.GetTunnelDst();
    uint32_t tunDst = tunDstAddr.to_v4().to_ulong();

    if (tunPort != OFPP_NONE) {
        switch (encapType) {
        case FlowManager::ENCAP_VLAN:
            ADDF(Bldr().table(RT).priority(324)
                 .ip().reg(RD, rdId).isIpDst("10.20.44.0/24")
                 .actions().pushVlan().move(SEPG12, VLAN)
                 .outPort(tunPort).done());
            ADDF(Bldr().table(RT).priority(324)
                 .ip().reg(RD, rdId).isIpDst("10.20.45.0/24")
                 .actions().pushVlan().move(SEPG12, VLAN)
                 .outPort(tunPort).done());
            ADDF(Bldr().table(RT).priority(332)
                 .ipv6().reg(RD, rdId).isIpv6Dst("2001:db8::/32")
                 .actions().pushVlan().move(SEPG12, VLAN)
                 .outPort(tunPort).done());
            break;
        case FlowManager::ENCAP_VXLAN:
        case FlowManager::ENCAP_IVXLAN:
        default:
            ADDF(Bldr().table(RT).priority(324)
                 .ip().reg(RD, rdId).isIpDst("10.20.44.0/24")
                 .actions().move(SEPG, TUNID).load(TUNDST, tunDst)
                 .outPort(tunPort).done());
            ADDF(Bldr().table(RT).priority(324)
                 .ip().reg(RD, rdId).isIpDst("10.20.45.0/24")
                 .actions().move(SEPG, TUNID).load(TUNDST, tunDst)
                 .outPort(tunPort).done());
            ADDF(Bldr().table(RT).priority(332)
                 .ipv6().reg(RD, rdId).isIpv6Dst("2001:db8::/32")
                 .actions().move(SEPG, TUNID).load(TUNDST, tunDst)
                 .outPort(tunPort).done());
            break;
        }
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
}

// Initialize endpoint-scoped flow entries
void FlowManagerFixture::initExpEp(shared_ptr<Endpoint>& ep,
                                   shared_ptr<EpGroup>& epg,
                                   uint32_t fdId, uint32_t bdId, uint32_t rdId,
                                   bool arpOn, bool routeOn) {
    string mac = ep->getMAC().get().toString();
    uint32_t port = portmapper.FindPort(ep->getInterfaceName().get());
    const unordered_set<string>& ips = ep->getIPs();
    uint32_t vnid = policyMgr.getVnidForGroup(epg->getURI()).get();
    uint32_t tunPort = flowManager.GetTunnelPort();
    address tunDstAddr = flowManager.GetTunnelDst();
    uint32_t tunDst = tunDstAddr.to_v4().to_ulong();
    FlowManager::EncapType encapType = flowManager.GetEncapType();

    string bmac("ff:ff:ff:ff:ff:ff");
    uint8_t rmacArr[6];
    memcpy(rmacArr, flowManager.GetRouterMacAddr(), sizeof(rmacArr));
    string rmac = MAC(rmacArr).toString();
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    // source rules
    if (port != OFPP_NONE) {
        ADDF(Bldr().table(SEC).priority(20).in(port).isEthSrc(mac)
             .actions().go(SRC).done());

        BOOST_FOREACH(const string& ip, ips) {
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
             .load(FD, fdId).load(RD, rdId).go(BR).done());
    }

    // dest rules
    if (port != OFPP_NONE) {
        // bridge
        ADDF(Bldr().table(BR).priority(10).reg(BD, bdId)
             .isEthDst(mac).actions().load(DEPG, vnid)
             .load(OUTPORT, port).go(POL).done());

        if (routeOn) {
            BOOST_FOREACH(const string& ip, ips) {
                address ipa = address::from_string(ip);
                if (ipa.is_v4()) {
                    // route
                    ADDF(Bldr().table(RT).priority(500).ip()
                         .reg(RD, rdId)
                         .isEthDst(rmac).isIpDst(ip)
                         .actions().load(DEPG, vnid)
                         .load(OUTPORT, port).ethSrc(rmac)
                         .ethDst(mac).decTtl().go(POL).done());
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
                        case FlowManager::ENCAP_VLAN:
                            ADDF(Bldr().table(BR).priority(21).arp()
                                 .reg(BD, bdId).reg(RD, rdId).in(tunPort)
                                 .isEthDst(bmac).isTpa(ipa.to_string())
                                 .isArpOp(1)
                                 .actions().move(ETHSRC, ETHDST)
                                 .load(ETHSRC, "0x8000").load(ARPOP, 2)
                                 .move(ARPSHA, ARPTHA).load(ARPSHA, "0x8000")
                                 .move(ARPSPA, ARPTPA)
                                 .load(ARPSPA, ipa.to_v4().to_ulong())
                                 .pushVlan().move(SEPG12, VLAN)
                                 .inport().done());
                            break;
                        case FlowManager::ENCAP_VXLAN:
                        case FlowManager::ENCAP_IVXLAN:
                        default:
                            ADDF(Bldr().table(BR).priority(21).arp()
                                 .reg(BD, bdId).reg(RD, rdId).in(tunPort)
                                 .isEthDst(bmac).isTpa(ipa.to_string())
                                 .isArpOp(1)
                                 .actions().move(ETHSRC, ETHDST)
                                 .load(ETHSRC, "0x8000").load(ARPOP, 2)
                                 .move(ARPSHA, ARPTHA).load(ARPSHA, "0x8000")
                                 .move(ARPSPA, ARPTPA)
                                 .load(ARPSPA, ipa.to_v4().to_ulong())
                                 .move(SEPG, TUNID).load(TUNDST, tunDst)
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
                    ADDF(Bldr().table(RT).priority(500).ipv6()
                         .reg(RD, rdId)
                         .isEthDst(rmac).isIpv6Dst(ip)
                         .actions().load(DEPG, vnid)
                         .load(OUTPORT, port).ethSrc(rmac)
                         .ethDst(mac).decTtl().go(POL).done());
                    if (ep->isDiscoveryProxyMode()) {
                        // proxy neighbor discovery
                        ADDF(Bldr().cookie(htonll(FlowManager::GetNDCookie()))
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
        ep0->getMAC().get().toUIntArray((uint8_t*)&metadata);
        ((uint8_t*)&metadata)[7] = 1;

        BOOST_FOREACH(const string& ip, ips) {
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
                ADDF(Bldr().cookie(htonll(FlowManager::GetNDCookie()))
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
}

void FlowManagerFixture::initSubnets(PolicyManager::subnet_vector_t sns,
                                     uint32_t bdId, uint32_t rdId) {
    string bmac("ff:ff:ff:ff:ff:ff");
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    /* Router entries when epg0 is connected to fd0
     */
    BOOST_FOREACH (shared_ptr<Subnet>& sn, sns) {
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
                 .cookie(htonll(FlowManager::GetNDCookie()))
                 .table(BR).priority(20).icmp6()
                 .reg(BD, bdId).reg(RD, rdId)
                 .isEthDst(mmac).icmp_type(135).icmp_code(0)
                 .isNdTarget(rip.to_string())
                 .actions().controller(65535)
                 .done());
        } else {
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

typedef unordered_map<uint32_t, uint32_t>::value_type IdKeyValue;

void FlowManagerFixture::initExpCon1() {
    uint16_t prio = FlowManager::MAX_POLICY_RULE_PRIORITY;
    PolicyManager::uri_set_t ps, cs;
    unordered_map<uint32_t, uint32_t> pvnids, cvnids;

    policyMgr.getContractProviders(con1->getURI(), ps);
    policyMgr.getContractConsumers(con1->getURI(), cs);
    flowManager.getGroupVnidAndRdId(ps, pvnids);
    flowManager.getGroupVnidAndRdId(cs, cvnids);
    uint32_t con1_cookie =
        flowManager.GetId(con1->getClassId(), con1->getURI());

    BOOST_FOREACH(const IdKeyValue& pid, pvnids) {
        uint32_t pvnid = pid.first;
        BOOST_FOREACH(const IdKeyValue& cid, cvnids) {
            uint32_t cvnid = cid.first;
#if 0
            uint16_t ctzone = 1;
#endif
            /* classifer 1  */
            ADDF(Bldr().table(POL).priority(prio)
                 .cookie(con1_cookie).tcp()
                 .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpDst(80)
                 .actions()
#if 0
                 .conntrack("commit", ctzone)
#endif
                 .go(OUT)
                 .done());
#if 0
            ADDF(Bldr().table(POL).priority(prio)
                 .cookie(con1_cookie).connState("-trk").tcp()
                 .reg(SEPG, pvnid).reg(DEPG, cvnid)
                 .actions().conntrack("recirc", ctzone).done());
            ADDF(Bldr().table(POL).priority(prio)
                 .cookie(con1_cookie).connState("-new+est+trk").tcp()
                 .reg(SEPG, pvnid).reg(DEPG, cvnid)
                 .actions().go(OUT).done());
#endif
            /* classifier 2  */
            ADDF(Bldr().table(POL).priority(prio-1)
                 .cookie(con1_cookie).arp()
                 .reg(SEPG, pvnid).reg(DEPG, cvnid)
                 .actions().go(OUT).done());
            /* classifier 6 */
            ADDF(Bldr().table(POL).priority(prio-2)
                 .cookie(con1_cookie).tcp()
                 .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpSrc(22)
                 .isTcpFlags("+syn+ack").actions().go(OUT).done());
            /* classifier 7 */
            ADDF(Bldr().table(POL).priority(prio-3)
                 .cookie(con1_cookie).tcp()
                 .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpSrc(21)
                 .isTcpFlags("+ack").actions().go(OUT).done());
            ADDF(Bldr().table(POL).priority(prio-3)
                 .cookie(con1_cookie).tcp()
                 .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpSrc(21)
                 .isTcpFlags("+rst").actions().go(OUT).done());
        }
    }
}

void FlowManagerFixture::initExpCon2() {
    uint16_t prio = FlowManager::MAX_POLICY_RULE_PRIORITY;
    PolicyManager::uri_set_t ps, cs;
    unordered_map<uint32_t, uint32_t> pvnids, cvnids;
    uint32_t con2_cookie =
        flowManager.GetId(con2->getClassId(), con2->getURI());

    policyMgr.getContractProviders(con2->getURI(), ps);
    flowManager.getGroupVnidAndRdId(ps, pvnids);
    BOOST_FOREACH (const IdKeyValue& pvnid, pvnids) {
        BOOST_FOREACH (const IdKeyValue& cvnid, pvnids) {
            ADDF(Bldr().table(POL).priority(prio).cookie(con2_cookie)
                 .reg(SEPG, cvnid.first).reg(DEPG, pvnid.first).isEth(0x8906)
                 .actions().go(OUT).done());
        }
    }
}

void FlowManagerFixture::initExpCon3() {
    uint32_t epg0_vnid = policyMgr.getVnidForGroup(epg0->getURI()).get();
    uint32_t epg1_vnid = policyMgr.getVnidForGroup(epg1->getURI()).get();
    uint16_t prio = FlowManager::MAX_POLICY_RULE_PRIORITY;
    PolicyManager::uri_set_t ps, cs;
    unordered_map<uint32_t, uint32_t> pvnids, cvnids;

    uint32_t con3_cookie =
        flowManager.GetId(con3->getClassId(), con3->getURI());
    MaskList ml_80_85 = list_of<Mask>(0x0050, 0xfffc)(0x0054, 0xfffe);
    MaskList ml_66_69 = list_of<Mask>(0x0042, 0xfffe)(0x0044, 0xfffe);
    MaskList ml_94_95 = list_of<Mask>(0x005e, 0xfffe);
    BOOST_FOREACH (const Mask& mk, ml_80_85) {
        ADDF(Bldr().table(POL).priority(prio)
             .cookie(con3_cookie).tcp()
             .reg(SEPG, epg1_vnid).reg(DEPG, epg0_vnid)
             .isTpDst(mk.first, mk.second).actions().drop().done());
    }
    BOOST_FOREACH (const Mask& mks, ml_66_69) {
        BOOST_FOREACH (const Mask& mkd, ml_94_95) {
            ADDF(Bldr().table(POL).priority(prio-1)
                 .cookie(con3_cookie).tcp()
                 .reg(SEPG, epg1_vnid).reg(DEPG, epg0_vnid)
                 .isTpSrc(mks.first, mks.second).isTpDst(mkd.first, mkd.second)
                 .actions().go(OUT).done());
        }
    }
}

// Initialize flows related to IP address mapping/NAT
void FlowManagerFixture::initExpIpMapping(bool natEpgMap, bool nextHop) {
    uint8_t rmacArr[6];
    memcpy(rmacArr, flowManager.GetRouterMacAddr(), sizeof(rmacArr));
    string rmac(MAC(rmacArr).toString());
    uint32_t port = portmapper.FindPort(ep0->getInterfaceName().get());
    string epmac(ep0->getMAC().get().toString());
    string bmac("ff:ff:ff:ff:ff:ff");
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    uint32_t tunPort = flowManager.GetTunnelPort();
    address tunDstAddr = flowManager.GetTunnelDst();
    uint32_t tunDst = tunDstAddr.to_v4().to_ulong();

    ADDF(Bldr().table(RT).priority(452).ip().reg(SEPG, 0x4242).reg(RD, 2)
         .isIpDst("5.5.5.5")
         .actions().ethSrc(rmac).ethDst(epmac)
         .ipDst("10.20.44.2").decTtl()
         .load(DEPG, 0xa0a).load(BD, 1).load(FD, 0)
         .load(RD, 1).load(OUTPORT, port).go(NAT).done());
    ADDF(Bldr().table(RT).priority(450).ip().reg(RD, 2)
         .isIpDst("5.5.5.5")
         .actions().load(DEPG, 0x4242).load(OUTPORT, 0x4242).mdAct(0x1)
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
         .move(SEPG, TUNID).load(TUNDST, tunDst)
         .inport().done());

    ADDF(Bldr().table(RT).priority(452).ipv6().reg(SEPG, 0x4242).reg(RD, 2)
         .isIpv6Dst("fdf1:9f86:d1af:6cc9::5")
         .actions().ethSrc(rmac).ethDst(epmac)
         .ipv6Dst("2001:db8::2").decTtl()
         .load(DEPG, 0xa0a).load(BD, 1).load(FD, 0)
         .load(RD, 1).load(OUTPORT, port).go(NAT).done());
    ADDF(Bldr().table(RT).priority(450).ipv6().reg(RD, 2)
         .isIpv6Dst("fdf1:9f86:d1af:6cc9::5")
         .actions().load(DEPG, 0x4242).load(OUTPORT, 0x4242).mdAct(0x1)
         .go(POL).done());
    ADDF(Bldr().cookie(htonll(FlowManager::GetNDCookie()))
         .table(BR).priority(20).icmp6()
         .reg(BD, 2).reg(RD, 2).isEthDst(mmac).icmp_type(135).icmp_code(0)
         .isNdTarget("fdf1:9f86:d1af:6cc9::5")
         .actions().load(SEPG, 0x4242).load64(METADATA, 0x100008000000000ll)
         .controller(65535).done());

    if (natEpgMap) {
        ADDF(Bldr().table(RT).priority(166).ipv6().reg(RD, 1)
             .isIpv6Dst("fdf1::/16")
             .actions().load(DEPG, 0x80000001).load(OUTPORT, 0x4242)
             .mdAct(0x2).go(POL).done());
        ADDF(Bldr().table(RT).priority(158).ip().reg(RD, 1)
             .isIpDst("5.0.0.0/8")
             .actions().load(DEPG, 0x80000001).load(OUTPORT, 0x4242)
             .mdAct(0x2).go(POL).done());
    } else {
        ADDF(Bldr().table(RT).priority(166).ipv6().reg(RD, 1)
             .isIpv6Dst("fdf1::/16")
             .actions().move(SEPG, TUNID).load(TUNDST, tunDst)
             .outPort(tunPort).done());
        ADDF(Bldr().table(RT).priority(158).ip().reg(RD, 1)
             .isIpDst("5.0.0.0/8")
             .actions().move(SEPG, TUNID).load(TUNDST, tunDst)
             .outPort(tunPort).done());
    }

    ADDF(Bldr().table(NAT).priority(166).ipv6().reg(RD, 1)
         .isIpv6Src("fdf1::/16").actions().load(SEPG, 0x80000001).go(POL).done());
    ADDF(Bldr().table(NAT).priority(158).ip().reg(RD, 1)
         .isIpSrc("5.0.0.0/8").actions().load(SEPG, 0x80000001).go(POL).done());

    if (nextHop) {
        ADDF(Bldr().table(SRC).priority(201).ip().in(42)
             .isEthSrc("42:00:42:42:42:42").isIpDst("5.5.5.5")
             .actions().ethSrc(rmac).ethDst(epmac).ipDst("10.20.44.2").decTtl()
             .load(DEPG, 0xa0a).load(BD, 1).load(FD, 0).load(RD, 1)
             .load(OUTPORT, 0x50).go(NAT).done());
        ADDF(Bldr().table(SRC).priority(200).isPktMark(1).ip().in(42)
             .isEthSrc("42:00:42:42:42:42").isIpDst("10.20.44.2")
             .actions().ethSrc(rmac)
             .load(DEPG, 0xa0a).load(BD, 1).load(FD, 0).load(RD, 1)
             .load(OUTPORT, 0x50).go(NAT).done());
        ADDF(Bldr().table(SRC).priority(201).ipv6().in(42)
             .isEthSrc("42:00:42:42:42:42").isIpv6Dst("fdf1:9f86:d1af:6cc9::5")
             .actions().ethSrc(rmac).ethDst(epmac).ipv6Dst("2001:db8::2").decTtl()
             .load(DEPG, 0xa0a).load(BD, 1).load(FD, 0).load(RD, 1)
             .load(OUTPORT, 0x50).go(NAT).done());
        ADDF(Bldr().table(SRC).priority(200).isPktMark(1).ipv6().in(42)
             .isEthSrc("42:00:42:42:42:42").isIpv6Dst("2001:db8::2")
             .actions().ethSrc(rmac)
             .load(DEPG, 0xa0a).load(BD, 1).load(FD, 0).load(RD, 1)
             .load(OUTPORT, 0x50).go(NAT).done());
        ADDF(Bldr().table(OUT).priority(10).ipv6()
             .reg(RD, 1).reg(OUTPORT, 0x4242).isMdAct(2).isIpv6Src("2001:db8::2")
             .actions().ethSrc(epmac).ethDst("42:00:42:42:42:42")
             .ipv6Src("fdf1:9f86:d1af:6cc9::5").decTtl()
             .load(PKT_MARK, 1).outPort(42).done());
        ADDF(Bldr().table(OUT).priority(10).ip()
             .reg(RD, 1).reg(OUTPORT, 0x4242).isMdAct(2).isIpSrc("10.20.44.2")
             .actions().ethSrc(epmac).ethDst("42:00:42:42:42:42")
             .ipSrc("5.5.5.5").decTtl()
             .load(PKT_MARK, 1).outPort(42).done());
    } else {
        ADDF(Bldr().table(OUT).priority(10).ipv6()
             .reg(RD, 1).reg(OUTPORT, 0x4242).isMdAct(2).isIpv6Src("2001:db8::2")
             .actions().ethSrc(epmac).ethDst(rmac)
             .ipv6Src("fdf1:9f86:d1af:6cc9::5").decTtl()
             .load(SEPG, 0x4242).load(BD, 2).load(FD, 1).load(RD, 2)
             .load(OUTPORT, 0).load(METADATA, 0).resubmit(2).done());
        ADDF(Bldr().table(OUT).priority(10).ip()
             .reg(RD, 1).reg(OUTPORT, 0x4242).isMdAct(2).isIpSrc("10.20.44.2")
             .actions().ethSrc(epmac).ethDst(rmac)
             .ipSrc("5.5.5.5").decTtl()
             .load(SEPG, 0x4242).load(BD, 2).load(FD, 1).load(RD, 2)
             .load(OUTPORT, 0).load(METADATA, 0).resubmit(2).done());
    }
}

void FlowManagerFixture::initExpService(bool nextHop) {
    string mac = "ed:84:da:ef:16:96";
    string bmac("ff:ff:ff:ff:ff:ff");
    uint8_t rmacArr[6];
    memcpy(rmacArr, flowManager.GetRouterMacAddr(), sizeof(rmacArr));
    string rmac = MAC(rmacArr).toString();
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    ADDF(Bldr().table(SEC).priority(100).in(17)
         .isEthSrc("ed:84:da:ef:16:96")
         .actions().go(SVS).done());
    ADDF(Bldr().table(SVS).priority(100).in(17)
         .isEthSrc("ed:84:da:ef:16:96")
         .actions()
         .load(SEPG, 0).load(BD, 0).load(FD, 0)
         .load(RD, 1).go(SVD).done());
    if (nextHop) {
        ADDF(Bldr().table(BR).priority(50)
             .ip().reg(RD, 1).isIpDst("169.254.169.254")
             .actions()
             .ethSrc(rmac).ethDst(mac)
             .ipDst("169.254.169.1")
             .decTtl().outPort(17).done());
        ADDF(Bldr().table(BR).priority(50)
             .ipv6().reg(RD, 1).isIpv6Dst("fe80::a9:fe:a9:fe")
             .actions()
             .ethSrc(rmac).ethDst(mac)
             .ipv6Dst("fe80::a9:fe:a9:1")
             .decTtl().outPort(17).done());

        ADDF(Bldr().table(SVS).priority(150).ip().in(17)
             .isEthSrc("ed:84:da:ef:16:96")
             .isIpSrc("169.254.169.1")
             .actions().load(RD, 1).ipSrc("169.254.169.254")
             .go(SVD).done());
        ADDF(Bldr().table(SVS).priority(150).ipv6().in(17)
             .isEthSrc("ed:84:da:ef:16:96")
             .isIpv6Src("fe80::a9:fe:a9:1")
             .actions().load(RD, 1).ipv6Src("fe80::a9:fe:a9:fe")
             .go(SVD).done());
    } else {
        ADDF(Bldr().table(BR).priority(50)
             .ip().reg(RD, 1).isIpDst("169.254.169.254")
             .actions()
             .ethSrc(rmac).ethDst(mac)
             .decTtl().outPort(17).done());
        ADDF(Bldr().table(BR).priority(50)
             .ipv6().reg(RD, 1).isIpv6Dst("fe80::a9:fe:a9:fe")
             .actions()
             .ethSrc(rmac).ethDst(mac).decTtl().outPort(17).done());
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
    ADDF(Bldr().cookie(htonll(FlowManager::GetNDCookie()))
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
    ADDF(Bldr().cookie(htonll(FlowManager::GetNDCookie()))
         .table(SVD).priority(31).icmp6()
         .reg(RD, 1).isEthSrc("ed:84:da:ef:16:96")
         .isEthDst(mmac)
         .icmp_type(135).icmp_code(0)
         .isNdTarget("fe80::1")
         .actions()
         .load64(METADATA, 0x1019616efda84edll)
         .controller(65535).done());
}

void FlowManagerFixture::initExpVirtualIp() {
    uint32_t port = portmapper.FindPort(ep0->getInterfaceName().get());
    ADDF(Bldr().cookie(htonll(FlowManager::GetVIPCookie(true)))
         .table(SEC).priority(60).arp().in(port)
         .isEthSrc("42:42:42:42:42:42").isSpa("42.42.42.42")
         .actions().controller(65535).go(SRC).done());
    ADDF(Bldr().cookie(htonll(FlowManager::GetVIPCookie(false)))
         .table(SEC).priority(60).icmp6().in(port)
         .isEthSrc("42:42:42:42:42:42")
         .icmp_type(136).icmp_code(0).isNdTarget("42::42")
         .actions().controller(65535).go(SRC).done());
}

void FlowManagerFixture::initExpVirtualDhcp(bool virtIp) {

    uint32_t port = portmapper.FindPort(ep0->getInterfaceName().get());
    ADDF(Bldr().cookie(htonll(FlowManager::GetDHCPCookie(true)))
         .table(SRC).priority(150).udp().in(port)
         .isEthSrc("00:00:00:00:80:00").isTpSrc(68).isTpDst(67)
         .actions().load(SEPG, 0xa0a).controller(65535).done());
    ADDF(Bldr().cookie(htonll(FlowManager::GetDHCPCookie(false)))
         .table(SRC).priority(150).udp6().in(port)
         .isEthSrc("00:00:00:00:80:00").isTpSrc(546).isTpDst(547)
         .actions().load(SEPG, 0xa0a).controller(65535).done());
    if (virtIp) {
        ADDF(Bldr().cookie(htonll(FlowManager::GetDHCPCookie(true)))
             .table(SRC).priority(150).udp().in(port)
             .isEthSrc("42:42:42:42:42:42").isTpSrc(68).isTpDst(67)
             .actions().load(SEPG, 0xa0a).controller(65535).done());
        ADDF(Bldr().cookie(htonll(FlowManager::GetDHCPCookie(false)))
             .table(SRC).priority(150).udp6().in(port)
             .isEthSrc("42:42:42:42:42:42").isTpSrc(546).isTpDst(547)
             .actions().load(SEPG, 0xa0a).controller(65535).done());
    }
}

/**
 * Create group mod entries for use in tests
 */
void
FlowManagerFixture::createGroupEntries(FlowManager::EncapType encapType) {
    uint32_t tunPort = flowManager.GetTunnelPort();
    address mcastTunDstAddr = address::from_string("224.1.1.1");
    uint32_t mcastTunDst = mcastTunDstAddr.to_v4().to_ulong();
    uint32_t ep0_port = portmapper.FindPort(ep0->getInterfaceName().get());

    /* Group entries */
    string bktInit = ",bucket=";
    ge_fd0 = "group_id=1,type=all";
    ge_bkt_ep0 = Bldr(bktInit).bktId(ep0_port).bktActions().outPort(ep0_port)
            .done();
    ge_bkt_ep2 = Bldr(bktInit).bktId(ep2_port).bktActions().outPort(ep2_port)
            .done();
    switch (encapType) {
    case FlowManager::ENCAP_VLAN:
        ge_bkt_tun = Bldr(bktInit).bktId(tunPort).bktActions()
            .pushVlan().move(SEPG12, VLAN).outPort(tunPort).done();
        break;
    case FlowManager::ENCAP_VXLAN:
    case FlowManager::ENCAP_IVXLAN:
    default:
        ge_bkt_tun = Bldr(bktInit).bktId(tunPort).bktActions().move(SEPG, TUNID)
            .load(TUNDST, mcastTunDst).outPort(tunPort).done();
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

void
FlowManagerFixture::createOnConnectEntries(FlowManager::EncapType encapType,
                                           FlowEntryList& flows,
                                           GroupEdit::EntryList& groups) {
    // Create a pre-existing table as read from a switch
    uint8_t mac[6];

    flows.clear();
    groups.clear();

    uint32_t epg4_vnid = policyMgr.getVnidForGroup(epg4->getURI()).get();

    FlowEntryPtr e0(new FlowEntry());
    flows.push_back(e0);
    e0->entry->table_id = SEC;
    e0->entry->cookie = htonll(0xabcd);

    FlowEntryPtr e1(new FlowEntry());
    flows.push_back(e1);
    e1->entry->table_id = SRC;
    e1->entry->priority = 149;
    match_set_in_port(&e1->entry->match, flowManager.GetTunnelPort());
    switch (encapType) {
    case FlowManager::ENCAP_VLAN:
        match_set_dl_vlan(&e1->entry->match, htons(epg4_vnid));
        break;
    case FlowManager::ENCAP_VXLAN:
    case FlowManager::ENCAP_IVXLAN:
    default:
        match_set_tun_id(&e1->entry->match, htonll(epg4_vnid));
        break;
    }
    ActionBuilder ab;
    if (encapType == FlowManager::ENCAP_VLAN)
        ab.SetPopVlan();
    ab.SetRegLoad(MFF_REG0, epg4_vnid);
    ab.SetRegLoad(MFF_REG4, uint32_t(0));
    ab.SetRegLoad(MFF_REG5, uint32_t(0));
    ab.SetRegLoad(MFF_REG6, uint32_t(1));
    ab.SetGotoTable(BR);
    ab.Build(e1->entry);

    FlowEntryPtr e2(new FlowEntry());
    flows.push_back(e2);
    e2->entry->table_id = POL;
    e2->entry->priority = 100;
    match_set_reg(&e2->entry->match, 0, epg4_vnid);
    match_set_reg(&e2->entry->match, 2, epg4_vnid);

    // invalid learn entry with invalid mac and port
    FlowEntryPtr e3(new FlowEntry());
    flows.push_back(e3);
    e3->entry->table_id = LRN;
    e3->entry->priority = 150;
    e3->entry->cookie = FlowManager::GetLearnEntryCookie();
    MAC("de:ad:be:ef:1:2").toUIntArray(mac);
    match_set_dl_dst(&e3->entry->match, mac);
    ActionBuilder ab3;
    ab3.SetRegLoad(MFF_REG7, 999);
    ab3.SetController();
    ab3.Build(e3->entry);

    // valid learn entry for ep0
    FlowEntryPtr e4(new FlowEntry());
    flows.push_back(e4);
    e4->entry->table_id = LRN;
    e4->entry->priority = 150;
    e4->entry->cookie = FlowManager::GetLearnEntryCookie();
    ep0->getMAC().get().toUIntArray(mac);
    match_set_dl_dst(&e4->entry->match, mac);
    ActionBuilder ab4;
    ab4.SetRegLoad(MFF_REG7, 80);
    ab4.SetController();
    ab4.Build(e4->entry);

    // invalid learn entry with invalid mac and valid port
    FlowEntryPtr e5(new FlowEntry());
    flows.push_back(e5);
    e5->entry->table_id = LRN;
    e5->entry->priority = 150;
    e5->entry->cookie = FlowManager::GetLearnEntryCookie();
    MAC("de:ad:be:ef:1:2").toUIntArray(mac);
    match_set_dl_dst(&e5->entry->match, mac);
    ActionBuilder ab5;
    ab5.SetRegLoad(MFF_REG7, 80);
    ab5.SetController();
    ab5.Build(e5->entry);

    // invalid learn entry with invalid port and valid mac
    FlowEntryPtr e6(new FlowEntry());
    flows.push_back(e6);
    e6->entry->table_id = LRN;
    e6->entry->priority = 150;
    e6->entry->cookie = FlowManager::GetLearnEntryCookie();
    ep0->getMAC().get().toUIntArray(mac);
    match_set_dl_dst(&e6->entry->match, mac);
    ActionBuilder ab6;
    ab6.SetRegLoad(MFF_REG7, 999);
    ab6.SetController();
    ab6.Build(e6->entry);

    // spurious entry in learn table, should be deleted
    FlowEntryPtr e7(new FlowEntry());
    flows.push_back(e7);
    e7->entry->table_id = LRN;
    e7->entry->priority = 8192;
    e7->entry->cookie = htonll(0xabcd);

    GroupEdit::Entry entryIn(new GroupEdit::GroupMod());
    entryIn->mod->command = OFPGC11_ADD;
    entryIn->mod->group_id = 10;
    groups.push_back(entryIn);

    // Flow mods that we expect after diffing against the table
    fe_connect_1 = Bldr().table(SEC).priority(0).cookie(0xabcd)
            .actions().drop().done();
    fe_connect_2 = Bldr().table(POL).priority(100).reg(SEPG, epg4_vnid)
            .reg(DEPG, epg4_vnid).actions().go(OUT).done();

    fe_connect_learn
        .push_back(Bldr().table(LRN).priority(150)
                   .cookie(ntohll(FlowManager::GetLearnEntryCookie()))
                   .isEthDst("de:ad:be:ef:01:02")
                   .actions().load(OUTPORT, 999).controller(0xffff)
                   .done());
    fe_connect_learn
        .push_back(Bldr().table(LRN).priority(150)
                   .cookie(ntohll(FlowManager::GetLearnEntryCookie()))
                   .isEthDst("de:ad:be:ef:01:02")
                   .actions().load(OUTPORT, 80).controller(0xffff)
                   .done());
    fe_connect_learn
        .push_back(Bldr().table(LRN).priority(150)
                   .cookie(ntohll(FlowManager::GetLearnEntryCookie()))
                   .isEthDst(ep0->getMAC().get().toString())
                   .actions().load(OUTPORT, 999).controller(0xffff)
                   .done());
    fe_connect_learn
        .push_back(Bldr().table(LRN).priority(8192).cookie(0xabcd)
                   .actions().drop().done());

}

BOOST_AUTO_TEST_SUITE_END()

string CanonicalizeGroupEntryStr(const string& entryStr) {
    size_t p = 0;
    vector<string> tokens;
    while (p != string::npos) {
        size_t np = entryStr.find(",bucket=", p > 0 ? p+1 : p);
        size_t len = np == string::npos ? string::npos : (np-p);
        tokens.push_back(entryStr.substr(p, len));
        p = np;
    }
    if (tokens.size() > 1) {
        sort(tokens.begin()+1, tokens.end());
    }
    string out;
    BOOST_FOREACH(const string& s, tokens) {
        out.append(s);
    }
    return out;
}
