/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Definition of logging macros and utility functions.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef AGENT_LOGGING_H
#define AGENT_LOGGING_H

#include <string>
#include <iostream>
#include <sstream>
#include <boost/assert.hpp>

namespace opflexagent {

/**
 * Supported log levels
 */
enum LogLevel { FATAL, ERROR, WARNING, INFO, DEBUG };

extern LogLevel logLevel;

/**
 * Abstract base class for log destination such as console, file etc.
 */
class LogSink {
public:
    /**
     * Write a log message to the log destination alongwith information about
     * its origin (source file, line number etc).
     *
     * @param level The log level of the message
     * @param filename Name of source file that generated the message
     * @param lineno Line number in source file that generated the message
     * @param functionName Name of function that generated the message
     * @param message The log message to write
     */
    virtual
    void write(LogLevel level, const char *filename, int lineno,
               const char *functionName, const std::string& message) = 0;
};

/**
 * Initialize logging
 *
 * @param level the log level to log at
 * @param toSyslog if true, log messages are written to syslog; parameter
 * "log_file" is ignored in that case
 * @param log_file the file to log to, or empty for console
 * @param syslog_name the name to use when logging to syslog
 */
void initLogging(const std::string& level, bool toSyslog,
                 const std::string& log_file,
                 const std::string& syslog_name = "opflex-agent");

/**
 * Change the logging level of the agent.
 *
 * @param level the log level to log at. If this is not a valid string,
 * logging level is set to the default level.
 */
void setLoggingLevel(const std::string& level);

/**
 * Get the currently configure log destination. The return pointer is never
 * NULL.
 */
LogSink * getLogSink();

/**
 * Logger used to construct and write a log message to the current log
 * destination.
 */
class Logger {
public:
    /**
     * Constructor that expects information about the origin of log message.
     * @param l The log level of the message
     * @param f Name of source file of the message
     * @param no Line number in source file of the message
     * @param fn Name of function enclosing the message
     */
    Logger(LogLevel l, const char *f, int no, const char *fn) :
        level(l), filename(f), lineNumber(no), functionName(fn) {}

    /**
     * Destroy the logger and log the output
     */
    ~Logger() {
        getLogSink()->write(level, filename, lineNumber, functionName,
                            buffer_.str());
    }

    /**
     * Get the ostream to write to
     */
    std::ostream& stream() { return buffer_; }

private:
    /**
     * The internal buffer for the logger
     */
    std::ostringstream buffer_;
    /**
     * The log level of the message
     */
    LogLevel level;
    /**
     * Name of source file of the message
     */
    const char *filename;
    /**
     * Line number in source file of the message
     */
    int lineNumber;
    /**
     * Name of function enclosing the message
     */
    const char *functionName;
};

#define LOG(lvl)                                                        \
    if (lvl <= opflexagent::logLevel)                                      \
        opflexagent::Logger(lvl, __FILE__, __LINE__, __FUNCTION__).stream()

#define LOG1(lvl, filename, lineNo, functionName, message)              \
    if (lvl <= opflexagent::logLevel)                                      \
        opflexagent::getLogSink()->write(lvl, filename, lineNo,            \
                                      functionName, message);
} /* namespace opflexagent */

#endif /* AGENT_LOGGING_H */
