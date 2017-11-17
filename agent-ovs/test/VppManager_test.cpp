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
#include "BaseFixture.h"
#include "VppManager.h"

#include "ModbFixture.h"
#include "RangeMask.h"

#define VPP_TESTS

using namespace boost::assign;
using namespace opflex::modb;
using namespace modelgbp::gbp;
using namespace ovsagent;
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
    {}

    virtual ~VppManagerFixture() {
    }

    IdGenerator idGen;
};

BOOST_FIXTURE_TEST_CASE(start, VppManagerFixture) {
}

BOOST_AUTO_TEST_SUITE_END()
