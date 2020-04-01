/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for endpoint manager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflex/modb/ObjectListener.h>
#include <modelgbp/ascii/StringMatchTypeEnumT.hpp>
#include <modelgbp/gbp/RoutingModeEnumT.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem/fstream.hpp>

#include <opflexagent/FSEndpointSource.h>
#include <opflexagent/FSExternalEndpointSource.h>
#include <opflexagent/logging.h>

#include <opflexagent/test/BaseFixture.h>
#include <opflexagent/test/MockEndpointSource.h>

namespace opflexagent {

using std::string;
using std::vector;
using std::shared_ptr;
using opflex::modb::ObjectListener;
using opflex::modb::class_id_t;
using opflex::modb::URI;
using opflex::modb::MAC;
using opflex::modb::URIBuilder;
using opflex::modb::Mutator;
using opflex::ofcore::OFFramework;
using boost::optional;

namespace fs = boost::filesystem;
using namespace modelgbp;
using namespace modelgbp::epdr;
using namespace modelgbp::epr;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using namespace modelgbp::ascii;

class EndpointFixture : public BaseFixture, public ObjectListener {
public:
    EndpointFixture()
        : BaseFixture(),
          epSource(&agent.getEndpointManager()),
          bduri("/PolicyUniverse/PolicySpace/test/GbpBridgeDomain/bd/"),
          rduri("/PolicyUniverse/PolicySpace/test/GbpRoutingDomain/rd/") {
        Mutator mutator(framework, "policyreg");
        universe = policy::Universe::resolve(framework).get();
        space = universe->addPolicySpace("test");
        mutator.commit();

        EndPointToGroupRSrc::registerListener(framework, this);
        EpgMappingCtx::registerListener(framework, this);
        BridgeDomain::registerListener(framework, this);
        EpGroup::registerListener(framework, this);
        ExternalL3EpToPathAttRSrc::registerListener(framework, this);
        ExternalL3EpToNodeAttRSrc::registerListener(framework, this);
        ExternalInterface::registerListener(framework, this);
        ExternalL3BridgeDomain::registerListener(framework, this);
    }

    virtual ~EndpointFixture() {
        EndPointToGroupRSrc::unregisterListener(framework, this);
        EpgMappingCtx::unregisterListener(framework, this);
        BridgeDomain::unregisterListener(framework, this);
        EpGroup::unregisterListener(framework, this);
        ExternalL3EpToPathAttRSrc::unregisterListener(framework, this);
        ExternalL3EpToNodeAttRSrc::unregisterListener(framework, this);
        ExternalInterface::unregisterListener(framework, this);
        ExternalL3BridgeDomain::unregisterListener(framework, this);
    }

    void addBd(const std::string& name) {
        optional<shared_ptr<BridgeDomain> > bd =
            space->resolveGbpBridgeDomain(name);
        if (!bd)
            space->addGbpBridgeDomain(name)
                ->addGbpBridgeDomainToNetworkRSrc()
                ->setTargetRoutingDomain(rduri);
    }
    void rmBd(const std::string& name) {
        BridgeDomain::remove(framework, "test", name);
    }

    void addRd(const std::string& name) {
        optional<shared_ptr<RoutingDomain> > rd =
            space->resolveGbpRoutingDomain(name);
        if (!rd)
            space->addGbpRoutingDomain(name);
    }
    void rmRd(const std::string& name) {
        RoutingDomain::remove(framework, "test", name);
    }

    void addEpg(const std::string& name = "epg") {
        optional<shared_ptr<EpGroup> > epg =
            space->resolveGbpEpGroup(name);
        if (!epg)
            space->addGbpEpGroup(name)
                ->addGbpEpGroupToNetworkRSrc()
                ->setTargetBridgeDomain(bduri);
    }
    void rmEpg(const std::string& name = "epg") {
        RoutingDomain::remove(framework, "test", name);
    }

    void addEpgMapping() {
        optional<shared_ptr<EpgMapping> > mapping =
            universe->resolveGbpeEpgMapping("testmapping");
        if (!mapping)
            universe->addGbpeEpgMapping("testmapping")
                ->addGbpeEpgMappingToDefaultGroupRSrc()
                ->setTargetEpGroup("test", "epg");
    }
    void rmEpgMapping() {
        EpgMapping::remove(framework, "testmapping");
    }

    void addEpAttributeSet() {
        optional<shared_ptr<EpAttributeSet> > attrSet =
            VMUniverse::resolve(framework).get()
            ->resolveGbpeEpAttributeSet("72ffb982-b2d5-4ae4-91ac-0dd61daf527a");
        if (!attrSet) {
            VMUniverse::resolve(framework).get()
                ->addGbpeEpAttributeSet("72ffb982-b2d5-4ae4-91ac-0dd61daf527a")
                ->addGbpeEpAttribute("registryattr")
                ->setValue("attrvalue");
        }
    }
    void rmEpAttributeSet() {
        EpAttributeSet::remove(framework, "72ffb982-b2d5-4ae4-91ac-0dd61daf527a");
    }
    opflex::modb::URI getExtL3BDURI(std::string& name) {
        return URIBuilder().addElement("PolicyUniverse")
               .addElement("PolicySpace")
               .addElement("test")
               .addElement("GbpExternalL3BridgeDomain")
               .addElement(name).build();
    }
    void addExternalInterface(const std::string& name) {
        optional<shared_ptr<ExternalInterface> > extInt =
            ExternalInterface::resolve(framework, "test", name);
        std::shared_ptr<ExternalL3BridgeDomain> bd;
        if (!extInt) {
            std::string bd_name = name;
            bd_name += "bd";
            space->addGbpExternalInterface(name)
                ->addGbpExternalInterfaceToExtl3bdRSrc()
                ->setTargetExternalL3BridgeDomain(
                    getExtL3BDURI(bd_name));
         }
    }
    void addExtL3BD(const std::string& name="ext_int1bd") {
        optional<shared_ptr<ExternalL3BridgeDomain>> obj =
            ExternalL3BridgeDomain::resolve(framework, "test", name);
        if(!obj) {
            space->addGbpExternalL3BridgeDomain(name)
                 ->addGbpExternalL3BridgeDomainToVrfRSrc()
                 ->setTargetRoutingDomain(rduri);
        }
    }
    void addExternalNode(const std::string& name) {
        optional<shared_ptr<ExternalNode> > extNode =
            space->resolveGbpExternalNode(name);
        if (!extNode)
            space->addGbpExternalNode(name);
    }
    void rmExternalInterface(const std::string& name="ext_int1") {
        ExternalInterface::remove(framework, "test", name);
    }
    void rmExtL3BD(const std::string& name) {
        ExternalL3BridgeDomain::remove(framework, "test", name);
    }
    void rmExternalNode(const std::string& name="ext_node1") {
        ExternalNode::remove(framework, "test", name);
    }
    virtual void objectUpdated(class_id_t class_id,
                               const URI& uri) {
        // Simulate policy resolution from the policy repository by
        // writing the referenced object in response to any changes
        Mutator mutator(framework, "policyreg");

        switch (class_id) {
        case EndPointToGroupRSrc::CLASS_ID:
            {
                optional<shared_ptr<EndPointToGroupRSrc> > obj =
                    EndPointToGroupRSrc::resolve(framework, uri);
                if (obj) {
                    vector<string> elements;
                    obj.get()->getTargetURI().get().getElements(elements);
                    addEpg(elements.back());
                } else
                    rmEpg();
            }
            break;
        case EpgMappingCtx::CLASS_ID:
            {
                optional<shared_ptr<EpgMappingCtx> > obj =
                    EpgMappingCtx::resolve(framework, uri);
                if (obj) {
                    addEpgMapping();
                    addEpAttributeSet();
                } else {
                    rmEpgMapping();
                    rmEpAttributeSet();
                }
            }
            break;
        case LocalL3Ep::CLASS_ID:
            break;
        case BridgeDomain::CLASS_ID:
            {
                optional<shared_ptr<BridgeDomain> > obj =
                    BridgeDomain::resolve(framework, uri);
                if (obj) addRd("rd");
                else rmRd("rd");
            }
            break;
        case EpGroup::CLASS_ID:
            {
                optional<shared_ptr<EpGroup> > obj =
                    EpGroup::resolve(framework, uri);
                if (obj) addBd("bd");
                else rmBd("bd");
                break;
            }
        case ExternalL3EpToPathAttRSrc::CLASS_ID:
            {
                optional<shared_ptr<ExternalL3EpToPathAttRSrc> > obj =
                    ExternalL3EpToPathAttRSrc::resolve(framework, uri);
                vector<string> uriElems;
                uri.getElements(uriElems);
                if (obj) {
                    vector<string> elements;
                    obj.get()->getTargetURI().get().getElements(elements);
                    addExternalInterface(elements.back());
                    ext_int_map[uriElems[2]] = elements.back();
                } else {
                    auto it = ext_int_map.find(uriElems[2]);
                    if(it != ext_int_map.end()) {
                        rmExternalInterface(it->second);
                        ext_int_map.erase(it);
                    }
                }
                break;
            }
        case ExternalL3EpToNodeAttRSrc::CLASS_ID:
            {
                optional<shared_ptr<ExternalL3EpToNodeAttRSrc> > obj =
                    ExternalL3EpToNodeAttRSrc::resolve(framework, uri);
                vector<string> uriElems;
                uri.getElements(uriElems);
                if (obj) {
                    vector<string> elements;
                    obj.get()->getTargetURI().get().getElements(elements);
                    addExternalNode(elements.back());
                    ext_node_map[uriElems[2]] = elements.back();
                }
                else {
                    auto it = ext_node_map.find(uriElems[2]);
                    if(it != ext_node_map.end()) {
                        rmExternalNode(it->second);
                        ext_node_map.erase(it);
                    }
                }
                break;
            }
        case ExternalInterface::CLASS_ID:
            {
                optional<shared_ptr<ExternalInterface> > obj =
                    ExternalInterface::resolve(framework, uri);
                vector<string> elements;
                uri.getElements(elements);
                std::string bd_name = elements.back() + "bd";
                if (obj) addExtL3BD(bd_name);
                else rmExtL3BD(bd_name);
                break;
            }
        case ExternalL3BridgeDomain::CLASS_ID:
            {
                optional<shared_ptr<ExternalL3BridgeDomain> > obj =
                    ExternalL3BridgeDomain::resolve(framework, uri);
                if (obj) addRd("rd");
                else rmRd("rd");
                break;
            }
        default:
            break;
        }
        mutator.commit();
    }

    shared_ptr<policy::Universe> universe;
    shared_ptr<policy::Space> space;
    MockEndpointSource epSource;
    URI bduri;
    URI rduri;
    std::unordered_map<std::string,std::string> ext_int_map;
    std::unordered_map<std::string,std::string> ext_node_map;
};

class FSEndpointFixture : public EndpointFixture {
public:
    FSEndpointFixture()
        : EndpointFixture(),
          temp(fs::temp_directory_path() / fs::unique_path()) {
        fs::create_directory(temp);
    }

    ~FSEndpointFixture() {
        fs::remove_all(temp);
    }

    fs::path temp;
};

BOOST_AUTO_TEST_SUITE(EndpointManager_test)

template<typename T>
bool hasEPREntry(OFFramework& framework, const URI& uri,
                 const boost::optional<std::string>& uuid = boost::none) {
    boost::optional<std::shared_ptr<T> > entry =
        T::resolve(framework, uri);
    if (!entry) return false;
    if (uuid) return (entry.get()->getUuid("") == uuid);
    return true;
}

template<typename T>
bool hasPolicyEntry(OFFramework& framework, const URI& uri
                   ) {
    boost::optional<std::shared_ptr<T> > entry =
        T::resolve(framework, uri);
    if (!entry) return false;
    return true;
}

static int getEGSize(EndpointManager& epManager, URI& epgu) {
    std::unordered_set<std::string> epUuids;
    epManager.getEndpointsForGroup(epgu, epUuids);
    return epUuids.size();
}

BOOST_FIXTURE_TEST_CASE( basic, EndpointFixture ) {
    URI epgu = URI("/PolicyUniverse/PolicySpace/test/GbpEpGroup/epg/");
    Endpoint ep1("e82e883b-851d-4cc6-bedb-fb5e27530043");
    ep1.setMAC(MAC("00:00:00:00:00:01"));
    ep1.addIP("10.1.1.2");
    ep1.addIP("10.1.1.3");
    ep1.setInterfaceName("veth1");
    ep1.setEgURI(epgu);
    Endpoint ep2("72ffb982-b2d5-4ae4-91ac-0dd61daf527a");
    ep2.setMAC(MAC("00:00:00:00:00:02"));
    ep2.setInterfaceName("veth2");
    ep2.addIP("10.1.1.4");
    ep2.setEgURI(epgu);

    URI epgnat = URI("/PolicyUniverse/PolicySpace/test/GbpEpGroup/nat-epg/");
    Endpoint::IPAddressMapping ipm("91c5b217-d244-432c-922d-533c6c036ab3");
    ipm.setMappedIP("10.1.1.4");
    ipm.setFloatingIP("5.5.5.5");
    ipm.setEgURI(epgnat);
    ep2.addIPAddressMapping(ipm);

    epSource.updateEndpoint(ep1);
    epSource.updateEndpoint(ep2);

    std::unordered_set<std::string> epUuids;
    agent.getEndpointManager().getEndpointsForGroup(epgu, epUuids);
    BOOST_CHECK_EQUAL(2, epUuids.size());
    BOOST_CHECK(epUuids.find(ep1.getUUID()) != epUuids.end());
    BOOST_CHECK(epUuids.find(ep2.getUUID()) != epUuids.end());

    epSource.removeEndpoint(ep2.getUUID());
    epUuids.clear();
    agent.getEndpointManager().getEndpointsForGroup(epgu, epUuids);
    BOOST_CHECK_EQUAL(1, epUuids.size());
    BOOST_CHECK(epUuids.find(ep1.getUUID()) != epUuids.end());

    epSource.updateEndpoint(ep2);

    URI l2epr1 = URIBuilder()
        .addElement("EprL2Universe")
        .addElement("EprL2Ep")
        .addElement(bduri.toString())
        .addElement(MAC("00:00:00:00:00:01")).build();
    URI l2epr2 = URIBuilder()
        .addElement("EprL2Universe")
        .addElement("EprL2Ep")
        .addElement(bduri.toString())
        .addElement(MAC("00:00:00:00:00:02")).build();
    URI l2epr2_ipm = URIBuilder()
        .addElement("EprL2Universe")
        .addElement("EprL2Ep")
        .addElement(bduri.toString())
        .addElement(MAC("00:00:00:00:00:02")).build();
    URI l3epr1_2 = URIBuilder()
        .addElement("EprL3Universe")
        .addElement("EprL3Ep")
        .addElement(rduri.toString())
        .addElement("10.1.1.2").build();
    URI l3epr1_3 = URIBuilder()
        .addElement("EprL3Universe")
        .addElement("EprL3Ep")
        .addElement(rduri.toString())
        .addElement("10.1.1.3").build();
    URI l3epr2_4 = URIBuilder()
        .addElement("EprL3Universe")
        .addElement("EprL3Ep")
        .addElement(rduri.toString())
        .addElement("10.1.1.4").build();
    URI l3epr2_ipm = URIBuilder()
        .addElement("EprL3Universe")
        .addElement("EprL3Ep")
        .addElement(rduri.toString())
        .addElement("5.5.5.5").build();

    WAIT_FOR(hasEPREntry<L2Ep>(framework, l2epr1), 500);
    WAIT_FOR(hasEPREntry<L2Ep>(framework, l2epr2), 500);
    WAIT_FOR(hasEPREntry<L2Ep>(framework, l2epr2_ipm), 500);

    WAIT_FOR(hasEPREntry<L3Ep>(framework, l3epr1_2), 500);
    WAIT_FOR(hasEPREntry<L3Ep>(framework, l3epr1_3), 500);
    WAIT_FOR(hasEPREntry<L3Ep>(framework, l3epr2_4), 500);
    WAIT_FOR(hasEPREntry<L3Ep>(framework, l3epr2_ipm), 500);

    Mutator mutator(framework, "policyreg");
    optional<shared_ptr<BridgeDomain> > bd =
        BridgeDomain::resolve(framework, bduri);
    BOOST_REQUIRE(bd);
    bd.get()->setRoutingMode(RoutingModeEnumT::CONST_DISABLED);
    mutator.commit();

    WAIT_FOR(!hasEPREntry<L3Ep>(framework, l3epr1_2), 500);
    WAIT_FOR(!hasEPREntry<L3Ep>(framework, l3epr1_3), 500);
    WAIT_FOR(!hasEPREntry<L3Ep>(framework, l3epr2_4), 500);
    WAIT_FOR(!hasEPREntry<L3Ep>(framework, l3epr2_ipm), 500);
}

BOOST_FIXTURE_TEST_CASE( epgmapping, EndpointFixture ) {
    URI epgu = URI("/PolicyUniverse/PolicySpace/test/GbpEpGroup/epg/");
    URI epg2u = URI("/PolicyUniverse/PolicySpace/test/GbpEpGroup/epg2/");
    URI epg3u = URI("/PolicyUniverse/PolicySpace/test/GbpEpGroup/epg3/");
    Endpoint ep2("72ffb982-b2d5-4ae4-91ac-0dd61daf527a");
    ep2.setMAC(MAC("00:00:00:00:00:02"));
    ep2.setInterfaceName("veth2");
    ep2.addIP("10.1.1.4");
    ep2.setEgMappingAlias("testmapping");
    ep2.addAttribute("localattr", "asddsa");

    epSource.updateEndpoint(ep2);

    WAIT_FOR(1 == getEGSize(agent.getEndpointManager(), epgu), 500);
    std::unordered_set<std::string> epUuids;
    agent.getEndpointManager().getEndpointsForGroup(epgu, epUuids);
    BOOST_CHECK(epUuids.find(ep2.getUUID()) != epUuids.end());

    URI l2epr2 = URIBuilder()
        .addElement("EprL2Universe")
        .addElement("EprL2Ep")
        .addElement(bduri.toString())
        .addElement(MAC("00:00:00:00:00:02")).build();
    URI l3epr2_4 = URIBuilder()
        .addElement("EprL3Universe")
        .addElement("EprL3Ep")
        .addElement(rduri.toString())
        .addElement("10.1.1.4").build();

    WAIT_FOR(hasEPREntry<L2Ep>(framework, l2epr2), 500);
    WAIT_FOR(hasEPREntry<L3Ep>(framework, l3epr2_4), 500);

    Mutator mutator(framework, "policyreg");
    shared_ptr<EpgMapping> mapping =
        universe->resolveGbpeEpgMapping("testmapping").get();
    mapping->addGbpeAttributeMappingRule("rule1")
        ->setOrder(10)
        .setAttributeName("localattr")
        .setMatchString("asd")
        .setMatchType(StringMatchTypeEnumT::CONST_STARTSWITH)
        .addGbpeMappingRuleToGroupRSrc()
        ->setTargetEpGroup(epg2u);
    mutator.commit();

    WAIT_FOR(1 == getEGSize(agent.getEndpointManager(), epg2u), 500);
    WAIT_FOR(0 == getEGSize(agent.getEndpointManager(), epgu), 500);

    mapping = universe->resolveGbpeEpgMapping("testmapping").get();
    mapping->addGbpeAttributeMappingRule("rule2")
        ->setOrder(9)
        .setAttributeName("registryattr")
        .setMatchString("value")
        .setMatchType(StringMatchTypeEnumT::CONST_ENDSWITH)
        .addGbpeMappingRuleToGroupRSrc()
        ->setTargetEpGroup(epg3u);
    mutator.commit();

    WAIT_FOR(1 == getEGSize(agent.getEndpointManager(), epg3u), 500);
    WAIT_FOR(0 == getEGSize(agent.getEndpointManager(), epg2u), 500);
    WAIT_FOR(0 == getEGSize(agent.getEndpointManager(), epgu), 500);

    mapping = universe->resolveGbpeEpgMapping("testmapping").get();
    mapping->addGbpeAttributeMappingRule("rule3")
        ->setOrder(8)
        .setAttributeName("registryattr")
        .setMatchString("attrvalue")
        .setMatchType(StringMatchTypeEnumT::CONST_EQUALS)
        .addGbpeMappingRuleToGroupRSrc()
        ->setTargetEpGroup(epg2u);
    mutator.commit();

    mapping = universe->resolveGbpeEpgMapping("testmapping").get();
    mapping->addGbpeAttributeMappingRule("rule4")
        ->setOrder(7)
        .setAttributeName("localattr")
        .setMatchString("sdds")
        .setMatchType(StringMatchTypeEnumT::CONST_CONTAINS)
        .addGbpeMappingRuleToGroupRSrc()
        ->setTargetEpGroup(epg2u);
    mutator.commit();

    WAIT_FOR(0 == getEGSize(agent.getEndpointManager(), epg3u), 500);
    WAIT_FOR(1 == getEGSize(agent.getEndpointManager(), epg2u), 500);
    WAIT_FOR(0 == getEGSize(agent.getEndpointManager(), epgu), 500);

    mapping = universe->resolveGbpeEpgMapping("testmapping").get();
    mapping->addGbpeAttributeMappingRule("rule5")
        ->setOrder(6)
        .setAttributeName("nothing")
        .setMatchString("lksdflkjsd")
        .setMatchType(StringMatchTypeEnumT::CONST_EQUALS)
        .setNegated(1)
        .addGbpeMappingRuleToGroupRSrc()
        ->setTargetEpGroup(epgu);
    mutator.commit();

    WAIT_FOR(0 == getEGSize(agent.getEndpointManager(), epg3u), 500);
    WAIT_FOR(0 == getEGSize(agent.getEndpointManager(), epg2u), 500);
    WAIT_FOR(1 == getEGSize(agent.getEndpointManager(), epgu), 500);
}

BOOST_FIXTURE_TEST_CASE( fssource, FSEndpointFixture ) {

    // check already existing ep file
    fs::path path1(temp / "83f18f0b-80f7-46e2-b06c-4d9487b0c754.ep");
    fs::ofstream os(path1);
    os << "{"
       << "\"uuid\":\"83f18f0b-80f7-46e2-b06c-4d9487b0c754\","
       << "\"mac\":\"10:ff:00:a3:01:00\","
       << "\"ip\":[\"10.0.0.1\",\"10.0.0.2\",\"10.0.0.3\"],"
       << "\"interface-name\":\"veth0\","
       << "\"endpoint-group\":\"/PolicyUniverse/PolicySpace/test/GbpEpGroup/epg/\","
       << "\"security-group\":["
       << "{\"policy-space\":\"sg1-space1\",\"name\":\"sg1\"},"
       << "{\"policy-space\":\"sg2-space2\",\"name\":\"sg2\"}"
       << "],"
       << "\"attributes\":{"
       << "\"attr1\":\"value1\",\"attr2\":\"value2\""
       << "}"
       << "}" << std::endl;
    os.close();

    FSWatcher watcher;
    FSEndpointSource source(&agent.getEndpointManager(), watcher,
                             temp.string());
    watcher.start();

    URI l2epr = URIBuilder()
        .addElement("EprL2Universe")
        .addElement("EprL2Ep")
        .addElement(bduri.toString())
        .addElement(MAC("10:ff:00:a3:01:00")).build();
    URI l3epr_1 = URIBuilder()
        .addElement("EprL3Universe")
        .addElement("EprL3Ep")
        .addElement(rduri.toString())
        .addElement("10.0.0.1").build();
    URI l3epr_2 = URIBuilder()
        .addElement("EprL3Universe")
        .addElement("EprL3Ep")
        .addElement(rduri.toString())
        .addElement("10.0.0.2").build();
    URI l3epdr_1 = URIBuilder()
        .addElement("EpdrL3Discovered")
        .addElement("EpdrLocalL3Ep")
        .addElement("10.0.0.1").build();
    URI l3epdr_2 = URIBuilder()
        .addElement("EpdrL3Discovered")
        .addElement("EpdrLocalL3Ep")
        .addElement("10.0.0.2").build();
    URI l3epdr_3 = URIBuilder()
        .addElement("EpdrL3Discovered")
        .addElement("EpdrLocalL3Ep")
        .addElement("10.0.0.3").build();


    URI sgc1 = URIBuilder()
        .addElement("PolicyUniverse")
        .addElement("PolicySpace")
        .addElement("sg1-space1")
        .addElement("GbpSecGroup")
        .addElement("sg1")
        .build();
    URI l2sgc_1 = URIBuilder(l2epr)
        .addElement("EprSecurityGroupContext")
        .addElement(sgc1.toString())
        .build();
    URI l31sgc_1 = URIBuilder(l3epr_1)
        .addElement("EprSecurityGroupContext")
        .addElement(sgc1.toString())
        .build();
    URI l32sgc_1 = URIBuilder(l3epr_2)
        .addElement("EprSecurityGroupContext")
        .addElement(sgc1.toString())
        .build();

    URI sgc2 = URIBuilder()
        .addElement("PolicyUniverse")
        .addElement("PolicySpace")
        .addElement("sg2-space2")
        .addElement("GbpSecGroup")
        .addElement("sg2")
        .build();
    URI l2sgc_2 = URIBuilder(l2epr)
        .addElement("EprSecurityGroupContext")
        .addElement(sgc2.toString())
        .build();
    URI l31sgc_2 = URIBuilder(l3epr_1)
        .addElement("EprSecurityGroupContext")
        .addElement(sgc2.toString())
        .build();
    URI l32sgc_2 = URIBuilder(l3epr_2)
        .addElement("EprSecurityGroupContext")
        .addElement(sgc2.toString())
        .build();

    URI epset = URIBuilder(l2epr)
        .addElement("GbpeReportedEpAttributeSet")
        .build();
    URI epattr_1 = URIBuilder(epset)
        .addElement("GbpeReportedEpAttribute")
        .addElement("attr1")
        .build();
    URI epattr_2 = URIBuilder(epset)
        .addElement("GbpeReportedEpAttribute")
        .addElement("attr2")
        .build();

    WAIT_FOR(hasEPREntry<L3Ep>(framework, l3epr_1), 500);
    WAIT_FOR(hasEPREntry<L3Ep>(framework, l3epr_2), 500);
    WAIT_FOR(hasEPREntry<L2Ep>(framework, l2epr), 500);
    WAIT_FOR(hasPolicyEntry<LocalL3Ep>(framework, l3epdr_1), 500);
    WAIT_FOR(hasPolicyEntry<LocalL3Ep>(framework, l3epdr_2), 500);
    WAIT_FOR(hasPolicyEntry<LocalL3Ep>(framework, l3epdr_3), 500);
    WAIT_FOR(hasPolicyEntry<SecurityGroupContext>(framework, l2sgc_1), 500);
    WAIT_FOR(hasPolicyEntry<SecurityGroupContext>(framework, l2sgc_2), 500);
    WAIT_FOR(hasPolicyEntry<SecurityGroupContext>(framework, l31sgc_1), 500);
    WAIT_FOR(hasPolicyEntry<SecurityGroupContext>(framework, l31sgc_2), 500);
    WAIT_FOR(hasPolicyEntry<SecurityGroupContext>(framework, l32sgc_1), 500);
    WAIT_FOR(hasPolicyEntry<SecurityGroupContext>(framework, l32sgc_2), 500);
    WAIT_FOR(hasPolicyEntry<ReportedEpAttribute>(framework, epattr_1), 500);
    WAIT_FOR(hasPolicyEntry<ReportedEpAttribute>(framework, epattr_2), 500);

    // Check updates to existing file: attr delete, sec grp delete, IP delete
    fs::ofstream os2(path1);
    os2 << "{"
       << "\"uuid\":\"83f18f0b-80f7-46e2-b06c-4d9487b0c754\","
       << "\"mac\":\"10:ff:00:a3:01:00\","
       << "\"ip\":[\"10.0.0.1\",\"10.0.0.2\"],"
       << "\"interface-name\":\"veth0\","
       << "\"endpoint-group\":\"/PolicyUniverse/PolicySpace/test/GbpEpGroup/epg/\","
       << "\"security-group\":["
       << "{\"policy-space\":\"sg1-space1\",\"name\":\"sg1\"}"
       << "],"
       << "\"attributes\":{"
       << "\"attr1\":\"value1\""
       << "}"
       << "}" << std::endl;
    os2.close();

    WAIT_FOR(hasEPREntry<L3Ep>(framework, l3epr_1), 500);
    WAIT_FOR(hasEPREntry<L3Ep>(framework, l3epr_2), 500);
    WAIT_FOR(hasPolicyEntry<LocalL3Ep>(framework, l3epdr_1), 500);
    WAIT_FOR(hasPolicyEntry<LocalL3Ep>(framework, l3epdr_2), 500);
    WAIT_FOR(!hasPolicyEntry<LocalL3Ep>(framework, l3epdr_3), 500);
    WAIT_FOR(hasEPREntry<L2Ep>(framework, l2epr), 500);
    WAIT_FOR(hasPolicyEntry<SecurityGroupContext>(framework, l2sgc_1), 500);
    WAIT_FOR(!hasPolicyEntry<SecurityGroupContext>(framework, l2sgc_2), 500);
    WAIT_FOR(hasPolicyEntry<SecurityGroupContext>(framework, l31sgc_1), 500);
    WAIT_FOR(!hasPolicyEntry<SecurityGroupContext>(framework, l31sgc_2), 500);
    WAIT_FOR(hasPolicyEntry<SecurityGroupContext>(framework, l32sgc_1), 500);
    WAIT_FOR(!hasPolicyEntry<SecurityGroupContext>(framework, l32sgc_2), 500);
    WAIT_FOR(hasPolicyEntry<ReportedEpAttribute>(framework, epattr_1), 500);
    WAIT_FOR(!hasPolicyEntry<ReportedEpAttribute>(framework, epattr_2), 500);

    // check for overwriting existing file with new ep
    fs::ofstream os3(path1);
    std::string uuid3("83f18f0b-80f7-46e2-b06c-4d9487b0c756");
    os3 << "{"
        << "\"uuid\":\"" << uuid3 << "\","
        << "\"mac\":\"10:ff:00:a3:01:02\","
        << "\"ip\":[\"10.0.0.4\"],"
        << "\"interface-name\":\"veth0\","
        << "\"endpoint-group\":\"/PolicyUniverse/PolicySpace/test/GbpEpGroup/epg/\","
        << "\"attributes\":{\"attr1\":\"value1\"}"
        << "}" << std::endl;
    os3.close();

    URI l3epdr_4 = URIBuilder()
        .addElement("EpdrL3Discovered")
        .addElement("EpdrLocalL3Ep")
        .addElement("10.0.0.4").build();

    WAIT_FOR(!hasEPREntry<L3Ep>(framework, l3epr_1), 500);
    WAIT_FOR(!hasEPREntry<L3Ep>(framework, l3epr_2), 500);
    WAIT_FOR(!hasPolicyEntry<LocalL3Ep>(framework, l3epdr_1), 500);
    WAIT_FOR(!hasPolicyEntry<LocalL3Ep>(framework, l3epdr_2), 500);
    WAIT_FOR(hasPolicyEntry<LocalL3Ep>(framework, l3epdr_4), 500);
    WAIT_FOR(!hasEPREntry<L2Ep>(framework, l2epr), 500);
    WAIT_FOR(!hasPolicyEntry<SecurityGroupContext>(framework, l2sgc_1), 500);
    WAIT_FOR(!hasPolicyEntry<SecurityGroupContext>(framework, l2sgc_2), 500);
    WAIT_FOR(!hasPolicyEntry<SecurityGroupContext>(framework, l31sgc_1), 500);
    WAIT_FOR(!hasPolicyEntry<SecurityGroupContext>(framework, l31sgc_2), 500);
    WAIT_FOR(!hasPolicyEntry<SecurityGroupContext>(framework, l32sgc_1), 500);
    WAIT_FOR(!hasPolicyEntry<SecurityGroupContext>(framework, l32sgc_2), 500);
    WAIT_FOR(!hasPolicyEntry<ReportedEpAttribute>(framework, epattr_1), 500);
    WAIT_FOR(!hasPolicyEntry<ReportedEpAttribute>(framework, epattr_2), 500);

    URI l2epr3 = URIBuilder()
        .addElement("EprL2Universe")
        .addElement("EprL2Ep")
        .addElement(bduri.toString())
        .addElement(MAC("10:ff:00:a3:01:02")).build();

    epset = URIBuilder(l2epr3)
        .addElement("GbpeReportedEpAttributeSet")
        .build();
    epattr_1 = URIBuilder(epset)
        .addElement("GbpeReportedEpAttribute")
        .addElement("attr1")
        .build();

    WAIT_FOR(hasEPREntry<L2Ep>(framework, l2epr3, uuid3), 500);
    WAIT_FOR(hasPolicyEntry<ReportedEpAttribute>(framework, epattr_1), 500);

    // check for a new EP added to watch directory
    URI l2epr2 = URIBuilder()
        .addElement("EprL2Universe")
        .addElement("EprL2Ep")
        .addElement(bduri.toString())
        .addElement(MAC("10:ff:00:a3:01:01")).build();

    epset = URIBuilder(l2epr2)
        .addElement("GbpeReportedEpAttributeSet")
        .build();
    epattr_1 = URIBuilder(epset)
        .addElement("GbpeReportedEpAttribute")
        .addElement("attr2")
        .build();

    fs::path path2(temp / "83f18f0b-80f7-46e2-b06c-4d9487b0c755.ep");
    fs::ofstream os4(path2);
    os4 << "{"
       << "\"uuid\":\"83f18f0b-80f7-46e2-b06c-4d9487b0c755\","
       << "\"mac\":\"10:ff:00:a3:01:01\","
       << "\"ip\":[\"10.0.0.3\"],"
       << "\"interface-name\":\"veth1\","
       << "\"endpoint-group\":\"/PolicyUniverse/PolicySpace/test/GbpEpGroup/epg/\","
       << "\"attributes\":{\"attr2\":\"value2\"}"
       << "}" << std::endl;
    os4.close();

    WAIT_FOR(hasEPREntry<L2Ep>(framework, l2epr2), 500);
    WAIT_FOR(hasPolicyEntry<LocalL3Ep>(framework, l3epdr_3), 500);
    WAIT_FOR(hasPolicyEntry<ReportedEpAttribute>(framework, epattr_1), 500);

    // check for removing an endpoint
    fs::remove(path2);

    WAIT_FOR(!hasEPREntry<L2Ep>(framework, l2epr2), 500);
    WAIT_FOR(!hasPolicyEntry<LocalL3Ep>(framework, l3epdr_3), 500);
    WAIT_FOR(!hasPolicyEntry<ReportedEpAttribute>(framework, epattr_1), 500);

    watcher.stop();
}

class MockEndpointListener : public EndpointListener {
public:
    virtual void endpointUpdated(const std::string& uuid) {};
    virtual void remoteEndpointUpdated(const std::string& uuid) {
        std::unique_lock<std::mutex> guard(mutex);
        updates.insert(uuid);
    };

    size_t numUpdates() {
        std::unique_lock<std::mutex> guard(mutex);
        return updates.size();
    }
    void clear() {
        std::unique_lock<std::mutex> guard(mutex);
        updates.clear();
    }

    std::mutex mutex;
    std::unordered_set<std::string> updates;
};

BOOST_FIXTURE_TEST_CASE( remoteEndpoint, BaseFixture ) {
    MockEndpointListener listener;
    agent.getEndpointManager().registerListener(&listener);

    Mutator m(framework, "policyreg");
    auto universe = policy::Universe::resolve(framework).get();
    auto space = universe->addPolicySpace("tenant0");
    auto bd0 = space->addGbpBridgeDomain("bd0");
    auto bd1 = space->addGbpBridgeDomain("bd1");
    auto epg0 = space->addGbpEpGroup("epg0");
    auto epg1 = space->addGbpEpGroup("epg1");
    auto invu = modelgbp::inv::Universe::resolve(framework);
    auto inv = invu.get()->addInvRemoteEndpointInventory();
    auto rep1 = inv->addInvRemoteInventoryEp("ep1");
    rep1->setMac(MAC("ab:cd:ef:ab:cd:ef"))
        .setNextHopTunnel("5.6.7.8")
        .addInvRemoteInventoryEpToGroupRSrc()
        ->setTargetEpGroup(epg0->getURI());
    auto rep2 = inv->addInvRemoteInventoryEp("ep2");
    rep2->setMac(MAC("ab:cd:ef:ab:cd:ff"))
        .setNextHopTunnel("5.6.7.9")
        .addInvRemoteInventoryEpToGroupRSrc()
        ->setTargetEpGroup(epg0->getURI());
    auto rep3 = inv->addInvRemoteInventoryEp("ep3");
    rep3->addInvRemoteInventoryEpToGroupRSrc()
        ->setTargetEpGroup(epg0->getURI());
    m.commit();

    // basic ep add
    WAIT_FOR(agent.getPolicyManager().groupExists(epg0->getURI()), 500);
    WAIT_FOR(agent.getPolicyManager().groupExists(epg1->getURI()), 500);
    WAIT_FOR(listener.numUpdates() == 3, 500);
    listener.clear();

    // update epg for ep
    rep1->addInvRemoteInventoryEpToGroupRSrc()
        ->setTargetEpGroup(epg1->getURI());
    m.commit();

    WAIT_FOR(listener.numUpdates() == 1, 500);
    listener.clear();

    // eg domain update
    epg0->addGbpEpGroupToNetworkRSrc()
        ->setTargetBridgeDomain(bd0->getURI());
    m.commit();
    WAIT_FOR(agent.getPolicyManager().getBDForGroup(epg0->getURI()), 500);
    WAIT_FOR(agent.getPolicyManager().getBDForGroup(epg0->getURI())
             .get()->getURI() == bd0->getURI(), 500);
    WAIT_FOR(listener.numUpdates() == 3, 500);
    listener.clear();

    // ep remove
    rep2->remove();
    m.commit();
    WAIT_FOR(listener.numUpdates() == 1, 500);
    listener.clear();

    // eg domain update (again)
    epg0->addGbpEpGroupToNetworkRSrc()
        ->setTargetBridgeDomain(bd1->getURI());
    m.commit();
    WAIT_FOR(agent.getPolicyManager().getBDForGroup(epg0->getURI())
             .get()->getURI() == bd1->getURI(), 500);
    WAIT_FOR(listener.numUpdates() == 1, 500);
    listener.clear();

    agent.getEndpointManager().unregisterListener(&listener);
}

BOOST_FIXTURE_TEST_CASE( fsextsource, FSEndpointFixture ) {

    // check already existing
    fs::path path1(temp / "83f18f0b-80f7-46e2-b06c-4d9487b0c790.extep");
    fs::ofstream os(path1);
    os << "{"
       << "\"uuid\":\"83f18f0b-80f7-46e2-b06c-4d9487b0c790\","
       << "\"mac\":\"10:ff:00:a3:02:00\","
       << "\"ip\":[\"10.0.0.2\"],"
       << "\"interface-name\":\"veth0\","
       << "\"policy-space-name\":\"test\","
       << "\"path-attachment\":\"ext_int1\","
       << "\"node-attachment\":\"ext_node1\""
       << "}" << std::endl;
    os.close();

    FSWatcher watcher;
    FSExternalEndpointSource source(&agent.getEndpointManager(), watcher,
                             temp.string());
    watcher.start();

    URI extInt1 = URIBuilder()
        .addElement("PolicyUniverse")
        .addElement("PolicySpace")
        .addElement("test")
        .addElement("GbpExternalInterface")
        .addElement("ext_int1").build();

    URI extbd1 = URIBuilder()
        .addElement("PolicyUniverse")
        .addElement("PolicySpace")
        .addElement("test")
        .addElement("GbpExternalL3BridgeDomain")
        .addElement("ext_int1bd").build();

    WAIT_FOR(hasPolicyEntry<ExternalInterface>(framework, extInt1), 500);
    WAIT_FOR(hasPolicyEntry<ExternalL3BridgeDomain>(framework, extbd1), 500);

    // check for a new EP added to watch directory
    fs::path path2(temp / "83f18f0b-80f7-46e2-b06c-4d9487b0c791.extep");
    fs::ofstream os2(path2);
    os2 << "{"
       << "\"uuid\":\"83f18f0b-80f7-46e2-b06c-4d9487b0c791\","
       << "\"mac\":\"10:ff:00:a3:02:01\","
       << "\"ip\":[\"10.0.0.10\"],"
       << "\"interface-name\":\"veth1\","
       << "\"policy-space-name\":\"test\","
       << "\"path-attachment\":\"ext_int2\","
       << "\"node-attachment\":\"ext_node2\""
       << "}" << std::endl;
    os2.close();

    URI extInt2 = URIBuilder()
        .addElement("PolicyUniverse")
        .addElement("PolicySpace")
        .addElement("test")
        .addElement("GbpExternalInterface")
        .addElement("ext_int2").build();

    URI extbd2 = URIBuilder()
        .addElement("PolicyUniverse")
        .addElement("PolicySpace")
        .addElement("test")
        .addElement("GbpExternalL3BridgeDomain")
        .addElement("ext_int2bd").build();

    URI extL3Ep2 = URIBuilder()
        .addElement("EpdrExternalDiscovered")
        .addElement("EpdrExternalL3Ep")
        .addElement("83f18f0b-80f7-46e2-b06c-4d9487b0c791")
        .build();

    WAIT_FOR(hasPolicyEntry<ExternalInterface>(framework, extInt2), 500);
    WAIT_FOR(hasPolicyEntry<ExternalL3BridgeDomain>(framework, extbd2), 500);

    // check for adjacency
    optional<opflex::modb::URI> rdURI = URIBuilder()
                                .addElement("PolicyUniverse")
                                .addElement("PolicySpace")
                                .addElement("test")
                                .addElement("GbpRoutingDomain")
                                .addElement("rd").build();

    std::string ep_ip_address("10.0.0.10");
    std::shared_ptr<const Endpoint> ep_new;
    WAIT_FOR(agent.getEndpointManager().getAdjacency(rdURI.get(),
                                            ep_ip_address,
                                            ep_new), 3000);
    boost::optional<opflex::modb::MAC> mac = ep_new->getMAC();
    boost::optional<std::string> accIntf = ep_new->getInterfaceName();
    std::string assigned_intf("veth1");
    opflex::modb::MAC assigned_mac("10:ff:00:a3:02:01");
    BOOST_CHECK(mac == assigned_mac);
    BOOST_CHECK(accIntf == assigned_intf);

    // check for removing an endpoint
    fs::remove(path2);

    WAIT_FOR(!hasPolicyEntry<ExternalL3Ep>(framework, extL3Ep2), 500);

    // check for overwriting existing file with new ep
    fs::ofstream os3(path1);
    std::string uuid3("83f18f0b-80f7-46e2-b06c-4d9487b0c792");
    os3 << "{"
        << "\"uuid\":\"" << uuid3 << "\","
        << "\"mac\":\"10:ff:00:a3:01:02\","
        << "\"ip\":[\"10.0.0.4\"],"
        << "\"interface-name\":\"veth0\","
        << "\"policy-space-name\":\"test\","
        << "\"path-attachment\":\"ext_int2\","
        << "\"node-attachment\":\"ext_node2\""
        << "}" << std::endl;
    os3.close();

    URI extL3Ep3 = URIBuilder()
        .addElement("EpdrExternalDiscovered")
        .addElement("EpdrExternalL3Ep")
        .addElement("83f18f0b-80f7-46e2-b06c-4d9487b0c792")
        .build();
    WAIT_FOR(hasPolicyEntry<ExternalL3Ep>(framework, extL3Ep3), 500);

}

BOOST_FIXTURE_TEST_CASE( fsextsvisource, FSEndpointFixture ) {

    // check for a new EP added to watch directory
    fs::path path1(temp / "83f18f0b-80f7-46e2-b06c-4d9487b0c793.ep");
    fs::ofstream os(path1);
    os << "{"
       << "\"uuid\":\"83f18f0b-80f7-46e2-b06c-4d9487b0c793\","
       << "\"mac\":\"10:ff:00:a3:01:03\","
       << "\"ip\":[\"10.1.0.3\"],"
       << "\"interface-name\":\"veth0\","
       << "\"policy-space-name\":\"test-ext-svi\","
       << "\"provider-vlan\" : true,"
       << "\"ext-encap-id\": 1000,"
       << "\"endpoint-group\":\"/PolicyUniverse/PolicySpace/test/ExtSviBD1/\""
       << "}" << std::endl;
    os.close();
    FSWatcher watcher;
    FSEndpointSource source(&agent.getEndpointManager(), watcher,
                             temp.string());
    watcher.start();
    WAIT_FOR((agent.getEndpointManager().getEndpoint(
            "83f18f0b-80f7-46e2-b06c-4d9487b0c793") != nullptr), 500);
    auto extSviEp = agent.getEndpointManager().getEndpoint(
            "83f18f0b-80f7-46e2-b06c-4d9487b0c793");
    BOOST_CHECK(extSviEp->isExternal() == true);
    BOOST_CHECK(extSviEp->getExtEncapId() == 1000);

    // check for removing an endpoint
    fs::remove(path1);

    WAIT_FOR((agent.getEndpointManager().getEndpoint(
            "83f18f0b-80f7-46e2-b06c-4d9487b0c793") == nullptr), 500);

}

BOOST_AUTO_TEST_SUITE_END()

} /* namespace opflexagent */
