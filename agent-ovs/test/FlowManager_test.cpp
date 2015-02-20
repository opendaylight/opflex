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

typedef pair<FlowEdit::TYPE, string> MOD;

static string CanonicalizeGroupEntryStr(const string& entryStr);

class MockFlowExecutor : public FlowExecutor {
public:
    MockFlowExecutor(): ignoreFlowModCounter(0) {}
    ~MockFlowExecutor() {}

    bool Execute(const FlowEdit& flowEdits) {
        if (ignoreFlowModCounter != 0) {
            ignoreFlowModCounter -= flowEdits.edits.size();
            return true;
        }
        const char *modStr[] = {"ADD", "MOD", "DEL"};
        struct ds strBuf;
        ds_init(&strBuf);

        BOOST_FOREACH(const FlowEdit::Entry& ed, flowEdits.edits) {
            ofp_print_flow_stats(&strBuf, ed.second->entry);
            string str = (const char*)(ds_cstr(&strBuf)+1); // trim space

            BOOST_CHECK_MESSAGE(!mods.empty(), "\nexp:\ngot: " << ed);
            if (!mods.empty()) {
                MOD exp = mods.front();
                mods.pop_front();
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
        if (ignoreGroupModCounter != 0) {
            ignoreGroupModCounter -= groupEdits.edits.size();
            return true;
        }
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
        ignoreFlowModCounter = 0;
        mods.push_back(MOD(mod, fe));
    }
    void Expect(FlowEdit::TYPE mod, const vector<string>& fe) {
        ignoreFlowModCounter = 0;
        BOOST_FOREACH(const string& s, fe) {
            mods.push_back(MOD(mod, s));
        }
    }
    void ExpectGroup(FlowEdit::TYPE mod, const string& ge) {
        ignoreGroupModCounter = 0;
        const char *modStr[] = {"ADD", "MOD", "DEL"};
        groupMods.push_back(CanonicalizeGroupEntryStr(
                string(modStr[mod]) + "|" + ge));
    }
    void IgnoreFlowMods(int count = -1) {
        ignoreFlowModCounter = count;
        mods.clear();
    }
    void IgnoreGroupMods(int count = -1) {
        ignoreGroupModCounter = count;
        groupMods.clear();
    }
    bool IsEmpty() { return mods.empty() && ignoreFlowModCounter <= 0; }
    bool IsGroupEmpty() {
        return groupMods.empty() && ignoreGroupModCounter <= 0;
    }
    void Clear() {
        mods.clear();
        groupMods.clear();
    }

    std::list<MOD> mods;
    std::list<string> groupMods;
    int ignoreFlowModCounter;
    int ignoreGroupModCounter;
};

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

        PolicyManager::uri_set_t egs;
        WAIT_FOR_DO(egs.size() == 2, 1000,
            egs.clear(); policyMgr.getContractProviders(con1->getURI(), egs));
        egs.clear();
        WAIT_FOR_DO(egs.size() == 2, 500,
            egs.clear(); policyMgr.getContractConsumers(con1->getURI(), egs));

        flowManager.Start();
    }
    virtual ~FlowManagerFixture() {
        flowManager.Stop();
    }
    void setConnected() {
        flowManager.PeerConnected();    // pretend that opflex-peer is present
        flowManager.Connected(NULL);    // force flowManager out of sync-ing
    }
    void createEntriesForObjects(FlowManager::EncapType encapType);
    void createOnConnectEntries(FlowManager::EncapType encapType,
                                FlowEntryList& flows,
                                GroupEdit::EntryList& groups);


    void epgTest();
    void arpModeTest();
    void localEpTest();
    void remoteEpTest();
    void fdTest();
    void groupFloodTest();
    void policyTest();
    void policyPortRangeTest();
    void connectTest();
    void portStatusTest();
    void mcastTest();

    MockFlowExecutor exec;
    MockFlowReader reader;
    FlowManager flowManager;
    MockPortMapper portmapper;
    PolicyManager& policyMgr;
    MockJsonCmdExecutor jsonCmdExecutor;

    string tunIf;
    vector<string> fe_fixed;
    vector<string> fe_fallback;
    vector<string> fe_epg0, fe_epg0_fd0, fe_epg0_routers;
    vector<string> fe_ep0, fe_ep0_fd0_1, fe_ep0_fd0_2;
    vector<string> fe_ep0_routeoff, fe_ep2_routeoff;
    vector<vector<string> > fe_ep0_eg1, fe_ep0_eg0;
    vector<string> fe_ep0_port_1, fe_ep0_port_2, fe_ep0_port_3;
    vector<string> fe_ep2, fe_ep2_eg1, fe_ep2_eg1_arp;
    vector<string> fe_con1, fe_con2, fe_con3;
    vector<string> fe_arpopt;
    vector<string> fe_fixed_tun_new, fe_fallback_tun_new;
    vector<string> fe_epg0_tun_new, fe_ep2_tun_new;
    vector<string> fe_rd0;
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

class VxlanFlowManagerFixture : public FlowManagerFixture {
public:
    VxlanFlowManagerFixture() : FlowManagerFixture() {
        flowManager.SetEncapType(FlowManager::ENCAP_VXLAN);
        createEntriesForObjects(FlowManager::ENCAP_VXLAN);
    }
    virtual ~VxlanFlowManagerFixture() {}
};

class VlanFlowManagerFixture : public FlowManagerFixture {
public:
    VlanFlowManagerFixture() : FlowManagerFixture() {
        flowManager.SetEncapType(FlowManager::ENCAP_VLAN);
        createEntriesForObjects(FlowManager::ENCAP_VLAN);
    }
    virtual ~VlanFlowManagerFixture() {}
};

void FlowManagerFixture::epgTest() {
    setConnected();

    LOG(INFO) << "epg: create";
    /* create */
    exec.Clear();
    exec.Expect(FlowEdit::add, fe_fixed);
    exec.Expect(FlowEdit::add, fe_fallback);
    exec.Expect(FlowEdit::add, fe_epg0);
    exec.Expect(FlowEdit::add, fe_rd0);
    exec.Expect(FlowEdit::add, fe_ep0);
    exec.Expect(FlowEdit::add, fe_ep2);
    flowManager.egDomainUpdated(epg0->getURI());
    WAIT_FOR(exec.IsEmpty(), 500);

    LOG(INFO) << "Disabling routing";

    /* Disable routing on bridge domain */
    exec.Clear();
    exec.Expect(FlowEdit::del, fe_ep0_routeoff);
    exec.Expect(FlowEdit::del, fe_ep2_routeoff);

    {
        Mutator mutator(framework, policyOwner);
        bd0->setRoutingMode(RoutingModeEnumT::CONST_DISABLED);
        mutator.commit();
    }
    flowManager.egDomainUpdated(epg0->getURI());
    WAIT_FOR(exec.IsEmpty(), 500);

    /* Enable routing */
    exec.Clear();
    exec.Expect(FlowEdit::add, fe_ep0_routeoff);
    exec.Expect(FlowEdit::add, fe_ep2_routeoff);
    {
        Mutator mutator(framework, policyOwner);
        bd0->setRoutingMode(RoutingModeEnumT::CONST_ENABLED);
        mutator.commit();
    }
    flowManager.egDomainUpdated(epg0->getURI());
    WAIT_FOR(exec.IsEmpty(), 500);

    LOG(INFO) << "epg: forwarding object change";
    /* forwarding object change */
    Mutator m1(framework, policyOwner);
    epg0->addGbpEpGroupToNetworkRSrc()
        ->setTargetSubnets(subnetsfd0->getURI());
    m1.commit();
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);

    exec.Clear();
    exec.Expect(FlowEdit::mod, fe_epg0_fd0);
    exec.Expect(FlowEdit::add, fe_epg0_routers);
    exec.Expect(FlowEdit::mod, fe_ep0_fd0_1);
    exec.Expect(FlowEdit::add, fe_ep0_fd0_2);
    exec.ExpectGroup(FlowEdit::add, ge_fd0 + ge_bkt_ep0 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::add, ge_fd0_prom + ge_bkt_tun);
    flowManager.egDomainUpdated(epg0->getURI());
    WAIT_FOR(exec.IsEmpty(), 500);
    BOOST_CHECK(exec.IsGroupEmpty());

    /* Don't add tests here that reference ep2 */

    LOG(INFO) << "epg: remove domain objects";
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
    exec.Clear();
    exec.Expect(FlowEdit::del, fe_rd0);
    exec.Expect(FlowEdit::del, fe_epg0_routers);
    flowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());
    BOOST_FOREACH(shared_ptr<Subnet>& sn, subnets_copy) {
        flowManager.domainUpdated(Subnet::CLASS_ID, sn->getURI());
    }
    WAIT_FOR(exec.IsEmpty(), 500);

    LOG(INFO) << "epg: remove";
    /* remove */
    Mutator m2(framework, policyOwner);
    epg0->remove();
    m2.commit();
    WAIT_FOR(policyMgr.groupExists(epg0->getURI()) == false, 500);
    exec.Clear();
    exec.Expect(FlowEdit::del, fe_epg0_fd0);
    exec.Expect(FlowEdit::del, fe_epg0[1]);
    flowManager.egDomainUpdated(epg0->getURI());
    WAIT_FOR(exec.IsEmpty(), 500);
}

BOOST_FIXTURE_TEST_CASE(epg_vxlan, VxlanFlowManagerFixture) {
    epgTest();
}

BOOST_FIXTURE_TEST_CASE(epg_vlan, VlanFlowManagerFixture) {
    epgTest();
}

void FlowManagerFixture::arpModeTest() {
    setConnected();

    /* setup entries for epg0 connected to fd0 */
    exec.IgnoreFlowMods(fe_fixed.size() + fe_fallback.size() +
        fe_epg0.size() + fe_rd0.size() + fe_ep0.size() + fe_ep2.size() +
        fe_epg0_routers.size() + fe_ep0_fd0_2.size());
    exec.IgnoreGroupMods();
    Mutator m1(framework, policyOwner);
    epg0->addGbpEpGroupToNetworkRSrc()
        ->setTargetSubnets(subnetsfd0->getURI());
    m1.commit();
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);
    flowManager.egDomainUpdated(epg0->getURI());
    WAIT_FOR(exec.IsEmpty(), 500);

    /* change arp mode to flood */
    fd0->setArpMode(AddressResModeEnumT::CONST_FLOOD)
        .setNeighborDiscMode(AddressResModeEnumT::CONST_FLOOD);
    m1.commit();
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()).get()
             ->getArpMode(AddressResModeEnumT::CONST_UNICAST)
             == AddressResModeEnumT::CONST_FLOOD, 500);

    exec.Clear();
    exec.Expect(FlowEdit::del, fe_arpopt);
    flowManager.egDomainUpdated(epg0->getURI());
    WAIT_FOR(exec.IsEmpty(), 500);
    BOOST_CHECK(exec.IsGroupEmpty());
}

BOOST_FIXTURE_TEST_CASE(arpmode_vxlan, VxlanFlowManagerFixture) {
    arpModeTest();
}

BOOST_FIXTURE_TEST_CASE(arpmode_vlan, VlanFlowManagerFixture) {
    arpModeTest();
}

void FlowManagerFixture::localEpTest() {
    setConnected();

    /* created */
    exec.Clear();
    exec.Expect(FlowEdit::add, fe_ep0);
    flowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsEmpty(), 500);

    /* endpoint group change */
    ep0->setEgURI(epg1->getURI());
    epSrc.updateEndpoint(*ep0);
    exec.Clear();
    for (size_t i = 0; i < fe_ep0_eg1.size() - 1; i++) {
        exec.Expect(i % 2 == 0 ? FlowEdit::mod : FlowEdit::add,
                    fe_ep0_eg1[i]);
    }
    exec.Expect(FlowEdit::del, *fe_ep0_eg1.rbegin());
    flowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsEmpty(), 500);

    /* endpoint group changes back to old one */
    ep0->setEgURI(epg0->getURI());
    epSrc.updateEndpoint(*ep0);
    exec.Clear();
    for (size_t i = 0; i < fe_ep0_eg0.size() - 1; i++) {
        exec.Expect(i % 2 == 0 ? FlowEdit::mod : FlowEdit::add,
                    fe_ep0_eg0[i]);
    }
    exec.Expect(FlowEdit::del, *fe_ep0_eg0.rbegin());
    flowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsEmpty(), 500);

    /* port-mapping change */
    portmapper.ports[ep0->getInterfaceName().get()] = 180;
    exec.Clear();
    exec.Expect(FlowEdit::add, fe_ep0_port_1);
    for (size_t i = 0; i < fe_ep0_port_1.size(); ++i)  {
        exec.Expect(FlowEdit::del, fe_ep0[i]);
    }
    exec.Expect(FlowEdit::add, fe_ep0_port_2);
    for (size_t i = fe_ep0_port_1.size();
         i < fe_ep0_port_1.size() + fe_ep0_port_2.size();
         ++i) {
        exec.Expect(FlowEdit::del, fe_ep0[i]);
    }
    exec.Expect(FlowEdit::mod, fe_ep0_port_3);
    flowManager.portStatusUpdate(ep0->getInterfaceName().get(),
                                 180, false);
    WAIT_FOR(exec.IsEmpty(), 500);

    /* remove endpoint */
    epSrc.removeEndpoint(ep0->getUUID());
    exec.Clear();
    exec.Expect(FlowEdit::del, fe_ep0_port_1);
    exec.Expect(FlowEdit::del, fe_ep0_port_2);
    exec.Expect(FlowEdit::del, fe_ep0_port_3);
    flowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsEmpty(), 500);
}

BOOST_FIXTURE_TEST_CASE(localEp_vxlan, VxlanFlowManagerFixture) {
    localEpTest();
}

BOOST_FIXTURE_TEST_CASE(localEp_vlan, VlanFlowManagerFixture) {
    localEpTest();
}

void FlowManagerFixture::remoteEpTest() {
    setConnected();

    /* created */
    exec.Expect(FlowEdit::add, fe_ep2);
    flowManager.endpointUpdated(ep2->getUUID());
    WAIT_FOR(exec.IsEmpty(), 500);

    LOG(INFO) << "Changing endpoint group";

    /* endpoint group change */
    ep2->setEgURI(epg1->getURI());
    epSrc.updateEndpoint(*ep2);
    exec.Clear();
    exec.Expect(FlowEdit::add, fe_ep2_eg1);
    exec.Expect(FlowEdit::mod, fe_ep2_eg1_arp);
    exec.Expect(FlowEdit::del, fe_ep2[0]);
    exec.Expect(FlowEdit::del, fe_ep2[1]);
    flowManager.endpointUpdated(ep2->getUUID());
    WAIT_FOR(exec.IsEmpty(), 500);
}

BOOST_FIXTURE_TEST_CASE(remoteEp_vxlan, VxlanFlowManagerFixture) {
    remoteEpTest();
}

BOOST_FIXTURE_TEST_CASE(remoteEp_vlan, VlanFlowManagerFixture) {
    remoteEpTest();
}

void FlowManagerFixture::fdTest() {
    setConnected();

    exec.IgnoreFlowMods(fe_ep0.size() + 7);
    portmapper.ports[ep2->getInterfaceName().get()] = ep2_port;
    portmapper.ports[ep4->getInterfaceName().get()] = ep4_port;
    flowManager.endpointUpdated(ep0->getUUID());
    flowManager.endpointUpdated(ep2->getUUID());
    WAIT_FOR(exec.IsEmpty(), 500);

    Mutator m1(framework, policyOwner);
    epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetSubnets(subnetsfd0->getURI());
    m1.commit();
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);
    exec.Clear();
    exec.Expect(FlowEdit::mod, fe_ep0_fd0_1);
    exec.Expect(FlowEdit::add, fe_ep0_fd0_2);
    exec.ExpectGroup(FlowEdit::add, ge_fd0 + ge_bkt_ep0 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::add, ge_fd0_prom + ge_bkt_tun);
    flowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsEmpty(), 500);
    BOOST_CHECK(exec.IsGroupEmpty());

    exec.IgnoreFlowMods();
    exec.Clear();
    exec.ExpectGroup(FlowEdit::mod, ge_fd0 + ge_bkt_ep0 + ge_bkt_ep2
            + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::mod, ge_fd0_prom + ge_bkt_tun);
    flowManager.endpointUpdated(ep2->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    /* remove port-mapping for ep2 */
    portmapper.ports.erase(ep2->getInterfaceName().get());
    exec.Clear();
    exec.ExpectGroup(FlowEdit::mod, ge_fd0 + ge_bkt_ep0 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::mod, ge_fd0_prom + ge_bkt_tun);
    flowManager.endpointUpdated(ep2->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    /* remove ep0 */
    epSrc.removeEndpoint(ep0->getUUID());
    exec.Clear();
    exec.ExpectGroup(FlowEdit::del, ge_fd0);
    exec.ExpectGroup(FlowEdit::del, ge_fd0_prom);
    flowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

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
    exec.IgnoreFlowMods(fe_ep0.size() + 7);
    portmapper.ports[ep2->getInterfaceName().get()] = ep2_port;
    flowManager.endpointUpdated(ep0->getUUID());
    flowManager.endpointUpdated(ep2->getUUID());
    WAIT_FOR(exec.IsEmpty(), 500);

    /* Move ep2 to epg2, add epg0 to fd0. Now fd0 has 2 epgs */
    ep2->setEgURI(epg2->getURI());
    epSrc.updateEndpoint(*ep2);
    Mutator m1(framework, policyOwner);
    epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetSubnets(subnetsfd0->getURI());
    m1.commit();
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);
    WAIT_FOR(policyMgr.getFDForGroup(epg2->getURI()) != boost::none, 500);
    exec.Clear();
    exec.Expect(FlowEdit::mod, fe_ep0_fd0_1);
    exec.Expect(FlowEdit::add, fe_ep0_fd0_2);
    exec.ExpectGroup(FlowEdit::add, ge_epg0 + ge_bkt_ep0 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::add, ge_epg0_prom + ge_bkt_tun);
    flowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsEmpty(), 500);
    BOOST_CHECK(exec.IsGroupEmpty());

    exec.IgnoreFlowMods();
    exec.Clear();
    exec.ExpectGroup(FlowEdit::add, ge_epg2 + ge_bkt_ep2 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::add, ge_epg2_prom + ge_bkt_tun);
    flowManager.endpointUpdated(ep2->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);

    /* remove ep0 */
    epSrc.removeEndpoint(ep0->getUUID());
    exec.Clear();
    exec.ExpectGroup(FlowEdit::del, ge_epg0);
    exec.ExpectGroup(FlowEdit::del, ge_epg0_prom);
    flowManager.endpointUpdated(ep0->getUUID());
    WAIT_FOR(exec.IsGroupEmpty(), 500);
}

BOOST_FIXTURE_TEST_CASE(group_flood_vxlan, VxlanFlowManagerFixture) {
    groupFloodTest();
}

BOOST_FIXTURE_TEST_CASE(group_flood_vlan, VlanFlowManagerFixture) {
    groupFloodTest();
}

void FlowManagerFixture::policyTest() {
    setConnected();

    exec.Expect(FlowEdit::add, fe_con2);
    flowManager.contractUpdated(con2->getURI());
    WAIT_FOR(exec.IsEmpty(), 500);

    exec.Clear();
    exec.Expect(FlowEdit::add, fe_con1);
    flowManager.contractUpdated(con1->getURI());
    WAIT_FOR(exec.IsEmpty(), 500);

    /* remove */
    Mutator m2(framework, policyOwner);
    con2->remove();
    m2.commit();
    WAIT_FOR(policyMgr.contractExists(con2->getURI()) == false, 500);
    exec.Clear();
    exec.Expect(FlowEdit::del, fe_con2);
    flowManager.contractUpdated(con2->getURI());
    WAIT_FOR(exec.IsEmpty(), 500);
}

BOOST_FIXTURE_TEST_CASE(policy_vxlan, VxlanFlowManagerFixture) {
    policyTest();
}

BOOST_FIXTURE_TEST_CASE(policy_vlan, VlanFlowManagerFixture) {
    policyTest();
}

void FlowManagerFixture::policyPortRangeTest() {
    setConnected();

    exec.Expect(FlowEdit::add, fe_con3);
    flowManager.contractUpdated(con3->getURI());
    WAIT_FOR(exec.IsEmpty(), 500);
}

BOOST_FIXTURE_TEST_CASE(policy_portrange_vxlan, VxlanFlowManagerFixture) {
    policyPortRangeTest();
}

BOOST_FIXTURE_TEST_CASE(policy_portrange_vlan, VlanFlowManagerFixture) {
    policyPortRangeTest();
}

void FlowManagerFixture::connectTest() {
    flowManager.SetFlowReader(&reader);

    exec.Expect(FlowEdit::add, fe_fixed);
    exec.Expect(FlowEdit::del, fe_connect_1);
    exec.Expect(FlowEdit::add, fe_rd0);
    exec.Expect(FlowEdit::add, fe_fallback);
    exec.Expect(FlowEdit::del, fe_connect_learn);
    exec.Expect(FlowEdit::mod, fe_connect_2);
    exec.ExpectGroup(FlowEdit::del, "group_id=10,type=all");
    flowManager.egDomainUpdated(epg4->getURI());
    setConnected();

    WAIT_FOR(exec.IsEmpty(), 500);
}

BOOST_FIXTURE_TEST_CASE(connect_vxlan, VxlanFlowManagerFixture) {
    createOnConnectEntries(FlowManager::ENCAP_VXLAN, reader.flows, reader.groups);
    connectTest();
}

BOOST_FIXTURE_TEST_CASE(connect_vlan, VlanFlowManagerFixture) {
    createOnConnectEntries(FlowManager::ENCAP_VLAN, reader.flows, reader.groups);
    connectTest();
}

void FlowManagerFixture::portStatusTest() {
    setConnected();

    /* create entries for epg0, ep0 and ep2 with original tunnel port */
    exec.Clear();
    exec.IgnoreFlowMods(fe_fixed.size() + fe_fallback.size()
            + fe_epg0.size() + fe_ep0.size() + fe_ep2.size());
    flowManager.egDomainUpdated(epg0->getURI());
    WAIT_FOR(exec.IsEmpty(), 500);

    /* delete all groups except epg0, then update tunnel port */
    exec.IgnoreFlowMods();
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

    exec.Clear();
    exec.Expect(FlowEdit::add, fe_fixed_tun_new);
    exec.Expect(FlowEdit::del, fe_fixed[6]);
    exec.Expect(FlowEdit::mod, fe_fallback_tun_new);
    exec.Expect(FlowEdit::add, fe_epg0_tun_new);
    exec.Expect(FlowEdit::del, fe_epg0[0]);
    exec.Expect(FlowEdit::mod, fe_ep2_tun_new);
    portmapper.ports[tunIf] = tun_port_new;
    flowManager.portStatusUpdate(tunIf, tun_port_new, false);
    WAIT_FOR(exec.IsEmpty(), 500);

    /* remove mapping for tunnel port */
    exec.Clear();
    exec.Expect(FlowEdit::del, fe_fixed_tun_new);
    exec.Expect(FlowEdit::del, fe_fallback_tun_new);
    exec.Expect(FlowEdit::del, fe_epg0_tun_new);
    exec.Expect(FlowEdit::del, fe_ep2_tun_new);
    portmapper.ports.erase(tunIf);
    flowManager.portStatusUpdate(tunIf, tun_port_new, false);
    WAIT_FOR(exec.IsEmpty(), 500);
}

BOOST_FIXTURE_TEST_CASE(portstatus_vxlan, VxlanFlowManagerFixture) {
    portStatusTest();
}

BOOST_FIXTURE_TEST_CASE(portstatus_vlan, VlanFlowManagerFixture) {
    portStatusTest();
}

void FlowManagerFixture::mcastTest() {
    exec.IgnoreFlowMods();
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

    Mutator mutator(framework, policyOwner);
    config->setMulticastGroupIP(mcast2);
    fd0ctx->setMulticastGroupIP(mcast4);
    mutator.commit();
    WAIT_FOR_DO(fd0ctx->getMulticastGroupIP("") == mcast4, 500,
        fd0ctx = policyMgr.getFloodContextForGroup(epg2->getURI()).get());

    jsonCmdExecutor.clear();
    jsonCmdExecutor.expect("dpif/tnl/igmp-leave" + prmBr + mcast1);
    jsonCmdExecutor.expect("dpif/tnl/igmp-join" + prmBr + mcast2);
    jsonCmdExecutor.expect("dpif/tnl/igmp-join" + prmBr + mcast4);
    flowManager.configUpdated(config->getURI());
    flowManager.egDomainUpdated(epg2->getURI());
    WAIT_FOR(jsonCmdExecutor.empty(), 500);

    fd0ctx->unsetMulticastGroupIP();
    mutator.commit();
    WAIT_FOR_DO(fd0ctx->isMulticastGroupIPSet() == false, 500,
        fd0ctx = policyMgr.getFloodContextForGroup(epg2->getURI()).get());
    jsonCmdExecutor.clear();
    jsonCmdExecutor.expect("dpif/tnl/igmp-leave" + prmBr + mcast4);
    flowManager.egDomainUpdated(epg2->getURI());
    WAIT_FOR(jsonCmdExecutor.empty(), 500);
}

BOOST_FIXTURE_TEST_CASE(mcast_vxlan, VxlanFlowManagerFixture) {
    mcastTest();
}

enum REG {
    SEPG, SEPG12, DEPG, BD, FD, RD, OUTPORT, TUNID, TUNDST, VLAN, 
    ETHSRC, ETHDST, ARPOP, ARPSHA, ARPTHA, ARPSPA, ARPTPA
};
string rstr[] = {
    "NXM_NX_REG0[]", "NXM_NX_REG0[0..11]", "NXM_NX_REG2[]", "NXM_NX_REG4[]",
    "NXM_NX_REG5[]", "NXM_NX_REG6[]", "NXM_NX_REG7[]", "NXM_NX_TUN_ID[0..31]",
    "NXM_NX_TUN_IPV4_DST[]", "OXM_OF_VLAN_VID[]", 
    "NXM_OF_ETH_SRC[]", "NXM_OF_ETH_DST[]", "NXM_OF_ARP_OP[]", 
    "NXM_NX_ARP_SHA[]", "NXM_NX_ARP_THA[]", "NXM_OF_ARP_SPA[]", "NXM_OF_ARP_TPA[]"
};
string rstr1[] = { "reg0", "reg0", "reg2", "reg4", "reg5", "reg6", "reg7", 
                   "", "", "", "", "", "", "" ,"", "", ""};

/**
 * Helper class to build string representation of a flow entry.
 */
class Bldr {
public:
    Bldr(const string& init="") : entry(init) {
        cntr = 0;
        if (entry.empty()) {
            entry = "cookie=0x0, duration=0s, table=0, n_packets=0, "
                    "n_bytes=0, idle_age=0";
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
    Bldr& actions() { rep(" actions="); cntr = 1; return *this; }
    Bldr& drop() { rep("drop"); return *this; }
    Bldr& load(REG r, uint32_t v) {
        rep("load:", str(v, true), "->" + rstr[r]); return *this;
    }
    Bldr& load(REG r, const string& v) {
        rep("load:", v, "->" + rstr[r]); return *this;
    }
    Bldr& move(REG s, REG d) {
        rep("move:"+rstr[s]+"->"+rstr[d]); return *this;
    }
    Bldr& ethSrc(string& s) { rep("set_field:", s, "->eth_src"); return *this; }
    Bldr& ethDst(string& s) { rep("set_field:", s, "->eth_dst"); return *this; }
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
    Bldr& popVlan() { rep("strip_vlan"); return *this; }
    Bldr& inport() { rep("IN_PORT"); return *this; }
    Bldr& controller(uint16_t len) { rep("CONTROLLER:", str(len)); return *this; }

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
            string tmp = entry.substr(0, pos1) + sep + pfx + val +
                    entry.substr(pos2);
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

/**
 * Create flow-entries corresponding to the state of objects and
 * modifications performed in the tests.
 */
void
FlowManagerFixture::createEntriesForObjects(FlowManager::EncapType encapType) {
    uint32_t tunPort = flowManager.GetTunnelPort();
    address tunDstAddr = flowManager.GetTunnelDst();
    uint32_t tunDst = tunDstAddr.to_v4().to_ulong();
    address mcastTunDstAddr = address::from_string("224.1.1.1");
    uint32_t mcastTunDst = mcastTunDstAddr.to_v4().to_ulong();

    uint8_t rmacArr[6];
    memcpy(rmacArr, flowManager.GetRouterMacAddr(), sizeof(rmacArr));
    string rmac = MAC(rmacArr).toString();
    string bmac("ff:ff:ff:ff:ff:ff");
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    fe_fixed.push_back(Bldr().table(0).priority(25).arp()
                       .actions().drop().done());
    fe_fixed.push_back(Bldr().table(0).priority(25).ip()
                       .actions().drop().done());
    fe_fixed.push_back(Bldr().table(0).priority(25).ipv6()
                       .actions().drop().done());
    fe_fixed.push_back(Bldr().table(0).priority(27).udp()
                       .isTpSrc(68).isTpDst(67).actions().go(1).done());
    fe_fixed.push_back(Bldr().table(0).priority(27).udp6()
                       .isTpSrc(546).isTpDst(547).actions().go(1).done());
    fe_fixed.push_back(Bldr().table(0).priority(27)
                       .icmp6().icmp_type(133).icmp_code(0)
                       .actions().go(1).done());
    fe_fixed.push_back(Bldr().table(0).priority(50)
                       .in(tunPort).actions().go(1).done());
    fe_fixed_tun_new.push_back(Bldr(fe_fixed[6]).in(tun_port_new).done());

    switch (encapType) {
    case FlowManager::ENCAP_VLAN:
        fe_fallback.push_back(Bldr().table(2).priority(1)
                              .actions().pushVlan().move(SEPG12, VLAN)
                              .outPort(tunPort).done());
        break;
    case FlowManager::ENCAP_VXLAN:
    case FlowManager::ENCAP_IVXLAN:
    default:
        fe_fallback.push_back(Bldr().table(2).priority(1)
                              .actions().move(SEPG, TUNID).load(TUNDST, tunDst)
                              .outPort(tunPort).done());
        break;
    }
    fe_fallback_tun_new.push_back(Bldr(fe_fallback[0])
                                  .outPort(tun_port_new).done());

    /* epg0 */
    uint32_t epg0_vnid = policyMgr.getVnidForGroup(epg0->getURI()).get();
    uint32_t epg1_vnid = policyMgr.getVnidForGroup(epg1->getURI()).get();
    switch (encapType) {
    case FlowManager::ENCAP_VLAN:
        fe_epg0.push_back(Bldr().table(1).priority(149)
                          .in(tunPort).isVlan(epg0_vnid)
                          .actions().popVlan()
                          .load(SEPG, epg0_vnid).load(BD, 1)
                          .load(FD, 0).load(RD, 1).go(2).done());
        break;
    case FlowManager::ENCAP_VXLAN:
    case FlowManager::ENCAP_IVXLAN:
    default:
        fe_epg0.push_back(Bldr().table(1).priority(149).tunId(epg0_vnid)
                          .in(tunPort).actions().load(SEPG, epg0_vnid).load(BD, 1)
                          .load(FD, 0).load(RD, 1).go(2).done());
        break;
    }
    fe_epg0.push_back(Bldr().table(4).priority(100).reg(SEPG, epg0_vnid)
            .reg(DEPG, epg0_vnid).actions().out(OUTPORT).done());
    fe_epg0_tun_new.push_back(Bldr(fe_epg0[0]).in(tun_port_new).done());

    /* epg0 connected to fd0 */
    fe_epg0_fd0.push_back(Bldr(fe_epg0[0]).load(FD, 1).done());

    /* Router entries when epg0 is connected to fd0. Since epg0 is connected to
     * bd0 in the beginning, get the router entries from an EPG already
     * connected to fd0, such as epg2
     */
    PolicyManager::subnet_vector_t epg0_fd0_subnets;
    policyMgr.getSubnetsForGroup(epg2->getURI(), epg0_fd0_subnets);
    BOOST_FOREACH (shared_ptr<Subnet>& sn, epg0_fd0_subnets) {
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
            fe_epg0_routers.push_back(Bldr()
                                      .cookie(htonll(FlowManager::GetNDCookie()))
                                      .table(2).priority(20).icmp6().reg(RD, 1)
                                      .isEthDst(mmac).icmp_type(135).icmp_code(0)
                                      .isNdTarget(rip.to_string())
                                      .actions().controller(65535)
                                      .done());
        } else {
            fe_epg0_routers.push_back(Bldr().table(2).priority(20).arp().reg(RD, 1)
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

    /* rd0 router solicitation */
    fe_rd0.push_back(Bldr().cookie(htonll(FlowManager::GetNDCookie()))
                     .table(2).priority(20).icmp6().reg(RD, 1)
                     .isEthDst(mmac).icmp_type(133).icmp_code(0)
                     .actions().controller(65535)
                     .done());

    /* local EP ep0 */
    string ep0_mac = ep0->getMAC().get().toString();
    uint32_t ep0_port = portmapper.FindPort(ep0->getInterfaceName().get());
    vector<string> ep0_arpopt;
    const unordered_set<string>& ep0ips = 
        agent.getEndpointManager().getEndpoint(ep0->getUUID())->getIPs();

    fe_ep0.push_back(Bldr().table(0).priority(20).in(ep0_port)
            .isEthSrc(ep0_mac).actions().go(1).done());
    BOOST_FOREACH(const string& ip, ep0ips) {
        address ipa = address::from_string(ip);
        if (ipa.is_v4()) {
            fe_ep0.push_back(Bldr().table(0).priority(30).ip().in(ep0_port)
                             .isEthSrc(ep0_mac).isIpSrc(ip).actions().go(1).done());
            fe_ep0.push_back(Bldr().table(0).priority(40).arp().in(ep0_port)
                             .isEthSrc(ep0_mac).isSpa(ip).actions().go(1).done());
        } else {
            fe_ep0.push_back(Bldr().table(0).priority(30).ipv6().in(ep0_port)
                             .isEthSrc(ep0_mac).isIpv6Src(ip).actions().go(1).done());
            fe_ep0.push_back(Bldr().table(0).priority(40).icmp6().in(ep0_port)
                             .isEthSrc(ep0_mac).isIpv6Src(ip)
                             .icmp_type(136).icmp_code(0).actions().go(1).done());
        }
    }

    fe_ep0.push_back(Bldr().table(1).priority(140).in(ep0_port)
            .isEthSrc(ep0_mac).actions().load(SEPG, epg0_vnid).load(BD, 1)
            .load(FD, 0).load(RD, 1).go(2).done());
    fe_ep0.push_back(Bldr().table(2).priority(10).reg(BD, 1)
            .isEthDst(ep0_mac).actions().load(DEPG, epg0_vnid)
            .load(OUTPORT, ep0_port).go(4).done());

    BOOST_FOREACH(const string& ip, ep0ips) {
        address ipa = address::from_string(ip);
        if (ipa.is_v4()) {
            fe_ep0.push_back(Bldr().table(2).priority(15).ip()
                             .reg(BD, 1).reg(RD, 1)
                             .isEthDst(rmac).isIpDst(ip)
                             .actions().load(DEPG, epg0_vnid)
                             .load(OUTPORT, ep0_port).ethSrc(rmac)
                             .ethDst(ep0_mac).decTtl().go(4).done());
            ep0_arpopt.push_back(Bldr().table(2).priority(20).arp().reg(RD, 1)
                                 .isEthDst(bmac).isTpa(ip).isArpOp(1)
                                 .actions().load(DEPG, epg0_vnid)
                                 .load(OUTPORT, ep0_port)
                                 .ethDst(ep0_mac).go(4).done());
            fe_ep0_routeoff.push_back(*fe_ep0.rbegin());
            fe_ep0.push_back(*ep0_arpopt.rbegin());
        } else {
            fe_ep0.push_back(Bldr().table(2).priority(15).ipv6()
                             .reg(BD, 1).reg(RD, 1)
                             .isEthDst(rmac).isIpv6Dst(ip)
                             .actions().load(DEPG, epg0_vnid)
                             .load(OUTPORT, ep0_port).ethSrc(rmac)
                             .ethDst(ep0_mac).decTtl().go(4).done());
            ep0_arpopt.push_back(Bldr().table(2).priority(20).icmp6().reg(RD, 1)
                                 .isEthDst(mmac).icmp_type(135).icmp_code(0)
                                 .isNdTarget(ip).actions().load(DEPG, epg0_vnid)
                                 .load(OUTPORT, ep0_port)
                                 .ethDst(ep0_mac).go(4).done());
            fe_ep0_routeoff.push_back(*fe_ep0.rbegin());
            fe_ep0.push_back(*ep0_arpopt.rbegin());
        }
    }

    /* ep0 when eg0 connected to fd0 */
    fe_ep0_fd0_1.push_back(Bldr(fe_ep0[9]).load(FD, 1).done());
    fe_ep0_fd0_2.push_back(Bldr().table(2).priority(10).reg(FD, 1)
            .isEthDst(mmac).actions().group(1).done());

    /* ep0 when moved to new group epg1 (fe_ep0_eg1) or moved back to
       old group (fe_ep0_eg0) */
    fe_ep0_eg1.push_back(list_of(Bldr(fe_ep0[9])
                                 .load(BD, 2).load(SEPG, epg1_vnid)
                                 .done()));
    fe_ep0_eg0.push_back(list_of(fe_ep0[9]));
    fe_ep0_eg1.push_back(list_of
                         (Bldr(fe_ep0[10]).reg(BD, 2)
                          .load(DEPG, epg1_vnid).done())
                         (Bldr(fe_ep0[11]).reg(BD, 2)
                          .load(DEPG, epg1_vnid).done()));
    fe_ep0_eg0.push_back(list_of(fe_ep0[10])(fe_ep0[11]));

    for (size_t i = 12; i <= 18; i++) {
        if (i % 2 == 0) {
            fe_ep0_eg1.push_back(list_of(Bldr(fe_ep0[i])
                                         .load(DEPG, epg1_vnid).done()));
        } else {
            fe_ep0_eg1.push_back(list_of(Bldr(fe_ep0[i]).reg(BD, 2)
                                         .load(DEPG, epg1_vnid).done()));
        }
        fe_ep0_eg0.push_back(list_of(fe_ep0[i]));
    }

    fe_ep0_eg1.push_back(vector<string>());
    fe_ep0_eg0.push_back(vector<string>());
    for (size_t i = 10; i < fe_ep0.size(); i++) {
        if (i == 12) continue;
        if (i == 14) continue;
        if (i == 16) continue;
        if (i == 18) continue;
        (*fe_ep0_eg1.rbegin()).push_back(fe_ep0[i]);
        (*fe_ep0_eg0.rbegin()).push_back(Bldr(fe_ep0[i]).reg(BD, 2)
                                         .load(DEPG, epg1_vnid).done());
    }

    /* ep0 on new port */
    uint32_t ep0_port_new = 180;
    size_t idx = 0;
    for (; idx < 9; ++idx) {
        fe_ep0_port_1.push_back(Bldr(fe_ep0[idx]).in(ep0_port_new).done());
    }
    fe_ep0_port_2.push_back(Bldr(fe_ep0[idx++]).in(ep0_port_new).done());
    for (; idx < fe_ep0.size(); ++idx) {
        fe_ep0_port_3.push_back(Bldr(fe_ep0[idx]).load(OUTPORT, ep0_port_new)
                .done());
    }

    /* Remote EP ep2 */
    string ep2_mac = ep2->getMAC().get().toString();
    string ep2_ip0 = *(ep2->getIPs().begin());
    string ep2_arpopt;

    switch (encapType) {
    case FlowManager::ENCAP_VLAN:
        ep2_arpopt = Bldr().table(2).priority(20).arp().reg(RD, 1)
            .isEthDst(bmac).isTpa(ep2_ip0).isArpOp(1)
            .actions().load(DEPG, epg0_vnid)
            .load(OUTPORT, tunPort).pushVlan().move(SEPG12, VLAN)
            .ethDst(ep2_mac).go(4).done();
        fe_ep2.push_back(Bldr().table(2).priority(10).reg(BD, 1).isEthDst(ep2_mac)
                         .actions().load(DEPG, epg0_vnid).load(OUTPORT, tunPort)
                         .pushVlan().move(SEPG12, VLAN).go(4)
                         .done());
        fe_ep2.push_back(Bldr().table(2).priority(15).ip().reg(BD, 1).reg(RD, 1)
                         .isEthDst(rmac).isIpDst(ep2_ip0).actions()
                         .load(DEPG, epg0_vnid).load(OUTPORT, tunPort)
                         .pushVlan().move(SEPG12, VLAN)
                         .ethSrc(rmac).decTtl().go(4).done());
        fe_ep2_routeoff.push_back(*fe_ep2.rbegin());
        break;
    case FlowManager::ENCAP_VXLAN:
    case FlowManager::ENCAP_IVXLAN:
    default:
        ep2_arpopt = Bldr().table(2).priority(20).arp().reg(RD, 1)
            .isEthDst(bmac).isTpa(ep2_ip0).isArpOp(1)
            .actions().load(DEPG, epg0_vnid)
            .load(OUTPORT, tunPort).move(SEPG, TUNID).load(TUNDST, tunDst)
            .ethDst(ep2_mac).go(4).done();
        fe_ep2.push_back(Bldr().table(2).priority(10).reg(BD, 1).isEthDst(ep2_mac)
                         .actions().load(DEPG, epg0_vnid).load(OUTPORT, tunPort)
                         .move(SEPG, TUNID).load(TUNDST, tunDst).go(4)
                         .done());
        fe_ep2.push_back(Bldr().table(2).priority(15).ip().reg(BD, 1).reg(RD, 1)
                         .isEthDst(rmac).isIpDst(ep2_ip0).actions().load(DEPG, epg0_vnid)
                         .load(OUTPORT, tunPort).move(SEPG, TUNID).load(TUNDST, tunDst)
                         .ethSrc(rmac).decTtl().go(4).done());
        fe_ep2_routeoff.push_back(*fe_ep2.rbegin());
        break;
    }
    fe_ep2.push_back(ep2_arpopt);
    BOOST_FOREACH (const string& se, fe_ep2) {
        fe_ep2_tun_new.push_back(Bldr(se).load(OUTPORT, tun_port_new).done());
    }

    /**
     * ARP flows
     */
    fe_arpopt.insert(fe_arpopt.end(), ep0_arpopt.begin(), ep0_arpopt.end());
    fe_arpopt.push_back(ep2_arpopt);

    /* ep2 connected to new group epg1 */
    fe_ep2_eg1.push_back(Bldr(fe_ep2[0]).reg(BD, 2).load(DEPG, epg1_vnid).done());
    fe_ep2_eg1.push_back(Bldr(fe_ep2[1]).reg(BD, 2).load(DEPG, epg1_vnid).done());
    fe_ep2_eg1_arp.push_back(Bldr(ep2_arpopt).load(DEPG, epg1_vnid).done());

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

    /* Contracts */
    uint16_t prio = FlowManager::MAX_POLICY_RULE_PRIORITY;
    PolicyManager::uri_set_t ps, cs;
    unordered_map<uint32_t, uint32_t> pvnids, cvnids;
    typedef unordered_map<uint32_t, uint32_t>::value_type IdKeyValue;

    /* con2 */
    uint32_t con2_cookie = flowManager.GetId(con2->getClassId(),
        con2->getURI());
    policyMgr.getContractProviders(con2->getURI(), ps);
    flowManager.getEpgVnidAndRdId(ps, pvnids);
    BOOST_FOREACH (const IdKeyValue& pvnid, pvnids) {
        BOOST_FOREACH (const IdKeyValue& cvnid, pvnids) {
            fe_con2.push_back(Bldr().table(4).priority(prio).cookie(con2_cookie)
                .reg(SEPG, cvnid.first).reg(DEPG, pvnid.first).isEth(0x8906)
                .actions().out(OUTPORT).done());
        }
    }
    ps.clear();
    pvnids.clear();

    /* con1 */
    policyMgr.getContractProviders(con1->getURI(), ps);
    policyMgr.getContractConsumers(con1->getURI(), cs);
    flowManager.getEpgVnidAndRdId(ps, pvnids);
    flowManager.getEpgVnidAndRdId(cs, cvnids);
    uint32_t con1_cookie = flowManager.GetId(con1->getClassId(),
            con1->getURI());
    BOOST_FOREACH(const IdKeyValue& pid, pvnids) {
        uint32_t pvnid = pid.first;
        BOOST_FOREACH(const IdKeyValue& cid, cvnids) {
            uint32_t cvnid = cid.first;
            uint16_t ctzone = 1;
            /* classifer 1  */
            fe_con1.push_back(Bldr().table(4).priority(prio)
                    .cookie(con1_cookie).tcp()
                    .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpDst(80)
                    .actions().conntrack("commit", ctzone).out(OUTPORT)
                    .done());
            fe_con1.push_back(Bldr().table(4).priority(prio)
                    .cookie(con1_cookie).connState("-trk").tcp()
                    .reg(SEPG, pvnid).reg(DEPG, cvnid)
                    .actions().conntrack("recirc", ctzone).done());
            fe_con1.push_back(Bldr().table(4).priority(prio)
                    .cookie(con1_cookie).connState("-new+est+trk").tcp()
                    .reg(SEPG, pvnid).reg(DEPG, cvnid)
                    .actions().out(OUTPORT).done());
            /* classifer 2  */
            fe_con1.push_back(Bldr().table(4).priority(prio-1)
                    .cookie(con1_cookie).arp()
                    .reg(SEPG, pvnid).reg(DEPG, cvnid)
                    .actions().out(OUTPORT).done());
            /* classifier 6 */
            fe_con1.push_back(Bldr().table(4).priority(prio-2)
                    .cookie(con1_cookie).tcp()
                    .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpSrc(22)
                    .isTcpFlags("+syn+ack").actions().out(OUTPORT).done());
            /* classifier 7 */
            fe_con1.push_back(Bldr().table(4).priority(prio-3)
                    .cookie(con1_cookie).tcp()
                    .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpSrc(21)
                    .isTcpFlags("+ack").actions().out(OUTPORT).done());
            fe_con1.push_back(Bldr().table(4).priority(prio-3)
                    .cookie(con1_cookie).tcp()
                    .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpSrc(21)
                    .isTcpFlags("+rst").actions().out(OUTPORT).done());
        }
    }

    /* con3 */
    uint32_t con3_cookie = flowManager.GetId(con3->getClassId(),
        con3->getURI());
    MaskList ml_80_85 = list_of<Mask>(0x0050, 0xfffc)(0x0054, 0xfffe);
    MaskList ml_66_69 = list_of<Mask>(0x0042, 0xfffe)(0x0044, 0xfffe);
    MaskList ml_94_95 = list_of<Mask>(0x005e, 0xfffe);
    BOOST_FOREACH (const Mask& mk, ml_80_85) {
        fe_con3.push_back(Bldr().table(4).priority(prio)
            .cookie(con3_cookie).tcp()
            .reg(SEPG, epg1_vnid).reg(DEPG, epg0_vnid)
            .isTpDst(mk.first, mk.second).actions().drop().done());
    }
    BOOST_FOREACH (const Mask& mks, ml_66_69) {
        BOOST_FOREACH (const Mask& mkd, ml_94_95) {
            fe_con3.push_back(Bldr().table(4).priority(prio-1)
                .cookie(con3_cookie).tcp()
                .reg(SEPG, epg1_vnid).reg(DEPG, epg0_vnid)
                .isTpSrc(mks.first, mks.second).isTpDst(mkd.first, mkd.second)
                .actions().out(OUTPORT).done());
        }
    }

    /* connect */
    fe_connect_1 = Bldr().table(0).priority(0).cookie(0xabcd)
            .actions().drop().done();
    uint32_t epg4_vnid = policyMgr.getVnidForGroup(epg4->getURI()).get();
    fe_connect_2 = Bldr().table(4).priority(100).reg(SEPG, epg4_vnid)
            .reg(DEPG, epg4_vnid).actions().out(OUTPORT).done();

    fe_connect_learn
        .push_back(Bldr().table(3).priority(150)
                   .cookie(ntohll(FlowManager::GetLearnEntryCookie()))
                   .isEthDst("de:ad:be:ef:01:02")
                   .actions().load(OUTPORT, 999).controller(0xffff)
                   .done());
    fe_connect_learn
        .push_back(Bldr().table(3).priority(150)
                   .cookie(ntohll(FlowManager::GetLearnEntryCookie()))
                   .isEthDst("de:ad:be:ef:01:02")
                   .actions().load(OUTPORT, 80).controller(0xffff)
                   .done());
    fe_connect_learn
        .push_back(Bldr().table(3).priority(150)
                   .cookie(ntohll(FlowManager::GetLearnEntryCookie()))
                   .isEthDst(ep0->getMAC().get().toString())
                   .actions().load(OUTPORT, 999).controller(0xffff)
                   .done());
                                       
}

void
FlowManagerFixture::createOnConnectEntries(FlowManager::EncapType encapType,
                                           FlowEntryList& flows,
                                           GroupEdit::EntryList& groups) {
    uint8_t mac[6];

    flows.clear();
    groups.clear();

    uint32_t epg4_vnid = policyMgr.getVnidForGroup(epg4->getURI()).get();

    FlowEntryPtr e0(new FlowEntry());
    flows.push_back(e0);
    e0->entry->table_id = 0;
    e0->entry->cookie = htonll(0xabcd);

    FlowEntryPtr e1(new FlowEntry());
    flows.push_back(e1);
    e1->entry->table_id = 1;
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
    ab.SetGotoTable(2);
    ab.Build(e1->entry);

    FlowEntryPtr e2(new FlowEntry());
    flows.push_back(e2);
    e2->entry->table_id = 4;
    e2->entry->priority = 100;
    match_set_reg(&e2->entry->match, 0, epg4_vnid);
    match_set_reg(&e2->entry->match, 2, epg4_vnid);

    // invalid learn entry with invalid mac and port
    FlowEntryPtr e3(new FlowEntry());
    flows.push_back(e3);
    e3->entry->table_id = 3;
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
    e4->entry->table_id = 3;
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
    e5->entry->table_id = 3;
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
    e6->entry->table_id = 3;
    e6->entry->priority = 150;
    e6->entry->cookie = FlowManager::GetLearnEntryCookie();
    ep0->getMAC().get().toUIntArray(mac);
    match_set_dl_dst(&e6->entry->match, mac);
    ActionBuilder ab6;
    ab6.SetRegLoad(MFF_REG7, 999);
    ab6.SetController();
    ab6.Build(e6->entry);

    GroupEdit::Entry entryIn(new GroupEdit::GroupMod());
    entryIn->mod->command = OFPGC11_ADD;
    entryIn->mod->group_id = 10;
    groups.push_back(entryIn);
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
