/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string.h>
#include <iostream>
#include <iomanip>
#include <stdlib.h>
#include <signal.h>
#include <netinet/in.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>


#include <utility>
#include <functional>
#include "logging.h"

//Debugging
#include <unistd.h>
// End debugging includes

#include "MockVppApi.h"
#include "MockVppConnection.h"

using namespace std;

namespace ovsagent {

  MockVppApi::MockVppApi() {
        mockVppConn = new MockVppConnection();
  }

  MockVppApi::~MockVppApi() {}

  MockVppConnection* MockVppApi::getVppConnection()
  {
    return mockVppConn;
  }


} // namespace ovsagent
