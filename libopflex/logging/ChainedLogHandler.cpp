/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for StdOutLogHandler class.
 *
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <opflex/logging/ChainedLogHandler.hpp>

#include <iostream>

namespace opflex {
namespace logging {

ChainedLogHandler::ChainedLogHandler(
        OFLogHandler * current,
        OFLogHandler * next)
    : OFLogHandler(
            next
              ?
            std::min(current->getLevel(), next->getLevel())
              :
            current->getLevel()),
            current_(current), next_(next) { }
ChainedLogHandler::~ChainedLogHandler() { }

void ChainedLogHandler::handleMessage(Logger const & logger) {
    current_->handleMessage(logger);

    if (!next_) {
        return;
    }

    next_->handleMessage(logger);
}

} /* namespace logging */
} /* namespace opflex */

