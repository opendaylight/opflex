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

#include <opflexagent/AgentLogHandler.h>
#include <opflexagent/logging.h>

namespace ovsagent {

using opflex::logging::OFLogHandler;

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
    ovsagent::LogLevel agentLevel = ovsagent::INFO;
    switch (level) {
    case OFLogHandler::TRACE:
    case OFLogHandler::DEBUG7:
    case OFLogHandler::DEBUG6:
    case OFLogHandler::DEBUG5:
    case OFLogHandler::DEBUG4:
    case OFLogHandler::DEBUG3:
    case OFLogHandler::DEBUG2:
    case OFLogHandler::DEBUG1:
    case OFLogHandler::DEBUG0:
        agentLevel = ovsagent::DEBUG;
        break;
    case OFLogHandler::INFO:
        agentLevel = ovsagent::INFO;
        break;
    case OFLogHandler::WARNING:
        agentLevel = ovsagent::WARNING;
        break;
    case OFLogHandler::ERROR:
        agentLevel = ovsagent::ERROR;
        break;
    default:
    case OFLogHandler::FATAL:
        agentLevel = ovsagent::FATAL;
        break;
    }
    LOG1(agentLevel, file.c_str(), line, function.c_str(), message);
}

} /* namespace ovsagent */
