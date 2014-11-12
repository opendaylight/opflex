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

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/program_options.hpp>

#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/console.hpp>

#include "Agent.h"
#include "BoostLogHandler.h"
#include "logging.h"

using std::vector;
using std::string;
using opflex::ofcore::OFFramework;
using opflex::logging::OFLogHandler;
namespace po = boost::program_options;
namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;
using boost::log::trivial::severity_level;
using namespace ovsagent;

BoostLogHandler logHandler(OFLogHandler::NO_LOGGING);

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
#define LOG_FORMAT "[%TimeStamp%] [%Severity%] %Message%"

int main(int argc, char** argv) {
    // Parse command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Print this help message")
        ("config,c",
         po::value<string>()->default_value(DEFAULT_CONF), 
         "Read configuration from the specified file")
        ("log", po::value<string>(), "Log to the specified file (default standard out)")
        ("level", po::value<string>(), "Use the specified log level (default INFO)")
        ("daemon", "Run the agent as a daemon")
        ;

    bool daemon = false;
    OFLogHandler::Level level = OFLogHandler::INFO;

    std::string log_file;
    severity_level blevel = INFO;

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
        if (vm.count("log")) {
            log_file = vm["log"].as<string>();
        }
        if (vm.count("level")) {
            std::string levelstr = vm["level"].as<string>();
            if (levelstr == "debug") {
                level = OFLogHandler::DEBUG;
                blevel = boost::log::trivial::debug;
            } else if (levelstr == "info") {
                level = OFLogHandler::INFO;
                blevel = boost::log::trivial::info;
            } else if (levelstr == "warning") {
                level = OFLogHandler::WARNING;
                blevel = boost::log::trivial::warning;
            } else if (levelstr == "error") {
                level = OFLogHandler::ERROR;
                blevel = boost::log::trivial::error;
            } else if (levelstr == "fatal") {
                level = OFLogHandler::FATAL;
                blevel = boost::log::trivial::fatal;
            }
        }
    } catch (po::unknown_option e) {
        std::cerr << e.what() << std::endl;
    }

    if (daemon)
        daemonize();

    // Initialize logging
    logging::add_common_attributes();
    logging::core::get()->set_filter (logging::trivial::severity >= blevel);
    logging::register_simple_formatter_factory< severity_level, char >("Severity");
    if (log_file != "") {
        logging::add_file_log(keywords::file_name = log_file,
                              keywords::format = LOG_FORMAT,
                              keywords::auto_flush = true,
                              keywords::open_mode = (std::ios::out | std::ios::app));
    } else {
        logging::add_console_log(std::cout,
                                 keywords::format = LOG_FORMAT);
    }

    logHandler.setLevel(level);
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
