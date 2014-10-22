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

#include <vector>
#include <string>
#include <iostream>

#include <glog/logging.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/program_options.hpp>

#include "Agent.h"
#include "GLogLogHandler.h"

using std::vector;
using std::string;
using ovsagent::Agent;
using ovsagent::GLogLogHandler;
using opflex::ofcore::OFFramework;
using opflex::logging::OFLogHandler;
namespace po = boost::program_options;

GLogLogHandler logHandler(OFLogHandler::INFO);

int main(int argc, char** argv) {
    // Parse command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Print this help message")
        ("config,c",
         po::value<string>()->default_value(SYSCONFDIR"/agent-ovs.conf"), 
         "Configuration file")
        ;

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).
                  options(desc).run(), vm);
        po::notify(vm);
    
        if (vm.count("help")) {
            std::cout << "Usage: options_description [options]\n";
            std::cout << desc;
            return 0;
        }
    } catch (po::unknown_option e) {
        std::cerr << e.what() << std::endl;
    }

    // Initialize logging
    google::InitGoogleLogging(argv[0]);
    OFLogHandler::registerHandler(logHandler);

    // Initialize agent and configuration
    Agent agent(OFFramework::defaultInstance());

    using boost::property_tree::ptree;
    ptree properties;
    read_json(vm["config"].as<string>(), properties);
    agent.setProperties(properties);

    agent.start();
    agent.stop();
}
