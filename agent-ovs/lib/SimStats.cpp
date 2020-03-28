/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for SimStats class
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/SimStats.h>
#include <modelgbp/gbpe/SecGrpClassifierCounter.hpp>
#include <modelgbp/gbpe/L24ClassifierCounter.hpp>



namespace opflexagent {

using namespace modelgbp::gbpe;

SimStats::SimStats(opflexagent::Agent& agent_)
    : agent(agent_), timer_interval(0), intCounter(0), contractCounter(0),
      secGrpCounter(0), stopping(false)  {}

SimStats::~SimStats() {}

void SimStats::updateInterfaceCounters() {
    if (stopping) return;

    auto& epMgr = agent.getEndpointManager();
    std::unordered_set<std::string> eps;
    epMgr.getEndpointUUIDs(eps);
    for (auto& uuid : eps) {
        opflexagent::EndpointManager::EpCounters counters;
        memset(&counters, 0, sizeof(counters));
        uint64_t c = ++intCounter;
        counters.txPackets = c;
        counters.rxPackets = c;
        counters.txBytes = c * 1500;
        counters.rxBytes = c * 1500;

        epMgr.updateEndpointCounters(uuid, counters);
    }
}

void SimStats::updateContractCounters() {

    auto& polMgr = agent.getPolicyManager();
    auto pu = modelgbp::policy::Universe::resolve(agent.getFramework());
    auto su =
        modelgbp::observer::PolicyStatUniverse::resolve(agent.getFramework());

    std::unique_lock<std::mutex> guard(mutex);
    opflex::modb::Mutator mutator(agent.getFramework(), "policyelement");
    for (auto& con : contracts) {
        opflexagent::PolicyManager::uri_set_t consumers;
        opflexagent::PolicyManager::uri_set_t providers;
        opflexagent::PolicyManager::uri_set_t intras;
        opflexagent::PolicyManager::rule_list_t rules;

        polMgr.getContractConsumers(con, consumers);
        polMgr.getContractProviders(con, providers);
        polMgr.getContractIntra(con, intras);
        polMgr.getContractRules(con, rules);
        uint64_t c = ++contractCounter;

        for (auto& rule : rules) {
            auto& l24Classifier =
                rule->getL24Classifier()->getURI().toString();

            for (auto& consumer : consumers) {
                for (auto& provider : providers) {
                    L24ClassifierCounter::remove(agent.getFramework(), agent.getUuid(), (c - 1), consumer.toString(),
                                                 provider.toString(),l24Classifier);
                    su.get()->addGbpeL24ClassifierCounter(agent.getUuid(), c,
                                                    consumer.toString(),
                                                    provider.toString(),
                                                    l24Classifier)
                        ->setPackets(c)
                        .setBytes(c * 1500);
                }
            }

            for (auto& intra : intras) {
                L24ClassifierCounter::remove(agent.getFramework(), agent.getUuid(), (c - 1), intra.toString(),
                                                 intra.toString(),l24Classifier);
                su.get()->
                    addGbpeL24ClassifierCounter(agent.getUuid(), c,
                                                intra.toString(),
                                                intra.toString(),
                                                l24Classifier)
                    ->setPackets(c)
                    .setBytes(c * 1500);
            }
        }
    }
    mutator.commit();
}

void SimStats::updateSecGrpCounters() {

    auto& polMgr = agent.getPolicyManager();
    auto su = modelgbp::observer::PolicyStatUniverse::resolve(agent.getFramework());
    std::unique_lock<std::mutex> guard(mutex);
    opflex::modb::Mutator mutator(agent.getFramework(), "policyelement");

    opflexagent::PolicyManager::rule_list_t rules;


    uint64_t c = ++secGrpCounter;
    for (auto& sec : secGroups) {
        polMgr.getSecGroupRules(sec, rules);
        for (auto& rule : rules) {
            auto& l24Classifier =
                rule->getL24Classifier()->getURI().toString();

            SecGrpClassifierCounter::remove(agent.getFramework(), agent.getUuid(), (c - 1),
                                    l24Classifier);
            su.get()->addGbpeSecGrpClassifierCounter(agent.getUuid(), c,
                  l24Classifier)
                  ->setTxpackets(c)
                  .setTxbytes(c * 1500)
                  .setRxpackets(c)
                  .setRxbytes(c * 1500);
        }
    }
    mutator.commit();

}

void SimStats::on_timer_contract(const boost::system::error_code& ec) {
    std::function<void(const boost::system::error_code&)> fn = [this](const boost::system::error_code& ec) {
                           this->on_timer_contract(ec);
    };
    if (!on_timer_check(contract_timer, ec)) {
        return;
    }
    io.post([this]() { updateContractCounters(); });
    timer_restart(agent.getContractInterval(), fn, contract_timer);
}

void SimStats::on_timer_security_group(const boost::system::error_code& ec) {
    std::function<void(const boost::system::error_code&)> fn = [this](const boost::system::error_code& ec) {
                           this->on_timer_security_group(ec);
    };
    if (!on_timer_check(security_group_timer, ec)) {
        return;
    }
    io.post([this]() { updateSecGrpCounters(); });
    timer_restart(agent.getSecurityGroupInterval(), fn, security_group_timer);
}

void SimStats::on_timer_interface(const boost::system::error_code& ec) {
    std::function<void(const boost::system::error_code&)> fn = [this](const boost::system::error_code& ec) {
                           this->on_timer_interface(ec);
    };
    if (!on_timer_check(interface_timer, ec)) {
        return;
    }
    io.post([this]() { updateInterfaceCounters(); });
    timer_restart(agent.getInterfaceInterval(), fn, interface_timer);
}

void SimStats::start() {

    LOG(INFO) << "Starting stats simulator";

    if (agent.getContractInterval() != 0) {
        contract_timer.reset(new deadline_timer(io, milliseconds(agent.getContractInterval())));
        contract_timer->async_wait([this](const boost::system::error_code& ec) {
                                          on_timer_contract(ec);
        });
    }

    if (agent.getSecurityGroupInterval() != 0) {
        security_group_timer.reset(new deadline_timer(io, milliseconds(agent.getSecurityGroupInterval())));
        security_group_timer->async_wait([this](const boost::system::error_code& ec) {
                                                on_timer_security_group(ec);
        });
    }

    if (agent.getInterfaceInterval() != 0) {
        interface_timer.reset(new deadline_timer(io, milliseconds(agent.getInterfaceInterval())));
        interface_timer->async_wait([this](const boost::system::error_code& ec) {
                                           on_timer_interface(ec);
        });
    }

    io_service_thread.reset(new std::thread([this]() { io.run(); }));
}

void SimStats::stop() {
    stopping = true;

    if (contract_timer) {
        contract_timer->cancel();
    }
    if (security_group_timer) {
        security_group_timer->cancel();
    }
    if (interface_timer) {
        interface_timer->cancel();
    }
    if (io_service_thread) {
        io_service_thread->join();
        io_service_thread.reset();
    }
}

void SimStats::contractUpdated(const opflex::modb::URI& uri) {
    std::unique_lock<std::mutex> guard(mutex);
    contracts.insert(uri);
}

void SimStats::secGroupUpdated(const opflex::modb::URI& uri) {
    std::unique_lock<std::mutex> guard(mutex);
    secGroups.insert(uri);
}

inline bool SimStats::on_timer_check(std::shared_ptr<boost::asio::deadline_timer> timer,
                           const boost::system::error_code& ec) {
    if (ec) {
        // shut down the timer when we get a cancellation
        timer.reset();
        return false;
    }
    return true;
}
inline void SimStats::timer_restart(uint32_t interval,
                      std::function<void(const boost::system::error_code& ec)> on_timer,
                      std::shared_ptr<boost::asio::deadline_timer> timer) {
    if (!this->stopping) {
        timer->expires_at(timer->expires_at() +
                          milliseconds(interval));
        timer->async_wait(on_timer);
    }
}

} /* namespace opflexagent */
