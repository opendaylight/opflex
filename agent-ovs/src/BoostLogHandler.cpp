/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for BoostLogHandler class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <iostream>

#include <boost/log/trivial.hpp>

#include "BoostLogHandler.h"
#include "logging.h"

namespace ovsagent {

using opflex::logging::OFLogHandler;
using boost::log::trivial::severity_level;

BoostLogHandler::BoostLogHandler(Level logLevel): OFLogHandler(logLevel) { }
BoostLogHandler::~BoostLogHandler() { }

void BoostLogHandler::setLevel(Level logLevel) {
    logLevel_ = logLevel;
}

void BoostLogHandler::handleMessage(const std::string& file,
                                   const int line,
                                   const std::string& function,
                                   const Level level,
                                   const std::string& message) {
    severity_level blevel;
    switch (level) {
    case OFLogHandler::DEBUG:
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
}

} /* namespace ovsagent */
