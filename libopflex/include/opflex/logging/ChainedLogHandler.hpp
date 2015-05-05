/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file ChainedHandler.h
 * @brief Interface definition file for ChainedHandler
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _____INCLUDE__OPFLEX__LOGGING__CHAINEDHANDLER_HPP
#define _____INCLUDE__OPFLEX__LOGGING__CHAINEDHANDLER_HPP

#include <opflex/logging/OFLogHandler.h>

namespace opflex {
namespace logging {

/**
 * An @ref OFLogHandler that simply logs to standard output.
 */
class ChainedLogHandler : public OFLogHandler {
  public:
    /**
     * Allocate a log handler that will log any messages with equal or
     * greater severity than the specified log level.
     * 
     * @param logLevel the minimum log level
     */
    ChainedLogHandler(
            OFLogHandler * current,
            OFLogHandler * next = NULL)
        __attribute__((no_instrument_function));
    virtual ~ChainedLogHandler()
        __attribute__((no_instrument_function));

    /* see OFLogHandler */
    virtual void handleMessage(const Logger& logger)
        __attribute__((no_instrument_function));
  private:
    OFLogHandler * current_;
    OFLogHandler * next_;

};

} /* namespace logging */
} /* namespace opflex */

#endif /* _____INCLUDE__OPFLEX__LOGGING__CHAINEDHANDLER_HPP */
