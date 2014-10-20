/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for GLogLogHandler class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <iostream>

#include <glog/logging.h>

#include "GLogLogHandler.h"

namespace ovsagent {

using opflex::logging::OFLogHandler;

GLogLogHandler::GLogLogHandler(Level logLevel_): OFLogHandler(logLevel_) { }
GLogLogHandler::~GLogLogHandler() { }

void GLogLogHandler::handleMessage(const std::string& file,
                                   const int line,
                                   const std::string& function,
                                   const Level level,
                                   const std::string& message) {
    int glevel = 0;
    switch (level) {
    case OFLogHandler::DEBUG:
    case OFLogHandler::INFO:
        glevel = google::GLOG_INFO;
        break;
    case OFLogHandler::WARNING:
        glevel = google::GLOG_WARNING;
        break;
    case OFLogHandler::ERROR:
        glevel = google::GLOG_ERROR;
        break;
    default:
    case OFLogHandler::FATAL:
        glevel = google::GLOG_FATAL;
        break;
    }

    google::LogMessage(file.c_str(), line, level).stream()
        << "[" << function << "] " << message.c_str();
}

} /* namespace ovsagent */
