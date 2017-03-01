/*
 * Test suite for class StatsManager.
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


#include "logging.h"
#include "ModbFixture.h"
#include "StatsManager.h"
#include "MockPortMapper.h"


namespace ovsagent {


class StatsManagerFixture : public ModbFixture {

public:
      StatsManagerFixture() : statsManager(&agent, intPortMapper, accessPortMapper, 10){
         
      }

   virtual ~StatsManagerFixture() {}

private:
   MockPortMapper intPortMapper;
   MockPortMapper accessPortMapper;
   StatsManager statsManager;
    
};

BOOST_AUTO_TEST_SUITE(StatsManager_test)

BOOST_FIXTURE_TEST_CASE(intOnly,StatsManagerFixture) {

}

BOOST_AUTO_TEST_SUITE_END()

}
