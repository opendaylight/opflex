/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for StdOutLogHandler class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif


#include <iostream>

#include <opflex/logging/StdOutLogHandler.h>

namespace opflex {
namespace logging {

StdOutLogHandler::StdOutLogHandler(Level logLevel_): OFLogHandler(logLevel_) { }
StdOutLogHandler::~StdOutLogHandler() { }

void StdOutLogHandler::handleMessage(
        Logger const & logger) {

    if (logger.level_ < logLevel_) {
        return;
    }

    std::cout
        << logger.file_
        << ":"
        << logger.line_
        << ":"
        << logger.function_
        << "["
        << logger.level_
        << "] "
        << logger.message()
        << std::endl
    ;
}

} /* namespace logging */
} /* namespace opflex */
