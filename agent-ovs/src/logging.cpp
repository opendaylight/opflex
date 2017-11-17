/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation of logging related utility functions.
 *
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "logging.h"
#include "AgentLogHandler.h"

#include <opflex/logging/OFLogHandler.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#ifdef RENDERER_OVS
#include <openvswitch/vlog.h>
#endif
#ifdef RENDERER_VPP
#include <fstream>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <vom/logger.hpp>
#endif

#include <algorithm>
#include <fstream>
#include <mutex>

#include <syslog.h>

using opflex::logging::OFLogHandler;

namespace ovsagent {

AgentLogHandler logHandler(OFLogHandler::NO_LOGGING);

LogLevel logLevel = DEBUG;

/**
 * Log sink to write log messages to a standard output stream, such as
 * standard output or file stream.
 */
class OStreamLogSink : public LogSink {
public:
    /**
     * Constructor that accepts the output stream to write logs to.
     * @param outStream The stream to send messages to.
     */
    OStreamLogSink(std::ostream& outStream) : out(&outStream) {
        static const boost::posix_time::time_facet facet;
        out->imbue(std::locale(out->getloc(), &facet));
    }

    /**
     * Constructor that accepts the name of a file where log messages will be
     * appended.
     * @param fileName The filename to send log messages to.
     */
    OStreamLogSink(const std::string& fileName) :
        fileStream(fileName.c_str(), std::ios_base::out | std::ios_base::app) {
        if (!fileStream.good()) {
            out = &std::cout;
            std::cerr << "Unable to open log file: " << fileName << std::endl;
        } else {
            out = &fileStream;
        }
    }

    virtual
    void write(LogLevel level, const char *filename, int lineno,
               const char *functionName, const std::string& message) {
        const char *levelStr = LEVEL_STR_DEBUG;
        switch (level) {
        case DEBUG:   levelStr = LEVEL_STR_DEBUG; break;
        case INFO:    levelStr = LEVEL_STR_INFO; break;
        case WARNING: levelStr = LEVEL_STR_WARNING; break;
        case ERROR:   levelStr = LEVEL_STR_ERROR; break;
        case FATAL:   levelStr = LEVEL_STR_FATAL; break;
        }
        std::lock_guard<std::mutex> lock(logMtx);
        (*out) << "[" << boost::posix_time::microsec_clock::local_time()
            << "] [" << levelStr << "] [" << filename << ":" << lineno << ":"
            << functionName << "] " << message << std::endl;
    }

private:
    std::fstream fileStream;
    std::ostream *out;
    std::mutex logMtx;
    static const char * LEVEL_STR_DEBUG;
    static const char * LEVEL_STR_INFO;
    static const char * LEVEL_STR_WARNING;
    static const char * LEVEL_STR_ERROR;
    static const char * LEVEL_STR_FATAL;
};

const char * OStreamLogSink::LEVEL_STR_DEBUG = "debug";
const char * OStreamLogSink::LEVEL_STR_INFO = "info";
const char * OStreamLogSink::LEVEL_STR_WARNING = "warning";
const char * OStreamLogSink::LEVEL_STR_ERROR = "error";
const char * OStreamLogSink::LEVEL_STR_FATAL = "fatal";

/**
 * Log sink to write log messages to syslog.
 */
class SyslogLogSink : public LogSink {
public:
    SyslogLogSink(const std::string& name) : syslog_name(name) {
        openlog(syslog_name.c_str(), LOG_CONS | LOG_PID, LOG_DAEMON);
    }
    ~SyslogLogSink() {
        closelog();
    }

    void write(LogLevel level, const char *filename, int lineno,
               const char *functionName, const std::string& message) {
        int priority = LOG_DEBUG;
        switch (level) {
        case DEBUG:   priority = LOG_DEBUG; break;
        case INFO:    priority = LOG_INFO; break;
        case WARNING: priority = LOG_WARNING; break;
        case ERROR:   priority = LOG_ERR; break;
        case FATAL:   priority = LOG_CRIT; break;
        }
        syslog(priority,
               "[%s:%d:%s] %s",
               filename, lineno, functionName, message.c_str());
    }

private:
    std::string syslog_name;
};

static OStreamLogSink consoleLogSink(std::cout);
static LogSink * currentLogSink = &consoleLogSink;

LogSink * getLogSink() {
    return currentLogSink;
}

OFLogHandler::Level lgoStrToLevel(const std::string& newLevelstr) {
    OFLogHandler::Level level = OFLogHandler::INFO;

    std::string levelstr = newLevelstr;
    std::transform(levelstr.begin(), levelstr.end(),
                   levelstr.begin(), ::tolower);

    if (levelstr == "debug" || levelstr == "debug0") {
        level = OFLogHandler::DEBUG0;
        logLevel = DEBUG;
    } else if (levelstr == "debug1") {
        level = OFLogHandler::DEBUG1;
        logLevel = DEBUG;
    } else if (levelstr == "debug2") {
        level = OFLogHandler::DEBUG2;
        logLevel = DEBUG;
    } else if (levelstr == "debug3") {
        level = OFLogHandler::DEBUG3;
        logLevel = DEBUG;
    } else if (levelstr == "debug4") {
        level = OFLogHandler::DEBUG4;
        logLevel = DEBUG;
    } else if (levelstr == "debug5") {
        level = OFLogHandler::DEBUG5;
        logLevel = DEBUG;
    } else if (levelstr == "debug6") {
        level = OFLogHandler::DEBUG6;
        logLevel = DEBUG;
    } else if (levelstr == "debug7") {
        level = OFLogHandler::DEBUG7;
        logLevel = DEBUG;
    } else if (levelstr == "trace") {
        level = OFLogHandler::TRACE;
        logLevel = DEBUG;
    } else if (levelstr == "info") {
        level = OFLogHandler::INFO;
        logLevel = INFO;
    } else if (levelstr == "warning") {
        level = OFLogHandler::WARNING;
        logLevel = WARNING;
    } else if (levelstr == "error") {
        level = OFLogHandler::ERROR;
        logLevel = ERROR;
    } else if (levelstr == "fatal") {
        level = OFLogHandler::FATAL;
        logLevel = FATAL;
    }

    return level;
}

#ifdef RENDERER_VPP
VOM::log_level_t agentLevelToVom(OFLogHandler::Level level) {
    switch (level)
    {
    case OFLogHandler::DEBUG0:
    case OFLogHandler::DEBUG1:
    case OFLogHandler::DEBUG2:
    case OFLogHandler::DEBUG3:
    case OFLogHandler::DEBUG4:
    case OFLogHandler::DEBUG5:
    case OFLogHandler::DEBUG6:
    case OFLogHandler::DEBUG7:
        return (VOM::log_level_t::DEBUG);
    case OFLogHandler::TRACE:
    case OFLogHandler::INFO:
        return (VOM::log_level_t::INFO);
    case OFLogHandler::WARNING:
        return (VOM::log_level_t::WARNING);
    case OFLogHandler::ERROR:
        return (VOM::log_level_t::ERROR);
    case OFLogHandler::FATAL:
        return (VOM::log_level_t::CRITICAL);
    }
    return (VOM::log_level_t::INFO);
}
#endif

void initLogging(const std::string& levelstr,
                 bool toSyslog,
                 const std::string& log_file,
                 const std::string& syslog_name) {
    OFLogHandler::Level level = lgoStrToLevel(levelstr);

    if (toSyslog) {
        currentLogSink = new SyslogLogSink(syslog_name);
    } else if (!log_file.empty()) {
        currentLogSink = new OStreamLogSink(log_file);
    }
    OFLogHandler::registerHandler(logHandler);

    logHandler.setLevel(level);
#ifdef RENDERER_OVS
    /* No good way to redirect OVS logs to our logs, suppress them for now */
    vlog_set_levels(NULL, VLF_ANY_DESTINATION, VLL_OFF);
#endif
#ifdef RENDERER_VPP
    /*
     * VPP uses boost for logging. Configure the desired properties for
     * the boost log lib
     */
    if (!log_file.empty())
    {
        VOM::logger().set(log_file);
    }
    VOM::logger().set(agentLevelToVom(level));
#endif
}

void setLoggingLevel(const std::string& newLevelstr) {
    OFLogHandler::Level level = lgoStrToLevel(newLevelstr);
#ifdef RENDERER_OVS
    logHandler.setLevel(level);
#endif
#ifdef RENDERER_VPP
    VOM::logger().set(agentLevelToVom(level));
#endif
}

} /* namespace ovsagent */
