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

#include <unistd.h>
#include <signal.h>
#include <string.h>

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

void sighandler(int sig) {
    LOG(INFO) << "Got " << strsignal(sig) << " signal";
}

static void daemonize(void)
{
    pid_t pid, sid;
    int fd;

    if ( getppid() == 1 ) return;
    pid = fork();
    if (pid < 0)
        exit(1);

    if (pid > 0)
        exit(EXIT_SUCCESS);
    sid = setsid();
    if (sid < 0)
        exit(1);
    if ((chdir("/")) < 0)
        exit(1);

    fd = open("/dev/null",O_RDWR, 0);

    if (fd != -1) {
        dup2 (fd, STDIN_FILENO);
        dup2 (fd, STDOUT_FILENO);
        dup2 (fd, STDERR_FILENO);
        if (fd > 2)
            close (fd);
    }
    umask(027);
}

#define DEFAULT_CONF SYSCONFDIR"/opflex-agent-ovs/opflex-agent-ovs.conf"

int main(int argc, char** argv) {
    // Parse command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Print this help message")
        ("config,c",
         po::value<string>()->default_value(DEFAULT_CONF), 
         "Read configuration from the specified file")
        ("daemon", "Run the agent as a daemon")
        ;

    bool daemon = false;

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
    } catch (po::unknown_option e) {
        std::cerr << e.what() << std::endl;
    }

    if (daemon)
        daemonize();

    // Initialize logging
    google::InitGoogleLogging(argv[0]);
    OFLogHandler::registerHandler(logHandler);

    try {
        // Initialize agent and configuration
        Agent agent(OFFramework::defaultInstance());
        
        using boost::property_tree::ptree;
        ptree properties;
        LOG(INFO) << "Reading configuration from " << vm["config"].as<string>();
        read_json(vm["config"].as<string>(), properties);
        agent.setProperties(properties);
        
        agent.start();

        // Pause the main thread until interrupted
        signal(SIGINT, sighandler);
        signal(SIGSTOP, sighandler);
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
