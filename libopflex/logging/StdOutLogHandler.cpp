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

#include <iostream>

#include "opflex/logging/StdOutLogHandler.h"

#include <uv.h>

namespace opflex {
namespace logging {

StdOutLogHandler::StdOutLogHandler(Level logLevel_): OFLogHandler(logLevel_) { }
StdOutLogHandler::~StdOutLogHandler() { }

void StdOutLogHandler::handleMessage(const std::string& file,
                                     const int line,
                                     const std::string& function,
                                     const Level level,
                                     const std::string& message) {
    if (level < logLevel_) return;

    std::cout << uv_now(uv_default_loop()) << " " << file << ":" << line << ":" << function <<
        "[" << level <<"] " << message << std::endl;
}

} /* namespace logging */
} /* namespace opflex */
