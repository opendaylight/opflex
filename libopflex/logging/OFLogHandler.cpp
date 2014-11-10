/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OFLogHandler class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/logging/OFLogHandler.h"
#include "opflex/logging/StdOutLogHandler.h"

namespace opflex {
namespace logging {

static OFLogHandler* volatile activeHandler = NULL;

OFLogHandler::OFLogHandler(Level logLevel) : logLevel_(logLevel) { };
OFLogHandler::~OFLogHandler() { }

void OFLogHandler::registerHandler(OFLogHandler& handler) {
    activeHandler = &handler;
}

OFLogHandler* OFLogHandler::getHandler() {
    static StdOutLogHandler defaultHandler(INFO);
    if (activeHandler) return activeHandler;
    return &defaultHandler;
}

bool OFLogHandler::shouldEmit(const Level level) {
    return level >= logLevel_;
}

} /* namespace logging */
} /* namespace opflex */
