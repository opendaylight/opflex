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

#include "Agent.h"
#include "logging.h"
#include "cmd.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <string>
#include <iostream>

#include <signal.h>
#include <string.h>

using std::string;
using opflex::ofcore::OFFramework;
namespace po = boost::program_options;
namespace fs = boost::filesystem;
namespace pt = boost::property_tree;
using namespace ovsagent;

void sighandler(int sig) {
    LOG(INFO) << "Got " << strsignal(sig) << " signal";
}

#define DEFAULT_CONF SYSCONFDIR"/opflex-agent-ovs/opflex-agent-ovs.conf"

static void readConfig(Agent& agent, string configFile) {
    pt::ptree properties;

    LOG(INFO) << "Reading configuration from " << configFile;
    pt::read_json(configFile, properties);
    agent.setProperties(properties);
}

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);

    // Parse command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Print this help message")
        ("config,c",
         po::value<std::vector<string> >(),
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

        std::vector<string> configFiles;
        if (vm.count("config"))
            configFiles = vm["config"].as<std::vector<string> >();
        else
            configFiles.push_back(DEFAULT_CONF);

        for (const string& configFile : configFiles) {
            if (fs::is_directory(configFile)) {
                LOG(INFO) << "Reading configuration from config directory "
                          << configFile;

                fs::recursive_directory_iterator end;
                std::set<string> files;
                for (fs::recursive_directory_iterator it(configFile);
                     it != end; ++it) {
                    if (fs::is_regular_file(it->status())) {
                        string fstr = it->path().string();
                        if (boost::algorithm::ends_with(fstr, ".conf") &&
                            !boost::algorithm::starts_with(fstr, ".")) {
                            files.insert(fstr);
                        }
                    }
                }
                for (const std::string& fstr : files) {
                    readConfig(agent, fstr);
                }
            } else {
                readConfig(agent, configFile);
            }
        }

        agent.applyProperties();
        agent.start();

        // Pause the main thread until interrupted
        signal(SIGINT, sighandler);
        signal(SIGTERM, sighandler);
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
