/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for SimStats
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_SIMSTATS_H
#define OPFLEXAGENT_SIMSTATS_H

#include <opflexagent/Agent.h>
#include <opflexagent/logging.h>
#include <opflexagent/cmd.h>
#include <boost/asio.hpp>

using boost::asio::deadline_timer;
using boost::posix_time::milliseconds;



namespace opflexagent {

/**
 * Main class for simulating counter statistics for artifacts like interfaces, contracts
 * and security groups.
 * sub classing of PolicyListener is required to learn about new contracts and
 * security groups being added.
 */

class SimStats : public opflexagent::PolicyListener {

public:
    /**
     * Create an instance of SimStats
     * @param[in] agent_ a reference to an instance of opflex agent.
     */
    SimStats(opflexagent::Agent& agent_);
    /**
     * Destroy this instance of SimStats
     */
    ~SimStats();
    /**
     * update all the interface counters known to this instance of opflex agent.
     */
    void updateInterfaceCounters();
    /**
     * update all the contract counters known to this instance of opflex agent.
     */
    void updateContractCounters();
    /**
     * update all the security group counters known to this instance of opflex agent.
     */
    void updateSecGrpCounters();
    /**
     * called when contract timer expires. It invokes the update method for contract counters.
     * @param[in] ec error code.
     */
    void on_timer_contract(const boost::system::error_code& ec);
    /**
     * called when security group timer expires. It invokes the update method for security group counters.
     * @param[in] ec error code.
     */
    void on_timer_security_group(const boost::system::error_code& ec);
    /**
     * called when interface timer expires. It invokes the update method for interface counters.
     * @param[in] ec error code.
     */
    void on_timer_interface(const boost::system::error_code& ec);
    /**
     * start the statistics simulator.
     * A new thread is started using ASIO.
     */
    void start();
    /**
     * stop the statistics simulator.
     * stop all threads associated with this instance.
     */
    void stop();
    /**
     * call back for informing opflex agent about changes to contracts.
     */
    virtual void contractUpdated(const opflex::modb::URI& uri);
    /**
     * call back for informing opflex agent about changes to security groups.
     */
    virtual void secGroupUpdated(const opflex::modb::URI& uri);
private:
    opflexagent::Agent& agent;
    uint32_t timer_interval;
    std::mutex mutex;
    std::unordered_set<opflex::modb::URI> contracts;
    std::unordered_set<opflex::modb::URI> secGroups;
    std::atomic<uint64_t> intCounter;
    std::atomic<uint64_t> contractCounter;
    std::atomic<uint64_t> contractGenIdCounter;
    std::atomic<uint64_t> secGrpCounter;
    std::atomic<uint64_t> secGrpGenIdCounter;
    boost::asio::io_service io;
    std::atomic_bool stopping;
    std::shared_ptr<std::thread> io_service_thread;
    std::shared_ptr<boost::asio::deadline_timer> contract_timer;
    std::shared_ptr<boost::asio::deadline_timer> security_group_timer;
    std::shared_ptr<boost::asio::deadline_timer> interface_timer;

    bool on_timer_check(std::shared_ptr<boost::asio::deadline_timer> timer,
                        const boost::system::error_code& ec);
    void timer_restart(uint32_t interval,
                       std::function<void(const boost::system::error_code& ec)> on_timer,
                       std::shared_ptr<boost::asio::deadline_timer> timer);
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_SIMSTATS_H */
