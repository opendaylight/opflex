/*
 * Test suite for class VppManager
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <memory>

#include <boost/optional.hpp>
#include <boost/test/unit_test.hpp>

#include <modelgbp/gbp/SecGroup.hpp>

#include <vom/bridge_domain.hpp>
#include <vom/bridge_domain_arp_entry.hpp>
#include <vom/bridge_domain_entry.hpp>
#include <vom/dhcp_config.hpp>
#include <vom/hw.hpp>
#include <vom/inspect.hpp>
#include <vom/interface.hpp>
#include <vom/l2_binding.hpp>
#include <vom/l3_binding.hpp>
#include <vom/lldp_binding.hpp>
#include <vom/lldp_global.hpp>
#include <vom/neighbour.hpp>
#include <vom/route.hpp>
#include <vom/route.hpp>
#include <vom/route_domain.hpp>
#include <vom/sub_interface.hpp>

#include "ModbFixture.h"
#include "VppManager.h"

using namespace VOM;
using namespace ovsagent;

BOOST_AUTO_TEST_SUITE(VppManager_test)

class MockCmdQ : public HW::cmd_q {
public:
    MockCmdQ() : handle(0) {}
    ~MockCmdQ() {}

    void enqueue(cmd* c) {
        std::shared_ptr<cmd> sp(c);
        m_cmds.push(sp);
    }
    void enqueue(std::queue<cmd*>& cmds) {
        cmd* c;

        while (!cmds.empty()) {
            c = cmds.front();
            cmds.pop();

            std::shared_ptr<cmd> sp(c);
            m_cmds.push(sp);
        }
    }
    void enqueue(std::shared_ptr<cmd> c) { m_cmds.push(c); }

    void dequeue(cmd* f) {}

    void dequeue(std::shared_ptr<cmd> cmd) {}

    rc_t write() {
        std::shared_ptr<cmd> c;

        while (!m_cmds.empty()) {
            c = m_cmds.front();
            m_cmds.pop();
            handle_cmd(c.get());
        }

        return (rc_t::OK);
    }

    /**
     * Blocking Connect to VPP - call once at bootup
     */
    bool connect() {}

private:
    void handle_cmd(cmd* c) {
        if (typeid(*c) == typeid(interface_cmds::af_packet_create_cmd)) {
            interface_cmds::af_packet_create_cmd* ac =
                dynamic_cast<interface_cmds::af_packet_create_cmd*>(c);
            HW::item<handle_t> res(++handle, rc_t::OK);
            ac->item() = res;
        }

        c->succeeded();
    }
    uint32_t handle;

    std::queue<std::shared_ptr<cmd>> m_cmds;
};

template <typename T> bool is_match(const T& expected) {
    std::shared_ptr<T> actual = T::find(expected.key());

    //  std::cout << "exp: " << expected.to_string() << std::endl;
    if (!actual)
        return false;

    //  std::cout << "act: " << actual->to_string() << std::endl;

    return (expected == *actual);
}

template <typename T> bool is_present(const T& search) {
    std::shared_ptr<T> actual = T::find(search.key());

    //  std::cout << "serach:  " << search.to_string() << std::endl;
    if (!actual)
        return false;

    //  std::cout << "present: " << actual->to_string() << std::endl;
    return (true);
}

#define IS_MATCH(obj) BOOST_TEST_CHECK(is_match(obj))

#define WAIT_FOR1(stmt) WAIT_FOR((stmt), 500)

class VppManagerFixture : public ModbFixture {
public:
    VppManagerFixture()
        : vMac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55},
          policyMgr(agent.getPolicyManager()), vppQ(),
          vppManager(agent, idGen, &vppQ), inspector() {
        createObjects();
        WAIT_FOR(policyMgr.groupExists(epg0->getURI()), 500);
        WAIT_FOR(policyMgr.getBDForGroup(epg0->getURI()) != boost::none, 500);

        WAIT_FOR(policyMgr.groupExists(epg1->getURI()), 500);
        WAIT_FOR(policyMgr.getRDForGroup(epg1->getURI()) != boost::none, 500);

        WAIT_FOR(policyMgr.groupExists(epg3->getURI()), 500);
        WAIT_FOR(policyMgr.getRDForGroup(epg3->getURI()) != boost::none, 500);

        WAIT_FOR(policyMgr.groupExists(epg4->getURI()), 500);
        WAIT_FOR(policyMgr.getRDForGroup(epg4->getURI()) != boost::none, 500);

        vppManager.start();
        vppManager.uplink().set("opflex-itf", 4093, "opflex-host");
        vppManager.setVirtualRouter(true, true, vMac.to_string());
    }

    virtual ~VppManagerFixture() {
        vppManager.stop();
        agent.stop();
    }

    void assignEpg0ToFd0(PolicyManager::subnet_vector_t& sns) {
        opflex::modb::Mutator mutator(framework, policyOwner);
        epg0->addGbpEpGroupToNetworkRSrc()->setTargetFloodDomain(fd0->getURI());
        mutator.commit();

        WAIT_FOR1(policyMgr.getFDForGroup(epg0->getURI()) != boost::none);
        WAIT_FOR_DO(sns.size() == 4, 500, sns.clear();
                    policyMgr.getSubnetsForGroup(epg0->getURI(), sns));
        WAIT_FOR1(
            (PolicyManager::getRouterIpForSubnet(*sns[1]) != boost::none));
    }

    void removeEpg(std::shared_ptr<modelgbp::gbp::EpGroup> epg) {
        opflex::modb::Mutator m2(framework, policyOwner);
        epg->remove();
        m2.commit();
        WAIT_FOR1(!policyMgr.groupExists(epg->getURI()));
    }

    mac_address_t vMac;
    PolicyManager& policyMgr;
    IdGenerator idGen;
    MockCmdQ vppQ;

    VppManager vppManager;

    /**
     * To assist in checking the state that is present manually do
     *
     * inspector.handle_input("all", std::cout);
     *
     * in any of the test-cases
     */
    inspect inspector;
};

BOOST_FIXTURE_TEST_CASE(start, VppManagerFixture) {
    /*
     * Validate the presence of the uplink state built at startup/boot
     *  - the physical unplink interface
     *  - the control VLAN sub-interface
     *  - DHCP configuration on the sub-interface
     *  - LLDP config on the physical interface
     */
    interface v_phy("opflex-itf", interface::type_t::ETHERNET,
                    interface::admin_state_t::UP);
    sub_interface v_sub(v_phy, interface::admin_state_t::UP, 4093);

    WAIT_FOR1(is_match(v_phy));
    WAIT_FOR(is_match(v_sub), 500);

    WAIT_FOR(is_match(dhcp_config(v_sub, "localhost")), 500);
    WAIT_FOR(is_match(lldp_global("localhost", 5, 2)), 500);
    WAIT_FOR(is_match(lldp_binding(v_phy, "uplink-interface")), 500);
}

BOOST_FIXTURE_TEST_CASE(endpoint_group_add_del, VppManagerFixture) {
    vppManager.egDomainUpdated(epg0->getURI());

    /*
     * check for the presence of a VOM route-domain matching the EPG's = 1
     * ID's are offset by 100.
     */
    route_domain v_rd(100);
    WAIT_FOR1(is_match(v_rd));

    /*
     * After waiting for the route-domain to be created
     * all other state should now be present
     */

    /*
     * Check for a bridge domain 100
     */
    bridge_domain v_bd(100);
    WAIT_FOR1(is_match(v_bd));

    /*
     * Find the BVI interface. the BVI's name includes the bridge-domain ID
     * the interface has a dependency on the route domain, so we 'new' the
     * interface so we can control its lifetime.
     */
    interface* v_bvi = new interface("bvi-100", interface::type_t::BVI,
                                     interface::admin_state_t::UP, v_rd);
    v_bvi->set(vMac);
    WAIT_FOR1(is_match(*v_bvi));

    /*
     * the BVI is put in the bridge-domain
     */
    l2_binding* v_l2b = new l2_binding(*v_bvi, v_bd);
    WAIT_FOR1(is_match(*v_l2b));

    /*
     * The EPG uplink interface, also bound to BD=1
     */
    interface v_phy("opflex-itf", interface::type_t::ETHERNET,
                    interface::admin_state_t::UP);
    sub_interface v_epg(v_phy, interface::admin_state_t::UP, 0xA0A);
    l2_binding* v_l2b_epg = new l2_binding(v_epg, v_bd);
    WAIT_FOR1(is_match(v_epg));
    WAIT_FOR1(is_match(*v_l2b_epg));

    /*
     * Add EPG0 into FD0 to assign it subnets
     */
    PolicyManager::subnet_vector_t sns;
    assignEpg0ToFd0(sns);
    vppManager.egDomainUpdated(epg0->getURI());

    /*
     * there should be a route for each of those sub-nets via the epg-uplink
     */
    for (auto subnet : sns) {
        boost::optional<boost::asio::ip::address> routerIp =
            PolicyManager::getRouterIpForSubnet(*subnet);

        if (routerIp) {
            boost::asio::ip::address raddr = routerIp.get();

            /*
             * - apply the host prefix on the BVI
             * - add an entry into the L2 FIB for it.
             * - add an entry into the ARP Table for it.
             */
            WAIT_FOR1(is_match(l3_binding(*v_bvi, {raddr})));
            WAIT_FOR1(is_match(bridge_domain_entry(v_bd, vMac, *v_bvi)));
            WAIT_FOR1(is_match(bridge_domain_arp_entry(v_bd, raddr, vMac)));

            /*
             * add the subnet as a DVR route, so all other EPs will be
             * L3-bridged to the uplink
             */
            route::prefix_t subnet_pfx(raddr, subnet->getPrefixLen().get());
            route::path dvr_path(v_epg, nh_proto_t::ETHERNET);
            route::ip_route subnet_route(v_rd, subnet_pfx);
            subnet_route.add(dvr_path);

            WAIT_FOR1(is_match(subnet_route));
        }
    }

    /*
     * Withdraw the EPG, all the state above should be gone
     */
    removeEpg(epg0);
    vppManager.egDomainUpdated(epg0->getURI());

    WAIT_FOR1(!is_present(*v_l2b));
    delete v_l2b;
    WAIT_FOR1(!is_present(*v_bvi));
    delete v_bvi;
    WAIT_FOR1(!is_present(*v_l2b_epg));
    delete v_l2b_epg;

    WAIT_FOR1(!is_present(v_epg));
    WAIT_FOR1(!is_present(v_bd));
    WAIT_FOR1(!is_present(v_rd));
}

BOOST_FIXTURE_TEST_CASE(endpoint_add_del, VppManagerFixture) {
    PolicyManager::subnet_vector_t sns;
    assignEpg0ToFd0(sns);
    vppManager.egDomainUpdated(epg0->getURI());
    vppManager.endpointUpdated(ep0->getUUID());

    std::vector<boost::asio::ip::address> ipAddresses;
    boost::system::error_code ec;

    for (const std::string& ipStr : ep0->getIPs()) {
        boost::asio::ip::address addr =
            boost::asio::ip::address::from_string(ipStr, ec);
        if (!ec) {
            ipAddresses.push_back(addr);
        }
    }
    mac_address_t v_mac("00:00:00:00:80:00");

    /*
     * Check for a bridge domain and route domain 100
     */
    bridge_domain v_bd(100);
    WAIT_FOR1(is_match(v_bd));
    route_domain v_rd(100);
    WAIT_FOR1(is_match(v_rd));

    /*
     * Find the EP's interface
     */
    interface* v_itf = new interface("port80", interface::type_t::AFPACKET,
                                     interface::admin_state_t::UP, v_rd);
    WAIT_FOR1(is_match(*v_itf));

    /*
     * the host interface is put in the bridge-domain
     */
    WAIT_FOR1(is_match(l2_binding(*v_itf, v_bd)));

    /*
     * A bridge-domain entry for the VM's MAC
     */
    WAIT_FOR1(is_match(bridge_domain_entry(v_bd, v_mac, *v_itf)));

    /*
     * routes, neighbours and BD entries for each of the EP's IP addresses
     */
    for (auto& ipAddr : ipAddresses) {
        WAIT_FOR1(is_match(bridge_domain_arp_entry(v_bd, ipAddr, v_mac)));
        WAIT_FOR1(is_match(neighbour(*v_itf, ipAddr, v_mac)));
        WAIT_FOR1(is_match(route::ip_route(v_rd, ipAddr, {ipAddr, *v_itf})));
    }

    /*
     * remove the EP
     */
    epSrc.removeEndpoint(ep0->getUUID());
    vppManager.endpointUpdated(ep0->getUUID());

    for (auto& ipAddr : ipAddresses) {
        WAIT_FOR1(!is_present(bridge_domain_arp_entry(v_bd, ipAddr, v_mac)));
        WAIT_FOR1(!is_present(neighbour(*v_itf, ipAddr, v_mac)));
        WAIT_FOR1(!is_present(route::ip_route(v_rd, ipAddr, {ipAddr, *v_itf})));
    }
    WAIT_FOR1(!is_present(bridge_domain_entry(v_bd, v_mac, *v_itf)));
    WAIT_FOR1(!is_present(l2_binding(*v_itf, v_bd)));
    WAIT_FOR1(!is_present(*v_itf));
    delete v_itf;

    /*
     * remove the EPG
     */
    removeEpg(epg0);
    vppManager.egDomainUpdated(epg0->getURI());

    WAIT_FOR1(!is_present(v_bd));
    WAIT_FOR1(!is_present(v_rd));
}

BOOST_FIXTURE_TEST_CASE(secGroup, VppManagerFixture) {
    using modelgbp::gbpe::L24Classifier;
    using namespace modelgbp::gbp;
    createPolicyObjects();

    PolicyManager::subnet_vector_t sns;
    PolicyManager::rule_list_t lrules;
    assignEpg0ToFd0(sns);
    vppManager.egDomainUpdated(epg0->getURI());
    vppManager.endpointUpdated(ep0->getUUID());

    std::shared_ptr<SecGroup> secGrp1, secGrp2;
    {
        opflex::modb::Mutator mutator(framework, policyOwner);
        secGrp1 = space->addGbpSecGroup("secgrp1");
        secGrp1->addGbpSecGroupSubject("1_subject1")
            ->addGbpSecGroupRule("1_1_rule1")
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(100)
            .addGbpRuleToClassifierRSrc(classifier1->getURI().toString());
        secGrp1->addGbpSecGroupSubject("1_subject1")
            ->addGbpSecGroupRule("1_1_rule2")
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(150)
            .addGbpRuleToClassifierRSrc(classifier8->getURI().toString());
        secGrp1->addGbpSecGroupSubject("1_subject1")
            ->addGbpSecGroupRule("1_1_rule3")
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(200)
            .addGbpRuleToClassifierRSrc(classifier6->getURI().toString());
        secGrp1->addGbpSecGroupSubject("1_subject1")
            ->addGbpSecGroupRule("1_1_rule4")
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(300)
            .addGbpRuleToClassifierRSrc(classifier7->getURI().toString());
        mutator.commit();
    }

    ep0->addSecurityGroup(secGrp1->getURI());
    epSrc.updateEndpoint(*ep0);

    WAIT_FOR_DO(lrules.size() == 4, 500, lrules.clear();
                policyMgr.getSecGroupRules(secGrp1->getURI(), lrules));

    vppManager.secGroupUpdated(secGrp1->getURI());

    route_domain v_rd(100);
    WAIT_FOR1(is_match(v_rd));

    /*
     * Find the EP's interface
     */
    interface* v_itf = new interface("port80", interface::type_t::AFPACKET,
                                     interface::admin_state_t::UP, v_rd);
    WAIT_FOR1(is_match(*v_itf));

    ACL::action_t act = ACL::action_t::PERMIT;
    ACL::l3_rule rule1(8192, act, route::prefix_t::ZERO, route::prefix_t::ZERO,
                       6, 0, 65535, 80, 65535, 0, 0);
    ACL::l3_rule rule2(8064, act, route::prefix_t::ZEROv6,
                       route::prefix_t::ZEROv6, 6, 0, 65535, 80, 65535, 0, 0);
    ACL::l3_rule rule3(7808, act, route::prefix_t::ZERO, route::prefix_t::ZERO,
                       6, 22, 65535, 0, 65535, 3, 3);
    ACL::l3_rule rule4(7680, act, route::prefix_t::ZERO, route::prefix_t::ZERO,
                       6, 21, 65535, 0, 65535, 16, 16);
    ACL::l3_list::rules_t rules({rule1, rule2, rule3, rule4});

    WAIT_FOR1(is_match(ACL::l3_list("/PolicyUniverse/PolicySpace/"
                             "tenant0/GbpSecGroup/secgrp1/out", rules)));

    {
        opflex::modb::Mutator mutator(framework, policyOwner);
        secGrp2 = space->addGbpSecGroup("secgrp2");
        secGrp2->addGbpSecGroupSubject("2_subject1")
            ->addGbpSecGroupRule("2_1_rule1")
            ->addGbpRuleToClassifierRSrc(classifier0->getURI().toString());
        secGrp2->addGbpSecGroupSubject("2_subject1")
            ->addGbpSecGroupRule("2_1_rule2")
            ->setDirection(DirectionEnumT::CONST_BIDIRECTIONAL).setOrder(20)
            .addGbpRuleToClassifierRSrc(classifier5->getURI().toString());
        secGrp2->addGbpSecGroupSubject("2_subject1")
            ->addGbpSecGroupRule("2_1_rule3")
            ->setDirection(DirectionEnumT::CONST_OUT).setOrder(30)
            .addGbpRuleToClassifierRSrc(classifier9->getURI().toString());
        mutator.commit();
    }

    ep0->addSecurityGroup(secGrp2->getURI());
    epSrc.updateEndpoint(*ep0);

    lrules.clear();
    WAIT_FOR_DO(lrules.size() == 2, 500, lrules.clear();
                policyMgr.getSecGroupRules(secGrp2->getURI(), lrules));

    vppManager.secGroupUpdated(secGrp2->getURI());

    act = ACL::action_t::PERMITANDREFLEX;
    ACL::l3_rule rule5(8064, act, route::prefix_t::ZERO, route::prefix_t::ZERO,
                       6, 0, 65535, 22, 65535, 0, 0);
    ACL::l3_list::rules_t rules2({rule5});
    WAIT_FOR1(is_match(ACL::l3_list("/PolicyUniverse/PolicySpace/"
                             "tenant0/GbpSecGroup/secgrp1/,/PolicyUniverse/"
                             "PolicySpace/tenant0/GbpSecGroup/secgrp2/in",
                              rules2)));

    delete v_itf;
}

BOOST_AUTO_TEST_SUITE_END()

/*
 * Local Variables:
 * eval: (c-set-style "llvm.org")
 * End:
 */
