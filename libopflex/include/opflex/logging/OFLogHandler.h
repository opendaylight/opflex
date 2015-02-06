/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OFLogHandler.h
 * @brief Interface definition file for OFLogHandler
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEX_LOGGING_OFLOGHANDLER_H
#define OPFLEX_LOGGING_OFLOGHANDLER_H

#include <string>

namespace opflex {
namespace logging {

/**
 * \addtogroup cpp
 * @{
 */

/**
 * @defgroup logging Logging
 * Define logging facility for the OpFlex framework.
 * @{
 */


/**
 * Interface for a log message handler for the OpFlex framework.  You
 * will need to implement this interface and register your
 * implementation if you want to log to your custom handler.
 *
 * By default, the OpFlex Framework will simply log to standard out at
 * INFO or above, but you can override this behavior by registering a
 * customer log handler.
 *
 * If you simply want to disable logging, the simplest way is as
 * follows:
 * @code
 * StdOutLogHandler disabledHandler(OFLogHandler::NO_LOGGING);
 * OFLogHandler::registerHandler(disabledHandler);
 * @endcode
 *
 */
class OFLogHandler {
public:

    /**
     * Log levels for OpFlex framework logging
     */
    enum Level {
        TRACE,
        DEBUG7,
        DEBUG6,
        DEBUG5,
        DEBUG4,
        DEBUG3,
        DEBUG2,
        DEBUG1,
        DEBUG0,
        INFO,
        WARNING,
        ERROR,
        FATAL,
        
        /* keep as the last one */
        NO_LOGGING
    };

    /**
     * Allocate a log handler that will log any messages with equal or
     * greater severity than the specified log level.
     * 
     * @param logLevel the minimum log level
     */
    OFLogHandler(Level logLevel)
        __attribute__((no_instrument_function));

    virtual ~OFLogHandler()
        __attribute__((no_instrument_function));

    /**
     * Process a single log message.  This file is called
     * synchronously from the thread that is doing the logging and is
     * unsynchronized.
     *
     * @param file the file that performs the logging
     * @param line the line number for the log message
     * @param function the name of the function that's performing the
     * logging
     * @param level the log level of the log message
     * @param message the formatted message to log
     */
    virtual void handleMessage(const std::string& file,
                               const int line,
                               const std::string& function,
                               const Level level,
                               const std::string& message) = 0;

    /**
     * Check whether we should attempt to log at the given log level.
     * 
     * @param level the level of a message to log
     * @return true if the log level could be allowed
     */
    virtual bool shouldEmit(const Level level)
        __attribute__((no_instrument_function));

    /**
     * Register a custom handler as the log handler.  You must ensure
     * that the custom log handler is not deallocated before any
     * framework components that might need to log to it.
     * 
     * @param handler the customer handler to register
     */
    static void registerHandler(OFLogHandler& handler)
        __attribute__((no_instrument_function));

    /**
     * Get the currently-active log handler.  Returns the default
     * handler if there is no active custom handler.
     *
     * @return the currently-active log handler
     */
    static OFLogHandler* getHandler()
        __attribute__((no_instrument_function));

protected:
    /**
     * The log level for this logger.
     */
    Level logLevel_;
};

/* @} logging */
/* @} cpp */

} /* namespace logging */
} /* namespace opflex */

#endif /* OPFLEX_LOGGING_OFLOGHANDLER_H */
