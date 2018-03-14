/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Stress test tool for testing OpFlex policy repository
 *
 * Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/Agent.h>
#include <opflexagent/logging.h>
#include <opflexagent/cmd.h>

#include <boost/program_options.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>

#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <random>
#include <atomic>

#include <signal.h>

using std::string;
using boost::uuids::to_string;
using boost::uuids::basic_random_generator;
using boost::asio::deadline_timer;
using boost::posix_time::milliseconds;
using opflexagent::INFO;
using opflexagent::ERROR;
namespace po = boost::program_options;

std::random_device rng;
std::mt19937 urng(rng());
basic_random_generator<std::mt19937> uuidGen(urng);

class TestAgent {
public:
    TestAgent(std::string domain, std::string identity) :
        framework(new opflex::ofcore::OFFramework()),
        agent(new opflexagent::Agent(*framework)) {
        framework->setOpflexIdentity(identity, domain);
        agent->getPolicyManager().setOpflexDomain(domain);
    }

    std::unique_ptr<opflex::ofcore::OFFramework> framework;
    std::unique_ptr<opflexagent::Agent> agent;
};

opflexagent::Endpoint createEndpoint(const uint32_t epId,
                                     const uint32_t groupId,
                                     const std::string& epgTemplate) {
    opflexagent::Endpoint ep(to_string(uuidGen()));

    ep.setInterfaceName("interface-name");

    uint8_t mac[6];
    mac[0] = 0xfe;
    mac[1] = 0xff;
    memcpy(mac+2, &epId, 4);
    ep.setMAC(opflex::modb::MAC(mac));

    uint32_t ip = 0xc0a80000 + epId;
    boost::asio::ip::address_v4 ipAddr(ip);
    ep.addIP(ipAddr.to_string());

    std::string uri =
        boost::replace_all_copy(epgTemplate, "<group_id>",
                                boost::lexical_cast<std::string>(groupId));
    ep.setEgURI(opflex::modb::URI(uri));

    return ep;
}

class StatsSimulator {
public:
    StatsSimulator(std::vector<TestAgent>& agents_,
                   uint32_t timer_interval_)
        : agents(agents_), timer_interval(timer_interval_) {}

    void updateCounters(opflexagent::Agent& a) {
        if (stopping) return;

        auto& epMgr = a.getEndpointManager();
        std::unordered_set<std::string> eps;
        epMgr.getEndpointsByIface("interface-name", eps);
        for (auto& uuid : eps) {
            opflexagent::EndpointManager::EpCounters counters;
            memset(&counters, 0, sizeof(counters));
            uint64_t c = ++counter;
            counters.txPackets = c;
            counters.rxPackets = c;
            counters.txBytes = c * 1500;
            counters.rxBytes = c * 1500;

            epMgr.updateEndpointCounters(uuid, counters);
        }
    }

    void await() {
    }

    void start() {
        if (timer_interval <= 0) return;

        LOG(INFO) << "Starting stats simulator";

        timer.reset(new deadline_timer(io, milliseconds(timer_interval)));
        timer->async_wait([this](const boost::system::error_code& ec) {
                on_timer(ec);
            });

        io_service_thread.reset(new std::thread([this]() { io.run(); }));
    }

    void stop() {
        stopping = true;

        if (timer) {
            timer->cancel();
        }
        if (io_service_thread) {
            io_service_thread->join();
            io_service_thread.reset();
        }
    }

    void on_timer(const boost::system::error_code& ec) {
        if (ec) {
            // shut down the timer when we get a cancellation
            timer.reset();
            return;
        }

        LOG(INFO) << "Updating interface counters " << counter;
        for (auto& a : agents) {
            io.post([this, &a]() { updateCounters(*a.agent); });
        }

        if (!stopping) {
            timer->expires_at(timer->expires_at() +
                              milliseconds(timer_interval));
            timer->async_wait([this](const boost::system::error_code& ec) {
                    on_timer(ec);
                });
        }
    }

    std::vector<TestAgent>& agents;
    std::atomic<uint64_t> counter;

    boost::asio::io_service io;
    std::atomic_bool stopping;
    std::unique_ptr<std::thread> io_service_thread;
    std::unique_ptr<boost::asio::deadline_timer> timer;

    uint32_t timer_interval;
};

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);

    // Parse command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Print this help message")
        ("level", po::value<string>()->default_value("warning"),
         "Use the specified log level (default warning).")
        ("no_ssl", "Disable SSL in OpFlex connection")
        ("host", po::value<string>()->default_value("10.0.0.30"),
         "OpFlex peer host name")
        ("port", po::value<int>()->default_value(8009),
         "OpFlex peer port")
        ("domain", po::value<string>()->
         default_value("comp/prov-OpenStack/"
                       "ctrlr-[stress]-stress/sw-InsiemeLSOid"),
         "OpFlex domain")
        ("agents,a", po::value<uint32_t>()->default_value(128),
         "Number of agents to create")
        ("endpoints,e", po::value<uint32_t>()->default_value(50),
         "Number of endpoints per agent to create")
        ("epgroups,g", po::value<uint32_t>()->default_value(50),
         "Number of EP groups to use for endpoints")
        ("stats_interval,s", po::value<uint32_t>()->default_value(10000),
         "Interval in milliseconds between updating stats. 0 to disable.")
        ("identity_template", po::value<string>()->
         default_value("stress_agent_<agent_id>"),
         "Template for OpFlex agent identities")
        ("epg_template", po::value<string>()->
         default_value("/PolicyUniverse/PolicySpace/common/GbpEpGroup/"
                       "stress%7cstress_<group_id>/"),
         "Template for EPG URIs for endpoints")
        ;

    std::string level_str;
    uint32_t num_agents;
    uint32_t num_endpoints;
    uint32_t num_epgs;
    uint32_t stats_interval;
    std::string epg_template;
    std::string identity_template;
    std::string domain;
    std::string host;
    int port;
    bool use_ssl = true;

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).
                  options(desc).run(), vm);
        po::notify(vm);
        if (vm.count("help")) {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << desc;
            return 0;
        }
        level_str = vm["level"].as<string>();
        num_agents = vm["agents"].as<uint32_t>();
        num_endpoints = vm["endpoints"].as<uint32_t>();
        num_epgs = vm["epgroups"].as<uint32_t>();
        stats_interval = vm["stats_interval"].as<uint32_t>();
        epg_template = vm["epg_template"].as<string>();
        identity_template = vm["identity_template"].as<string>();
        domain = vm["domain"].as<string>();
        host = vm["host"].as<string>();
        port = vm["port"].as<int>();
        if (vm.count("no_ssl")) {
            use_ssl = false;
        }
    } catch (po::unknown_option e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    LOG(INFO) << "Starting " << num_agents << " agents with domain "
              << domain;

    std::vector<TestAgent> agents;
    for (uint32_t i; i < num_agents; i++) {
        std::string identity =
            boost::replace_all_copy(identity_template, "<agent_id>",
                                    boost::lexical_cast<std::string>(i));
        LOG(INFO) << "Creating agent " << identity;
        agents.emplace_back(domain, identity);
    }

    if (agents.size() == 0) {
        LOG(opflexagent::ERROR) << "No agents created";
        return 2;
    }

    opflexagent::initLogging(level_str, false, "");

    sigset_t waitset;
    sigemptyset(&waitset);
    sigaddset(&waitset, SIGINT);
    sigaddset(&waitset, SIGTERM);
    sigprocmask(SIG_BLOCK, &waitset, NULL);

    std::thread signal_thread([&agents, &waitset]() {
            int sig;
            int result = sigwait(&waitset, &sig);
            if (result == 0) {
                LOG(INFO) << "Got " << strsignal(sig) << " signal";
            } else {
                LOG(ERROR) << "Failed to wait for signals: " << errno;
            }
        });

    for (size_t i; i < agents.size(); i++) {
        LOG(INFO) << "Starting agent " << (i+1);
        TestAgent& a = agents.at(i);
        a.agent->start();
        if (use_ssl) {
            a.agent->getFramework().enableSSL("/etc/ssl/certs/", false);
        }
        a.agent->getFramework().addPeer(host, port);
    }

    StatsSimulator stats(agents, stats_interval);
    stats.start();

    uint32_t epId = 0;
    uint32_t groupId = 0;
    for (uint32_t i = 0; i < num_agents; i++) {
        opflexagent::EndpointSource source(&agents[i].agent->
                                           getEndpointManager());

        for (uint32_t j = 0; j < num_endpoints; j++) {
            auto ep = createEndpoint(epId, groupId, epg_template);
            LOG(INFO) << "Creating " << ep << " on agent " << (i+1);
            source.updateEndpoint(ep);

            epId += 1;
        }

        groupId = (groupId + 1) % num_epgs;
    }

    signal_thread.join();

    stats.stop();
    for (TestAgent& a : agents) {
        a.agent->stop();
    }

    return 0;
}
