/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for learning bridge manager
 *
 * Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/LearningBridgeManager.h>
#include <opflexagent/FSLearningBridgeSource.h>
#include <opflexagent/logging.h>

#include <opflexagent/test/BaseFixture.h>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/join.hpp>

#include <sstream>

namespace opflexagent {

namespace fs = boost::filesystem;

class MockLBSource : public LearningBridgeSource {
public:
    MockLBSource(LearningBridgeManager* manager)
        : LearningBridgeSource(manager) {

    }

    virtual ~MockLBSource() { }

    virtual void start() { }

    virtual void stop() { }
};

typedef std::set<LearningBridgeIface::vlan_range_t> range_set_t;

class MockLBListener : public LearningBridgeListener {
public:
    virtual void lbVlanUpdated(LearningBridgeIface::vlan_range_t vlan) {
        std::unique_lock<std::mutex> guard(mutex);
        vlanUpdates.insert(vlan);
    };

    size_t numUpdates() {
        std::unique_lock<std::mutex> guard(mutex);
        return vlanUpdates.size();
    }
    void clear() {
        std::unique_lock<std::mutex> guard(mutex);
        vlanUpdates.clear();
    }
    void logUpdates() {
        std::stringstream ss;
        for (auto& r: vlanUpdates) {
            ss << r << " ";
        };
        LOG(DEBUG) << "Updates: " << ss.str();
    }

    std::mutex mutex;
    range_set_t vlanUpdates;
};

class LBFixture : public BaseFixture {
public:
    LBFixture()
        : BaseFixture(),
          lbSource(&agent.getLearningBridgeManager()) {
    }

    virtual ~LBFixture() {
    }

    MockLBSource lbSource;
};

class FSLBFixture : public LBFixture {
public:
    FSLBFixture()
        : LBFixture(),
          temp(fs::temp_directory_path() / fs::unique_path()) {
        fs::create_directory(temp);
    }

    ~FSLBFixture() {
        fs::remove_all(temp);
    }

    fs::path temp;
};

BOOST_AUTO_TEST_SUITE(LearningBridgeManager_test)

BOOST_FIXTURE_TEST_CASE( basic, LBFixture ) {

}

BOOST_FIXTURE_TEST_CASE( vlans, LBFixture ) {
    typedef LearningBridgeIface::vlan_range_t vlan_range_t;
    typedef std::vector<std::pair<vlan_range_t,
                                  std::unordered_set<std::string>>> expected_t;
    auto f = [&]() -> expected_t {
        expected_t r;
        std::stringstream ss;
        agent.getLearningBridgeManager().
            forEachVlanRange([&](vlan_range_t range,
                                 const std::unordered_set<std::string>& set) {
                                 ss << "{" << range <<
                                     ": {" << boost::join(set, ",") << "}} ";
                                 r.emplace_back(range, set);
                             });
        LOG(DEBUG) << "Result: " << ss.str();
        return r;
    };

    typedef std::vector<vlan_range_t> ranges_t;
    struct test_case {
        ranges_t input;
        range_set_t notify;
        expected_t expected;
        range_set_t remove_notify;
        expected_t remove_expected;
    };

    const std::vector<test_case> test_cases {
        {
            { {10,20}, {30,40}, {45,50} },
            { {10,20}, {30,40}, {45,50} },
            { {{10,20}, {"0"}}, {{30,40}, {"0"}}, {{45,50}, {"0"}} },
            { {10,10}, {11,14}, {15,20}, {30,30}, {31,39},
              {40,40}, {45,49}, {50,50} },
            { {{5,9}, {"3","6"}}, {{10,10}, {"3","6"}},
              {{11,14}, {"6"}}, {{15,20}, {"1"}},
              {{21,25}, {"1"}}, {{30,30}, {"4"}},
              {{31,39}, {"4","5"}}, {{40,40}, {"4"}},
              {{50,50}, {"2"}}, {{51,51}, {"2"}} }
        },
        {
            { {15,25} },
            { {10,20}, {10,14}, {15,20}, {21,25} },
            { {{10,14}, {"0"}}, {{15,20}, {"0","1"}}, {{21,25}, {"1"}},
              {{30,40}, {"0"}}, {{45,50}, {"0"}} },
            { {15,20}, {21,25} },
            { {{5,9}, {"3","6"}}, {{10,10}, {"3","6"}},
              {{11,14}, {"6"}}, {{30,30}, {"4"}},
              {{31,39}, {"4","5"}}, {{40,40}, {"4"}},
              {{50,50}, {"2"}}, {{51,51}, {"2"}} }
        },
        {
            { {50,51} },
            { {45,50}, {45,49}, {50,50}, {51,51} },
            { {{10,14}, {"0"}}, {{15,20}, {"0","1"}}, {{21,25}, {"1"}},
              {{30,40}, {"0"}}, {{45,49}, {"0"}}, {{50,50}, {"0","2"}},
              {{51,51}, {"2"}} },
            { {50,50}, {51,51} },
            { {{5,9}, {"3","6"}}, {{10,10}, {"3","6"}},
              {{11,14}, {"6"}}, {{30,30}, {"4"}},
              {{31,39}, {"4","5"}}, {{40,40}, {"4"}} }
        },
        {
            { {5,10} },
            { {5,9}, {10,10}, {10,14}, {11,14} },
            { {{5,9}, {"3"}}, {{10,10}, {"0","3"}}, {{11,14}, {"0"}},
              {{15,20}, {"0","1"}}, {{21,25}, {"1"}}, {{30,40}, {"0"}},
              {{45,49}, {"0"}}, {{50,50}, {"0","2"}}, {{51,51}, {"2"}} },
            { {5,9}, {10,10} },
            { {{5,9}, {"6"}}, {{10,10}, {"6"}},
              {{11,14}, {"6"}}, {{30,30}, {"4"}},
              {{31,39}, {"4","5"}}, {{40,40}, {"4"}} }
        },
        {
            { {30,40} },
            { {30,40} },
            { {{5,9}, {"3"}}, {{10,10}, {"0","3"}}, {{11,14}, {"0"}},
              {{15,20}, {"0","1"}}, {{21,25}, {"1"}}, {{30,40}, {"0","4"}},
              {{45,49}, {"0"}}, {{50,50}, {"0","2"}}, {{51,51}, {"2"}} },
            { {30,30}, {31,39}, {40,40} },
            { {{5,9}, {"6"}}, {{10,10}, {"6"}},
              {{11,14}, {"6"}}, {{31,39}, {"5"}} }
        },
        {
            { {31,39} },
            { {30,40}, {30,30}, {31,39}, {40,40} },
            { {{5,9}, {"3"}}, {{10,10}, {"0","3"}}, {{11,14}, {"0"}},
              {{15,20}, {"0","1"}}, {{21,25}, {"1"}}, {{30,30}, {"0","4"}},
              {{31,39}, {"0","4","5"}}, {{40,40}, {"0","4"}},
              {{45,49}, {"0"}}, {{50,50}, {"0","2"}}, {{51,51}, {"2"}} },
            { {31,39} },
            { {{5,9}, {"6"}}, {{10,10}, {"6"}}, {{11,14}, {"6"}} }
        },
        {
            { {5,14} },
            { {5,9}, {10,10}, {11,14} },
            { {{5,9}, {"3","6"}}, {{10,10}, {"0","3","6"}},
              {{11,14}, {"0","6"}}, {{15,20}, {"0","1"}},
              {{21,25}, {"1"}}, {{30,30}, {"0","4"}},
              {{31,39}, {"0","4","5"}}, {{40,40}, {"0","4"}},
              {{45,49}, {"0"}}, {{50,50}, {"0","2"}}, {{51,51}, {"2"}} },
            { {5,9}, {10,10}, {11,14} },
            { }
        }
    };

    MockLBListener listener;
    agent.getLearningBridgeManager().registerListener(&listener);

    for (size_t i = 0; i < test_cases.size(); i++) {
        LOG(INFO) << "Checking " << i;
        LearningBridgeIface iface;
        iface.setUUID(boost::lexical_cast<std::string>(i));
        for (const auto& r : test_cases[i].input) {
            iface.addTrunkVlans(r);
        }
        lbSource.updateLBIface(iface);
        BOOST_CHECK(test_cases[i].expected == f());
        listener.logUpdates();
        BOOST_CHECK(test_cases[i].notify == listener.vlanUpdates);
        listener.clear();
    }

    for (size_t i = 0; i < test_cases.size(); i++) {
        LOG(INFO) << "Checking remove " << i;
        lbSource.removeLBIface(boost::lexical_cast<std::string>(i));
        BOOST_CHECK(test_cases[i].remove_expected == f());
        listener.logUpdates();
        BOOST_CHECK(test_cases[i].remove_notify == listener.vlanUpdates);
        listener.clear();
    }
}

//BOOST_FIXTURE_TEST_CASE( fssource, FSLearningBridgeFixture ) {
//
//    // check already existing
//    fs::path path1(temp / "83f18f0b-80f7-46e2-b06c-4d9487b0c754.ep");
//    fs::ofstream os(path1);
//    os << "{"
//       << "\"uuid\":\"83f18f0b-80f7-46e2-b06c-4d9487b0c754\","
//       << "\"mac\":\"10:ff:00:a3:01:00\","
//       << "\"ip\":[\"10.0.0.1\",\"10.0.0.2\"],"
//       << "\"interface-name\":\"veth0\","
//       << "\"endpoint-group\":\"/PolicyUniverse/PolicySpace/test/GbpEpGroup/epg/\","
//       << "\"attributes\":{\"attr1\":\"value1\"}"
//       << "}" << std::endl;
//    os.close();
//
//    FSWatcher watcher;
//    FSLearningBridgeSource source(&agent.getLearningBridgeManager(), watcher,
//                             temp.string());
//    watcher.start();
//
//    URI l2epr = URIBuilder()
//        .addElement("EprL2Universe")
//        .addElement("EprL2Ep")
//        .addElement(bduri.toString())
//        .addElement(MAC("10:ff:00:a3:01:00")).build();
//    URI l3epr_1 = URIBuilder()
//        .addElement("EprL3Universe")
//        .addElement("EprL3Ep")
//        .addElement(rduri.toString())
//        .addElement("10.0.0.1").build();
//    URI l3epr_2 = URIBuilder()
//        .addElement("EprL3Universe")
//        .addElement("EprL3Ep")
//        .addElement(rduri.toString())
//        .addElement("10.0.0.2").build();
//
//    WAIT_FOR(hasEPREntry<L3Ep>(framework, l3epr_1), 500);
//    WAIT_FOR(hasEPREntry<L3Ep>(framework, l3epr_2), 500);
//    WAIT_FOR(hasEPREntry<L2Ep>(framework, l2epr), 500);
//
//    URI l2epr2 = URIBuilder()
//        .addElement("EprL2Universe")
//        .addElement("EprL2Ep")
//        .addElement(bduri.toString())
//        .addElement(MAC("10:ff:00:a3:01:01")).build();
//
//    // check for a new EP added to watch directory
//    fs::path path2(temp / "83f18f0b-80f7-46e2-b06c-4d9487b0c755.ep");
//    fs::ofstream os2(path2);
//    os2 << "{"
//       << "\"uuid\":\"83f18f0b-80f7-46e2-b06c-4d9487b0c755\","
//       << "\"mac\":\"10:ff:00:a3:01:01\","
//       << "\"ip\":[\"10.0.0.3\"],"
//       << "\"interface-name\":\"veth1\","
//       << "\"endpoint-group\":\"/PolicyUniverse/PolicySpace/test/GbpEpGroup/epg/\","
//       << "\"attributes\":{\"attr2\":\"value2\"}"
//       << "}" << std::endl;
//    os2.close();
//
//    WAIT_FOR(hasEPREntry<L2Ep>(framework, l2epr2), 500);
//
//    // check for removing an endpoint
//    fs::remove(path2);
//
//    WAIT_FOR(!hasEPREntry<L2Ep>(framework, l2epr2), 500);
//
//    // check for overwriting existing file with new ep
//    fs::ofstream os3(path1);
//    std::string uuid3("83f18f0b-80f7-46e2-b06c-4d9487b0c756");
//    os3 << "{"
//        << "\"uuid\":\"" << uuid3 << "\","
//        << "\"mac\":\"10:ff:00:a3:01:02\","
//        << "\"ip\":[\"10.0.0.4\"],"
//        << "\"interface-name\":\"veth0\","
//        << "\"endpoint-group\":\"/PolicyUniverse/PolicySpace/test/GbpEpGroup/epg/\","
//        << "\"attributes\":{\"attr1\":\"value1\"}"
//        << "}" << std::endl;
//    os3.close();
//
//    WAIT_FOR(!hasEPREntry<L3Ep>(framework, l3epr_1), 500);
//    WAIT_FOR(!hasEPREntry<L3Ep>(framework, l3epr_2), 500);
//    WAIT_FOR(!hasEPREntry<L2Ep>(framework, l2epr), 500);
//
//    URI l2epr3 = URIBuilder()
//        .addElement("EprL2Universe")
//        .addElement("EprL2Ep")
//        .addElement(bduri.toString())
//        .addElement(MAC("10:ff:00:a3:01:02")).build();
//
//    WAIT_FOR(hasEPREntry<L2Ep>(framework, l2epr3, uuid3), 500);
//
//    watcher.stop();
//}

//class MockEndpointListener : public EndpointListener {
//public:
//    virtual void endpointUpdated(const std::string& uuid) {};
//    virtual void remoteEndpointUpdated(const std::string& uuid) {
//        std::unique_lock<std::mutex> guard(mutex);
//        updates.insert(uuid);
//    };
//
//    size_t numUpdates() {
//        std::unique_lock<std::mutex> guard(mutex);
//        return updates.size();
//    }
//    void clear() {
//        std::unique_lock<std::mutex> guard(mutex);
//        updates.clear();
//    }
//
//    std::mutex mutex;
//    std::unordered_set<std::string> updates;
//};

BOOST_AUTO_TEST_SUITE_END()

} /* namespace opflexagent */
