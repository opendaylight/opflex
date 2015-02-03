/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for AgentLogHandler class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <iostream>

#include "AgentLogHandler.h"
#include "logging.h"

namespace ovsagent {

using opflex::logging::OFLogHandler;

#ifdef USE_BOOST_LOG
using boost::log::trivial::severity_level;
#endif

AgentLogHandler::AgentLogHandler(Level logLevel): OFLogHandler(logLevel) { }
AgentLogHandler::~AgentLogHandler() { }

void AgentLogHandler::setLevel(Level logLevel) {
    logLevel_ = logLevel;
}

void AgentLogHandler::handleMessage(const std::string& file,
                                   const int line,
                                   const std::string& function,
                                   const Level level,
                                   const std::string& message) {
#ifdef USE_BOOST_LOG
    severity_level blevel;
    switch (level) {
    case OFLogHandler::TRACE:
    case OFLogHandler::DEBUG4:
    case OFLogHandler::DEBUG3:
    case OFLogHandler::DEBUG2:
    case OFLogHandler::DEBUG1:
        blevel = ovsagent::DEBUG;
        break;
    case OFLogHandler::INFO:
        blevel = ovsagent::INFO;
        break;
    case OFLogHandler::WARNING:
        blevel = ovsagent::WARNING;
        break;
    case OFLogHandler::ERROR:
        blevel = ovsagent::ERROR;
        break;
    default:
    case OFLogHandler::FATAL:
        blevel = ovsagent::FATAL;
        break;
    }

    BOOST_LOG_STREAM_WITH_PARAMS(::boost::log::trivial::logger::get(),  \
                                 (::boost::log::keywords::severity = blevel)) \
        << "[" << file << ":" << line << ":" << function << "] " << message;
#else
    int slevel;
    switch (level) {
    case OFLogHandler::TRACE:
    case OFLogHandler::DEBUG4:
    case OFLogHandler::DEBUG3:
    case OFLogHandler::DEBUG2:
    case OFLogHandler::DEBUG1:
        slevel = LOG_DEBUG;
        break;
    case OFLogHandler::INFO:
        slevel = LOG_INFO;
        break;
    case OFLogHandler::WARNING:
        slevel = LOG_WARNING;
        break;
    case OFLogHandler::ERROR:
        slevel = LOG_ERR;
        break;
    default:
    case OFLogHandler::FATAL:
        slevel = LOG_CRIT;
        break;
    }

    std::cout << "<" << slevel << "> "
              << "[" << file << ":" << line << ":" << function << "] "
              << message << std::endl;

#endif
}

} /* namespace ovsagent */
