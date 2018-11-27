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



namespace opflexagent {



SimStats::SimStats(opflexagent::Agent& agent_, uint32_t timer_interval_)
    : agent(agent_), timer_interval(timer_interval_), intCounter(0), contractCounter(0),
      stopping(false) {}

SimStats::~SimStats() {}

void SimStats::updateInterfaceCounters(opflexagent::Agent& a) {
    if (stopping) return;

    auto& epMgr = a.getEndpointManager();
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

void SimStats::updateContractCounters(opflexagent::Agent& a) {

    auto& polMgr = a.getPolicyManager();
    auto pu = modelgbp::policy::Universe::resolve(a.getFramework());
    auto su =
        modelgbp::observer::PolicyStatUniverse::resolve(a.getFramework());

    std::unique_lock<std::mutex> guard(mutex);
    opflex::modb::Mutator mutator(a.getFramework(), "policyelement");
    for (auto& con : contracts) {
        opflexagent::PolicyManager::uri_set_t consumers;
        opflexagent::PolicyManager::uri_set_t providers;
        opflexagent::PolicyManager::uri_set_t intras;
        opflexagent::PolicyManager::rule_list_t rules;

        polMgr.getContractConsumers(con, consumers);
        polMgr.getContractProviders(con, providers);
        polMgr.getContractIntra(con, intras);
        polMgr.getContractRules(con, rules);
        uint64_t genId = ++contractGenIdCounter;

        for (auto& rule : rules) {
            auto& l24Classifier =
                rule->getL24Classifier()->getURI().toString();

            for (auto& consumer : consumers) {
                for (auto& provider : providers) {
                    uint64_t c = ++contractCounter;
                    su.get()->
                        addGbpeL24ClassifierCounter("uuid", genId,
                                                    consumer.toString(),
                                                    provider.toString(),
                                                    l24Classifier)
                        ->setPackets(c)
                        .setBytes(c * 1500);
                }
            }

            for (auto& intra : intras) {
                uint64_t c = ++contractCounter;
                su.get()->
                    addGbpeL24ClassifierCounter("uuid", genId,
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

void SimStats::updateSecGrpCounters(opflexagent::Agent& a) {

    auto& polMgr = a.getPolicyManager();
    auto su = modelgbp::observer::PolicyStatUniverse::resolve(a.getFramework());
    std::unique_lock<std::mutex> guard(mutex);
    opflex::modb::Mutator mutator(a.getFramework(), "policyelement");

    opflexagent::PolicyManager::rule_list_t rules;


    uint64_t counter = ++secGrpGenIdCounter;
    for (auto& sec : secGroups) {
        polMgr.getSecGroupRules(sec, rules);
        for (auto& rule : rules) {
            auto& l24Classifier =
                rule->getL24Classifier()->getURI().toString();

            su.get()->addGbpeSecGrpClassifierCounter("uuid", counter,
                  l24Classifier)
                  ->setTxpackets(counter)
                  .setTxbytes(counter * 1500)
                  .setRxpackets(counter)
                  .setRxbytes(counter * 1500);
        }
    }
    mutator.commit();

}

void SimStats::on_timer(const boost::system::error_code& ec) {
    if (ec) {
        // shut down the timer when we get a cancellation
        timer.reset();
        return;
    }


    auto& a = agent;
    io.post([this, &a]() { updateInterfaceCounters(a); });
    io.post([this, &a]() { updateContractCounters(a); });
    io.post([this, &a]() { updateSecGrpCounters(a); });


    if (!stopping) {
        timer->expires_at(timer->expires_at() +
                          milliseconds(timer_interval));
        timer->async_wait([this](const boost::system::error_code& ec) {
                on_timer(ec);
            });
    }
}

void SimStats::start() {
    if (timer_interval <= 0) return;

    LOG(INFO) << "Starting stats simulator";

    timer.reset(new deadline_timer(io, milliseconds(timer_interval)));
    timer->async_wait([this](const boost::system::error_code& ec) {
            on_timer(ec);
        });

    io_service_thread.reset(new std::thread([this]() { io.run(); }));
}

void SimStats::stop() {
    stopping = true;

    if (timer) {
        timer->cancel();
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


} /* namespace opflexagent */
