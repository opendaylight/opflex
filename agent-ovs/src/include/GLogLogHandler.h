/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file GLogLogHandler.h
 * @brief Interface definition file for GLogLogHandler
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OVSAGENT_GLOGLOGHANDLER_H
#define OVSAGENT_GLOGLOGHANDLER_H

#include <opflex/logging/OFLogHandler.h>

namespace ovsagent {

/**
 * An OFLogHandler that logs to google log library
 */
class GLogLogHandler : public opflex::logging::OFLogHandler {
public:
    /**
     * Allocate a log handler that will log any messages with equal or
     * greater severity than the specified log level.
     * 
     * @param logLevel the minimum log level
     */
    GLogLogHandler(Level logLevel);
    virtual ~GLogLogHandler();

    /* see OFLogHandler */
    virtual void handleMessage(const std::string& file,
                               const int line,
                               const std::string& function,
                               const opflex::logging::OFLogHandler::Level level,
                               const std::string& message);
};

} /* namespace ovsagent */

#endif /* OVSAGENT_GLOGLOGHANDLER_H */
