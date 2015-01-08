/*
 * Fixture to setup connectivity to an OVS switch
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#ifndef INTEGRATION_TEST_CONNECTIONFIXTURE_H_
#define INTEGRATION_TEST_CONNECTIONFIXTURE_H_

#include <string>

class ConnectionFixture {
public:
    ConnectionFixture() : testSwitchName("myTestBridge") {
        AddSwitch(testSwitchName);
    }

    ~ConnectionFixture() {
        RemoveSwitch(testSwitchName);
    }

    int AddSwitch(const std::string& swName) {
        std::string comm("ovs-vsctl --may-exist add-br " + swName);
        return system(comm.c_str());
    }

    int RemoveSwitch(const std::string& swName) {
        std::string comm("ovs-vsctl --if-exists del-br " + swName);
        return system(comm.c_str());
    }

    int DeleteAllFlows(const std::string& swName) {
        std::string comm("ovs-ofctl del-flows "  + swName);
        return system(comm.c_str());
    }

    std::string testSwitchName;
};

#define WAIT_FOR(condition, count)                             \
    {                                                          \
        int _c = 0;                                            \
        while (_c < count && !(condition)) {                   \
           ++_c;                                               \
           sleep(1);                                           \
        }                                                      \
        BOOST_CHECK((condition));                              \
    }

#endif /* INTEGRATION_TEST_CONNECTIONFIXTURE_H_ */
