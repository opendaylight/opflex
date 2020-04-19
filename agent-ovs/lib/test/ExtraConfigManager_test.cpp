/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for extra config manager
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <boost/filesystem/fstream.hpp>

#include <opflexagent/FSRDConfigSource.h>
#include <opflexagent/logging.h>

#include <opflexagent/test/BaseFixture.h>

namespace opflexagent {

namespace fs = boost::filesystem;

class FSRDConfigFixture : public BaseFixture {
public:
    FSRDConfigFixture() : BaseFixture(),
        temp(fs::temp_directory_path() / fs::unique_path()) {
        fs::create_directory(temp);
    }

    ~FSRDConfigFixture() {
        fs::remove_all(temp);
    }

    fs::path temp;
};

BOOST_AUTO_TEST_SUITE(ExtraConfigManager_test)

BOOST_FIXTURE_TEST_CASE( rdconfigsource, FSRDConfigFixture ) {

    // check for a new RDConfig added to watch directory
    fs::path path1(temp / "abc.rdconfig");

    fs::ofstream os(path1);
    os << "{"
       << "\"uuid\":\"83f18f0b-80f7-46e2-b06c-4d9487b0c793\","
       << "\"domain-name\":\"rd1\","
       << "\"domain-policy-space\":\"space1\","
       << "\"internal-subnets\" : [\"1.2.3.4\", \"5.6.7.8\"]"
       << "}" << std::endl;
    os.close();
    FSWatcher watcher;
    FSRDConfigSource source(&agent.getExtraConfigManager(), watcher,
                             temp.string());
    watcher.start();
    URI rdUri("/PolicyUniverse/PolicySpace/space1/GbpRoutingDomain/rd1/");
    WAIT_FOR((agent.getExtraConfigManager().getRDConfig(rdUri) != nullptr), 500);

    // check for removing an RDConfig
    fs::remove(path1);

    WAIT_FOR((agent.getExtraConfigManager().getRDConfig(rdUri) == nullptr), 500);
}

BOOST_AUTO_TEST_SUITE_END()
}