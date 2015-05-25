/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file StdOutLogHandler.h
 * @brief Interface definition file for StdOutLogHandler
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEX_LOGGING_STDOUTLOGHANDLER_H
#define OPFLEX_LOGGING_STDOUTLOGHANDLER_H

#include "opflex/logging/OFLogHandler.h"

namespace opflex {
namespace logging {

/**
 * An @ref OFLogHandler that simply logs to standard output.
 */
class StdOutLogHandler : public OFLogHandler {
public:
    /**
     * Allocate a log handler that will log any messages with equal or
     * greater severity than the specified log level.
     * 
     * @param logLevel the minimum log level
     */
    StdOutLogHandler(Level logLevel)
        __attribute__((no_instrument_function));
    virtual ~StdOutLogHandler()
        __attribute__((no_instrument_function));

    /* see OFLogHandler */
    virtual void handleMessage(Logger const & logger)
        __attribute__((no_instrument_function));
};

} /* namespace logging */
} /* namespace opflex */

#endif /* OPFLEX_LOGGING_STDOUTLOGHANDLER_H */
