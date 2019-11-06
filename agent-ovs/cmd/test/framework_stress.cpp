/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Framework stress test standalone
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#include <unistd.h>
#include <csignal>

#include <string>
#include <vector>
#include <iostream>

#include <boost/program_options.hpp>

#include <modelgbp/dmtree/Root.hpp>
#include <modelgbp/metadata/metadata.hpp>
#include <opflex/test/MockOpflexServer.h>
#include <opflex/ofcore/OFFramework.h>
#include <opflex/ofcore/OFConstants.h>

#include <opflexagent/logging.h>

using std::string;
using std::make_pair;
using std::shared_ptr;
namespace po = boost::program_options;
using opflex::test::MockOpflexServer;
using opflex::ofcore::OFConstants;
using namespace opflexagent;

volatile bool running = true;
volatile int gotconfig = 0;

/**
 * Listener for changes related to plaform config.
 */
class ConfigListener : public opflex::modb::ObjectListener {
public:
    ConfigListener() {}
    virtual ~ConfigListener() {}

    virtual void objectUpdated(opflex::modb::class_id_t class_id,
                               const opflex::modb::URI& uri) {
        LOG(INFO);
        gotconfig += 1;
    }
};
ConfigListener configListener;

void sighandler(int sig) {
    running = false;
    LOG(INFO) << "Got " << strsignal(sig) << " signal";
}

#define SERVER_ROLES \
        (OFConstants::POLICY_REPOSITORY |     \
         OFConstants::ENDPOINT_REGISTRY |     \
         OFConstants::OBSERVER)
#define LOCALHOST "127.0.0.1"

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);
    // Parse command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Print this help message")
        ("log", po::value<string>()->default_value(""),
         "Log to the specified file (default standard out)")
        ("level", po::value<string>()->default_value("info"),
         "Use the specified log level (default info)")
        ("policy,p", po::value<string>()->default_value(""),
         "Read the specified policy file to seed the MODB")
        ;

    std::string log_file;
    std::string level_str;
    std::string policy_file;

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
        log_file = vm["log"].as<string>();
        level_str = vm["level"].as<string>();
        policy_file = vm["policy"].as<string>();

    } catch (const po::unknown_option& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    initLogging(level_str, false /*syslog*/, log_file, "framework-stress");

    try {
        MockOpflexServer::peer_vec_t peer_vec;
        peer_vec.push_back(make_pair(SERVER_ROLES, LOCALHOST":8009"));
        MockOpflexServer server(8009, SERVER_ROLES, peer_vec, std::vector<std::string>(),
                                modelgbp::getMetadata());

        if (policy_file != "") {
            server.readPolicy(policy_file);
        }

        server.start();

        signal(SIGINT | SIGTERM, sighandler);
        while (running) {
            gotconfig = 0;

            opflex::ofcore::OFFramework framework;
            framework.setModel(modelgbp::getMetadata());
            framework.setOpflexIdentity("test", "test");
            framework.start();
            framework.addPeer(LOCALHOST, 8009);

            modelgbp::platform::Config::
                registerListener(framework, &configListener);
            modelgbp::gbp::EpGroup::
                registerListener(framework, &configListener);

            shared_ptr<modelgbp::epdr::L2Discovered> l2d;
            {
                opflex::modb::Mutator mutator(framework, "init");
                shared_ptr<modelgbp::dmtree::Root> root =
                    modelgbp::dmtree::Root::createRootElement(framework);
                root->addPolicyUniverse();
                root->addRelatorUniverse();
                root->addEprL2Universe();
                root->addEprL3Universe();
                l2d = root->addEpdrL2Discovered();
                root->addEpdrL3Discovered();
                root->addGbpeVMUniverse();
                root->addObserverEpStatUniverse();

                root->addDomainConfig()
                    ->addDomainConfigToConfigRSrc()
                    ->setTargetConfig("openstack");

                mutator.commit();
            }
            {
                opflex::modb::Mutator mutator(framework, "policyelement");
                l2d->addEpdrLocalL2Ep("1")
                    ->addEpdrEndPointToGroupRSrc()
                    ->setTargetEpGroup(opflex::modb::URIBuilder()
                                       .addElement("PolicyUniverse")
                                       .addElement("PolicySpace")
                                       .addElement("test")
                                       .addElement("GbpEpGroup")
                                       .addElement("group1").build());
                l2d->addEpdrLocalL2Ep("2")
                    ->addEpdrEndPointToGroupRSrc()
                    ->setTargetEpGroup(opflex::modb::URIBuilder()
                                       .addElement("PolicyUniverse")
                                       .addElement("PolicySpace")
                                       .addElement("test")
                                       .addElement("GbpEpGroup")
                                       .addElement("group2").build());
                l2d->addEpdrLocalL2Ep("3")
                    ->addEpdrEndPointToGroupRSrc()
                    ->setTargetEpGroup(opflex::modb::URIBuilder()
                                       .addElement("PolicyUniverse")
                                       .addElement("PolicySpace")
                                       .addElement("test")
                                       .addElement("GbpEpGroup")
                                       .addElement("group3").build());
                mutator.commit();
            }

            int c = 0;
            while (gotconfig < 2 && c < 500) {
                c += 1;
                usleep(1000);
            }
            framework.prettyPrintMODB(opflexagent::Logger(INFO, __FILE__,
                                                          __LINE__,
                                                          __FUNCTION__)
                                      .stream() << "\n",
                                      true, false, true);
            if (gotconfig < 2)
                LOG(ERROR) << "Didn't happen!";
            framework.stop();
        }

        server.stop();
    } catch (const std::exception& e) {
        LOG(ERROR) << "Fatal error: " << e.what();
        return 2;
    } catch (...) {
        LOG(ERROR) << "Unknown fatal error";
        return 3;
    }
}
