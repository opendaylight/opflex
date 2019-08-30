/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for base fixture
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflex/ofcore/OFFramework.h>
#include <opflex/ofcore/OFConstants.h>
#include <modelgbp/metadata/metadata.hpp>

#include <opflexagent/Agent.h>
#include <opflexagent/TunnelEpManager.h>

#include <boost/filesystem/fstream.hpp>
#include <boost/test/unit_test.hpp>

#pragma once
#ifndef OPFLEXAGENT_TEST_BASEFIXTURE_H
#define OPFLEXAGENT_TEST_BASEFIXTURE_H

namespace opflexagent {

/**
 * A fixture that adds an object store
 */
class BaseFixture {
typedef opflex::ofcore::OFConstants::OpflexElementMode opflex_elem_t;
public:
    BaseFixture(opflex_elem_t mode = opflex_elem_t::INVALID_MODE) :
    agent(framework), tunnelEpManager(&agent) {
        agent.setRendererForwardingMode(mode);
        if(mode == opflex_elem_t::TRANSPORT_MODE) {
            /**
             * Set test values for transport mode
             **/
            boost::system::error_code ec;
            boost::asio::ip::address_v4 proxyAddress;
            framework.setElementMode(mode);
            proxyAddress = boost::asio::ip::address_v4::from_string("44.44.44.44",ec);
            framework.setV4Proxy(proxyAddress);
            proxyAddress = boost::asio::ip::address_v4::from_string("66.66.66.66",ec);
            framework.setV6Proxy(proxyAddress);
            proxyAddress = boost::asio::ip::address_v4::from_string("55.55.55.55",ec);
            framework.setMacProxy(proxyAddress);
        }
        agent.start();
    }

    virtual ~BaseFixture() {
        agent.stop();
    }

    /**
     * A framework object
     */
    opflex::ofcore::MockOFFramework framework;
    /**
     * An agent instance
     */
    Agent agent;
    TunnelEpManager tunnelEpManager;
};

/**
 * A simple guard for creating temporary files for testing
 */
class TempGuard {
public:
    TempGuard() :
        temp_dir(boost::filesystem::temp_directory_path() /
                 boost::filesystem::unique_path()) {
        boost::filesystem::create_directory(temp_dir);
    }

    ~TempGuard() {
        boost::filesystem::remove_all(temp_dir);
    }

    /**
     * the temporary path
     */
    boost::filesystem::path temp_dir;
};

// wait for a condition to become true because of an event in another
// thread. Executes 'stmt' after each wait iteration.
#define WAIT_FOR_DO_ONFAIL(condition, count, stmt, onfail) \
    {                                                      \
        int _c = 0;                                        \
        while (_c < count) {                               \
            if (condition) break;                          \
            _c += 1;                                       \
            usleep(1000);                                  \
            stmt;                                          \
        }                                                  \
        BOOST_CHECK((condition));                          \
        if (!(condition))                                  \
            {onfail;}                                      \
    }

// wait for a condition to become true because of an event in another
// thread. Executes 'stmt' after each wait iteration.
#define WAIT_FOR_DO(condition, count, stmt) \
    WAIT_FOR_DO_ONFAIL(condition, count, stmt, ;)

// wait for a condition to become true because of an event in another
// thread
#define WAIT_FOR(condition, count)  WAIT_FOR_DO(condition, count, ;)

// wait for a condition to become true because of an event in another
// thread. Executes onfail on failure
#define WAIT_FOR_ONFAIL(condition, count, onfail)       \
    WAIT_FOR_DO_ONFAIL(condition, count, ;, onfail)

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_TEST_BASEFIXTURE_H */
