/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Mock server standalone
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#include <unistd.h>
#include <signal.h>

#include <string>
#include <vector>
#include <iostream>

#include <boost/program_options.hpp>
#include <boost/assign/list_of.hpp>

#include <modelgbp/dmtree/Root.hpp>
#include <modelgbp/metadata/metadata.hpp>
#include <opflex/test/MockOpflexServer.h>
#include <opflex/ofcore/OFFramework.h>
#include <opflex/ofcore/OFConstants.h>

#include <opflexagent/logging.h>
#include <opflexagent/cmd.h>
#include "Policies.h"
#include <signal.h>

using std::string;
using std::make_pair;
using boost::assign::list_of;
namespace po = boost::program_options;
using opflex::test::MockOpflexServer;
using opflex::ofcore::OFConstants;
using namespace opflexagent;

void sighandler(int sig) {
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
        ("sample", po::value<string>()->default_value(""),
         "Output a sample policy to the given file then exit")
        ("daemon", "Run the mock server as a daemon")
        ("policy,p", po::value<string>()->default_value(""),
         "Read the specified policy file to seed the MODB")
        ("ssl_castore", po::value<string>()->default_value("/etc/ssl/certs/"),
         "Use the specified path or certificate file as the SSL CA store")
        ("ssl_key", po::value<string>()->default_value(""),
         "Enable SSL and use the private key specified")
        ("ssl_pass", po::value<string>()->default_value(""),
         "Use the specified password for the private key")
        ("peer", po::value<std::vector<string> >(),
         "A peer specified as hostname:port to return in identity response")
        ;

    bool daemon = false;
    std::string log_file;
    std::string level_str;
    std::string policy_file;
    std::string sample_file;
    std::string ssl_castore;
    std::string ssl_key;
    std::string ssl_pass;
    std::vector<std::string> peers;

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
        if (vm.count("daemon")) {
            daemon = true;
        }
        log_file = vm["log"].as<string>();
        level_str = vm["level"].as<string>();
        policy_file = vm["policy"].as<string>();
        sample_file = vm["sample"].as<string>();
        ssl_castore = vm["ssl_castore"].as<string>();
        ssl_key = vm["ssl_key"].as<string>();
        ssl_pass = vm["ssl_pass"].as<string>();
        if (vm.count("peer"))
            peers = vm["peer"].as<std::vector<string> >();

    } catch (po::unknown_option e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    if (daemon)
        daemonize();

    initLogging(level_str, false /*syslog*/, log_file, "mock-server");

    try {
        if (sample_file != "") {
            opflex::ofcore::MockOFFramework mframework;
            mframework.setModel(modelgbp::getMetadata());
            mframework.start();
            Policies::writeBasicInit(mframework);
            Policies::writeTestPolicy(mframework);

            mframework.dumpMODB(sample_file);

            mframework.stop();
            return 0;
        }

        MockOpflexServer::peer_vec_t peer_vec;
        for (const std::string& pstr : peers)
            peer_vec.push_back(make_pair(SERVER_ROLES, pstr));
        if (peer_vec.size() == 0)
            peer_vec.push_back(make_pair(SERVER_ROLES, LOCALHOST":8009"));

        MockOpflexServer server(8009, SERVER_ROLES, peer_vec,
                                modelgbp::getMetadata());

        if (policy_file != "") {
            server.readPolicy(policy_file);
        }

        if (ssl_key != "") {
            server.enableSSL(ssl_castore, ssl_key, ssl_pass);
        }

        server.start();
        signal(SIGINT | SIGTERM, sighandler);
        pause();
        server.stop();
    } catch (const std::exception& e) {
        LOG(ERROR) << "Fatal error: " << e.what();
        return 2;
    } catch (...) {
        LOG(ERROR) << "Unknown fatal error";
        return 3;
    }
}
