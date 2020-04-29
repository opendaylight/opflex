/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _INCLUDE__OPFLEX__LOGGING_HPP
#define _INCLUDE__OPFLEX__LOGGING_HPP

#include <sstream>
#include <iostream>

#include "opflex/logging/OFLogHandler.h"

namespace opflex {
namespace logging {
namespace internal {

/**
 * A simple logging interface used to process log messages internally
 */
class Logger {
  public:

    /**
     * Get the output buffer to write to
     */
    std::ostream & stream()
        __attribute__((no_instrument_function));

    /**
     * Construct a new logger to handle a specific message
     *
     * @param level the log level 
     * @param file the file that is generating the log
     * @param line the line number
     * @param function the function that is generating the log
     */
    Logger(OFLogHandler::Level const level,
           char const * file,
           int const line,
           char const * function)
        __attribute__((no_instrument_function));

    ~Logger()
        __attribute__((no_instrument_function));

private:
    OFLogHandler::Level const level_;
    char const * file_;
    int const line_;
    char const * function_;

    std::ostringstream buffer_;
};

} /* namespace internal */
} /* namespace logging */
} /* namespace opflex */

/**
 * Convenience macro for debug log level
 */
#define TRACE opflex::logging::OFLogHandler::TRACE

/**
 * Convenience macro for debug log level 0
 */
#define DEBUG0 opflex::logging::OFLogHandler::DEBUG0

/**
 * Convenience macro for debug log level 1
 */
#define DEBUG1 opflex::logging::OFLogHandler::DEBUG1

/**
 * Alias for DEBUG0
 */
#define DEBUG DEBUG0

/**
 * Convenience macro for debug log level 2
 */
#define DEBUG2 opflex::logging::OFLogHandler::DEBUG2

/**
 * Convenience macro for debug log level 3
 */
#define DEBUG3 opflex::logging::OFLogHandler::DEBUG3

/**
 * Convenience macro for debug log level 4
 */
#define DEBUG4 opflex::logging::OFLogHandler::DEBUG4

/**
 * Convenience macro for debug log level 5
 */
#define DEBUG5 opflex::logging::OFLogHandler::DEBUG5

/**
 * Convenience macro for info log level
 */
#define INFO opflex::logging::OFLogHandler::INFO

/**
 * Convenience macro for warning log level
 */
#define WARNING opflex::logging::OFLogHandler::WARNING

/**
 * Convenience macro for error log level
 */
#define ERROR opflex::logging::OFLogHandler::ERROR

/**
 * Convenience macro for fatal log level
 */
#define FATAL opflex::logging::OFLogHandler::FATAL

/**
 * Check whether log level is low enough before computing expensive
 * log messages
 */
#define LOG_SHOULD_EMIT(level)                                          \
    (opflex::logging::OFLogHandler::getHandler()->shouldEmit(level))

/**
 * Create a log stream that you can log to as in:
 * @code
 * LOG(INFO) << "My message";
 * @endcode
 * Note that you should NOT include a newline in your log message
 */
#define LOG(level)                                                      \
    if(LOG_SHOULD_EMIT(level))                                          \
        opflex::logging::internal::Logger(level,                        \
                                          __FILE__,                     \
                                          __LINE__,                     \
                                          __FUNCTION__)                 \
            .stream()                                                   \

/* quick and dirty compatibility with glog's verbose logging */
#define VLOG_TO_LEVEL(integer) \
    (opflex::logging::OFLogHandler::Level(INFO-TRACE-1-(integer)))

#define VLOG(integer) \
    LOG(VLOG_TO_LEVEL(integer))

#define VLOG_IS_ON(integer) \
    LOG_SHOULD_EMIT(VLOG_TO_LEVEL(integer))

#endif /* _INCLUDE__OPFLEX__LOGGING_HPP */
