/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Main implementation for OVS agent
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <signal.h>
#include <string.h>

#include <string>
#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/program_options.hpp>

#include "Agent.h"
#include "logging.h"
#include "cmd.h"
#include <signal.h>

using std::string;
using opflex::ofcore::OFFramework;
namespace po = boost::program_options;
using namespace ovsagent;

void sighandler(int sig) {
    LOG(INFO) << "Got " << strsignal(sig) << " signal";
}

#define DEFAULT_CONF SYSCONFDIR"/opflex-agent-ovs/opflex-agent-ovs.conf"

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);

    // Parse command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Print this help message")
        ("config,c",
         po::value<string>()->default_value(DEFAULT_CONF), 
         "Read configuration from the specified file")
        ("log", po::value<string>()->default_value(""), 
         "Log to the specified file (default standard out)")
        ("level", po::value<string>()->default_value("info"),
         "Use the specified log level (default info). "
         "Overridden by log level in configuration file")
        ("syslog", "Log to syslog instead of file or standard out")
        ("daemon", "Run the agent as a daemon")
        ;
    
    bool daemon = false;
    bool logToSyslog = false;
    std::string log_file;
    std::string level_str;

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
        if (vm.count("syslog")) {
            logToSyslog = true;
        }
    } catch (po::unknown_option e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    if (daemon)
        daemonize();

    initLogging(level_str, logToSyslog, log_file);

    try {
        // Initialize agent and configuration
        Agent agent(OFFramework::defaultInstance());
        
        using boost::property_tree::ptree;
        ptree properties;

        string configFile = vm["config"].as<string>();
        LOG(INFO) << "Reading configuration from " << configFile;
        read_json(configFile, properties);
        agent.setProperties(properties);
        
        agent.start();

        // Pause the main thread until interrupted
        signal(SIGINT, sighandler);
        pause();
        agent.stop();
        return 0;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Fatal error: " << e.what();
        return 2;
    } catch (...) {
        LOG(ERROR) << "Unknown fatal error";
        return 3;
    }
}
