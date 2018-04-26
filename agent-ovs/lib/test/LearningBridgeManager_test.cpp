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

typedef std::set<LearningBridgeIface::vlan_range_t> range_set_t;

class MockLBListener : public LearningBridgeListener {
public:
    virtual void lbIfaceUpdated(const std::string& uuid) {
        std::unique_lock<std::mutex> guard(mutex);
        ifaceUpdates.insert(uuid);
    }

    virtual void lbVlanUpdated(LearningBridgeIface::vlan_range_t vlan) {
        std::unique_lock<std::mutex> guard(mutex);
        vlanUpdates.insert(vlan);
    };

    size_t numIfaceUpdates() {
        std::unique_lock<std::mutex> guard(mutex);
        return ifaceUpdates.size();
    }
    void clear() {
        std::unique_lock<std::mutex> guard(mutex);
        vlanUpdates.clear();
        ifaceUpdates.clear();
    }
    void logUpdates() {
        std::unique_lock<std::mutex> guard(mutex);
        std::stringstream ss;
        for (auto& r: vlanUpdates) {
            ss << r << " ";
        };
        LOG(DEBUG) << "Updates: Vlan " << ss.str()
                   << "Iface " << boost::join(ifaceUpdates, ",");
    }
    range_set_t getVlanUpdates() {
        std::unique_lock<std::mutex> guard(mutex);
        return vlanUpdates;
    }
    std::unordered_set<std::string> getIfaceUpdates() {
        std::unique_lock<std::mutex> guard(mutex);
        return ifaceUpdates;
    }

    std::mutex mutex;
    range_set_t vlanUpdates;
    std::unordered_set<std::string> ifaceUpdates;
};

class LBFixture : public BaseFixture {
public:
    LBFixture()
        : BaseFixture(),
          lbSource(&agent.getLearningBridgeManager()) {
    }

    virtual ~LBFixture() {
    }

    LearningBridgeSource lbSource;
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
        LOG(INFO) << "Checking add " << i;
        LearningBridgeIface iface;
        iface.setUUID(boost::lexical_cast<std::string>(i));
        for (const auto& r : test_cases[i].input) {
            iface.addTrunkVlans(r);
        }
        lbSource.updateLBIface(iface);
        BOOST_CHECK(test_cases[i].expected == f());
        listener.logUpdates();
        BOOST_CHECK(test_cases[i].notify == listener.getVlanUpdates());
        listener.clear();
    }

    for (size_t i = 0; i < test_cases.size(); i++) {
        LOG(INFO) << "Checking remove " << i;
        lbSource.removeLBIface(boost::lexical_cast<std::string>(i));
        BOOST_CHECK(test_cases[i].remove_expected == f());
        listener.logUpdates();
        BOOST_CHECK(test_cases[i].remove_notify == listener.getVlanUpdates());
        listener.clear();
    }
}

BOOST_FIXTURE_TEST_CASE( fssource, FSLBFixture ) {
    // check already existing
    {
        fs::path path(temp / "1.lbiface");
        fs::ofstream os(path);
        os << "{"
           << "\"uuid\":\"1\","
           << "\"interface-name\":\"veth0\","
           << "\"trunk-vlans\": [{\"start\": 50, \"end\": 100}]"
           << "}" << std::endl;
        os.close();
    }

    MockLBListener listener;
    agent.getLearningBridgeManager().registerListener(&listener);

    FSWatcher watcher;
    FSLearningBridgeSource source(&agent.getLearningBridgeManager(), watcher,
                                  temp.string());
    watcher.start();

    typedef std::unordered_set<std::string> uuid_set_t;

    WAIT_FOR(listener.numIfaceUpdates() >= 1, 500);
    listener.logUpdates();
    BOOST_CHECK(listener.getIfaceUpdates() == uuid_set_t({"1"}));
    BOOST_CHECK(listener.getVlanUpdates() == range_set_t({{50,100}}));
    listener.clear();

    // add
    {
        fs::path path(temp / "2.lbiface");
        fs::ofstream os(path);
        os << "{"
           << "\"uuid\":\"2\","
           << "\"interface-name\":\"veth1\","
           << "\"trunk-vlans\": [{\"start\": 50, \"end\": 100}]"
           << "}" << std::endl;
        os.close();
    }

    WAIT_FOR(listener.numIfaceUpdates() >= 1, 500);
    listener.logUpdates();
    BOOST_CHECK(listener.getIfaceUpdates() == uuid_set_t({"2"}));
    BOOST_CHECK(listener.getVlanUpdates() == range_set_t({{50,100}}));
    listener.clear();

    // update
    {
        fs::path path(temp / "1.lbiface");
        fs::ofstream os(path);
        os << "{"
           << "\"uuid\":\"1\","
           << "\"interface-name\":\"veth0\","
           << "\"trunk-vlans\": [{\"start\": 200, \"end\": 300}]"
           << "}" << std::endl;
        os.close();
    }

    WAIT_FOR(listener.numIfaceUpdates() >= 1, 500);
    listener.logUpdates();
    BOOST_CHECK(listener.getIfaceUpdates() == uuid_set_t({"1"}));
    BOOST_CHECK(listener.getVlanUpdates() == range_set_t({{50,100},{200,300}}));
    listener.clear();

    watcher.stop();
    agent.getLearningBridgeManager().unregisterListener(&listener);
}

BOOST_AUTO_TEST_SUITE_END()

} /* namespace opflexagent */
