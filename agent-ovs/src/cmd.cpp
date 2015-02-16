/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Utility functions for command-line programs
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <opflex/logging/OFLogHandler.h>
#include "ovs.h"
#include "logging.h"

#ifdef USE_BOOST_LOG
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/console.hpp>
#endif

#include "cmd.h"
#include "AgentLogHandler.h"

#ifdef USE_BOOST_LOG
namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;
using boost::log::trivial::severity_level;
#endif

using opflex::logging::OFLogHandler;

namespace ovsagent {

AgentLogHandler logHandler(OFLogHandler::NO_LOGGING);

void daemonize()
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

#define LOG_FORMAT "[%TimeStamp%] [%Severity%] %Message%"

#ifndef USE_BOOST_LOG
int logLevel = INFO;
#endif

void initLogging(const std::string& levelstr,
                 const std::string& log_file) {
#ifdef USE_BOOST_LOG
    logging::add_common_attributes();
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
#endif

    OFLogHandler::registerHandler(logHandler);

    setLoggingLevel(levelstr);
    /* No good way to redirect OVS logs to our logs, suppress them for now */
    vlog_set_levels(NULL, VLF_ANY_DESTINATION, VLL_OFF);
}

void setLoggingLevel(const std::string& newLevelstr) {
    OFLogHandler::Level level = OFLogHandler::INFO;

    std::string levelstr = newLevelstr;
    std::transform(levelstr.begin(), levelstr.end(),
                   levelstr.begin(), ::tolower);

#ifdef USE_BOOST_LOG
    severity_level blevel = INFO;
#endif

    if (levelstr == "debug" || levelstr == "debug0") {
        level = OFLogHandler::DEBUG0;
#ifdef USE_BOOST_LOG
        blevel = boost::log::trivial::debug;
#else
        logLevel = DEBUG;
#endif
    } else if (levelstr == "debug1") {
        level = OFLogHandler::DEBUG1;
#ifdef USE_BOOST_LOG
        blevel = boost::log::trivial::debug;
#else
        logLevel = DEBUG;
#endif
    } else if (levelstr == "debug2") {
        level = OFLogHandler::DEBUG2;
#ifdef USE_BOOST_LOG
        blevel = boost::log::trivial::debug;
#else
        logLevel = DEBUG;
#endif
    } else if (levelstr == "debug3") {
        level = OFLogHandler::DEBUG3;
#ifdef USE_BOOST_LOG
        blevel = boost::log::trivial::debug;
#else
        logLevel = DEBUG;
#endif
    } else if (levelstr == "debug4") {
        level = OFLogHandler::DEBUG4;
#ifdef USE_BOOST_LOG
        blevel = boost::log::trivial::debug;
#else
        logLevel = DEBUG;
#endif
    } else if (levelstr == "debug5") {
        level = OFLogHandler::DEBUG5;
#ifdef USE_BOOST_LOG
        blevel = boost::log::trivial::debug;
#else
        logLevel = DEBUG;
#endif
    } else if (levelstr == "debug6") {
        level = OFLogHandler::DEBUG6;
#ifdef USE_BOOST_LOG
        blevel = boost::log::trivial::debug;
#else
        logLevel = DEBUG;
#endif
    } else if (levelstr == "debug7") {
        level = OFLogHandler::DEBUG7;
#ifdef USE_BOOST_LOG
        blevel = boost::log::trivial::debug;
#else
        logLevel = DEBUG;
#endif
    } else if (levelstr == "trace") {
        level = OFLogHandler::TRACE;
#ifdef USE_BOOST_LOG
        blevel = boost::log::trivial::debug;
#else
        logLevel = DEBUG;
#endif
    } else if (levelstr == "info") {
        level = OFLogHandler::INFO;
#ifdef USE_BOOST_LOG
        blevel = boost::log::trivial::info;
#else
        logLevel = INFO;
#endif
    } else if (levelstr == "warning") {
        level = OFLogHandler::WARNING;
#ifdef USE_BOOST_LOG
        blevel = boost::log::trivial::warning;
#else
        logLevel = WARNING;
#endif
    } else if (levelstr == "error") {
        level = OFLogHandler::ERROR;
#ifdef USE_BOOST_LOG
        blevel = boost::log::trivial::error;
#else
        logLevel = ERROR;
#endif
    } else if (levelstr == "fatal") {
        level = OFLogHandler::FATAL;
#ifdef USE_BOOST_LOG
        blevel = boost::log::trivial::fatal;
#else
        logLevel = FATAL;
#endif
    }

#ifdef USE_BOOST_LOG
    logging::core::get()->set_filter (logging::trivial::severity >= blevel);
#endif

    logHandler.setLevel(level);
}

} /* namespace ovsagent */
