/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for VppLogHandler class.
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <iostream>

#include "VppLogHandler.h"
#include "logging.h"

namespace ovsagent {

void VppLogHandler::handle_message(const std::string& file, const int line,
                                   const std::string& function,
                                   const VOM::log_level_t& level,
                                   const std::string& message) {
    ovsagent::LogLevel agentLevel = ovsagent::INFO;

    if (VOM::log_level_t::DEBUG == level)
        agentLevel = ovsagent::DEBUG;
    else if (VOM::log_level_t::INFO == level)
        agentLevel = ovsagent::INFO;
    else if (VOM::log_level_t::WARNING == level)
        agentLevel = ovsagent::WARNING;
    else if (VOM::log_level_t::ERROR == level)
        agentLevel = ovsagent::ERROR;
    else if (VOM::log_level_t::CRITICAL == level)
        agentLevel = ovsagent::FATAL;

    LOG1(agentLevel, file.c_str(), line, function.c_str(), message);
}

} /* namespace ovsagent */
