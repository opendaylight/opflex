/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file VppLogHandler.h
 * @brief Interface definition file for AgentLogHandler
 */
/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef __VPP_LOG_HANDLER_H__
#define __VPP_LOG_HANDLER_H__

#include <vom/logger.hpp>

namespace ovsagent {

/**
 * A VOM log handler that logs to the agnet logging mechanism
 */
class VppLogHandler : public VOM::log_t::handler {
public:
    /**
     * Constructor
     */
    VppLogHandler() = default;

    /**
     * Desctructor
     */
    ~VppLogHandler() = default;

    /**
     * Implement log_t::handler::handle_message
     */
    void handle_message(const std::string& file, const int line,
                        const std::string& function,
                        const VOM::log_level_t& level,
                        const std::string& message);
private:
    /**
     * Copy Constructor
     */
    VppLogHandler(const VppLogHandler&) = delete;
};

} /* namespace ovsagent */

#endif
