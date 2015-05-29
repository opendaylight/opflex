/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file AgentLogHandler.h
 * @brief Interface definition file for AgentLogHandler
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OVSAGENT_AGENTLOGHANDLER_H
#define OVSAGENT_AGENTLOGHANDLER_H

#include <opflex/logging/OFLogHandler.h>

namespace ovsagent {

/**
 * An OFLogHandler that logs to google log library
 */
class AgentLogHandler : public opflex::logging::OFLogHandler {
public:
    /**
     * Allocate a log handler that will log any messages with equal or
     * greater severity than the specified log level.
     * 
     * @param logLevel the minimum log level
     */
    AgentLogHandler(opflex::logging::OFLogHandler::Level logLevel);
    virtual ~AgentLogHandler();

    /**
     * Implement opflex::logging::OFLogHandler::handleMessage
     */
    virtual void handleMessage(opflex::logging::Logger const & logger);

    /**
     * Implement opflex::logging::OFLogHandler::setLevel
     */
    void setLevel(opflex::logging::OFLogHandler::Level logLevel);
};

} /* namespace ovsagent */

#endif /* OVSAGENT_AGENTLOGHANDLER_H */
