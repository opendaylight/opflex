/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Macros for logging
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

#include "config.h"
#if defined(HAVE_BOOST_LOG) && defined(HAVE_BOOST_LOG_SETUP) \
    && !defined(NO_BOOST_LOG)
#define USE_BOOST_LOG
#endif

#ifdef USE_BOOST_LOG
#include <boost/log/trivial.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#else
#include <sstream>
#include <syslog.h>
#endif

namespace ovsagent {

#ifdef USE_BOOST_LOG
static const boost::log::trivial::severity_level DEBUG   = boost::log::trivial::debug;
static const boost::log::trivial::severity_level INFO    = boost::log::trivial::info;
static const boost::log::trivial::severity_level WARNING = boost::log::trivial::warning;
static const boost::log::trivial::severity_level ERROR   = boost::log::trivial::error;
static const boost::log::trivial::severity_level FATAL   = boost::log::trivial::fatal;

#define LOG(lvl) BOOST_LOG_STREAM_WITH_PARAMS(::boost::log::trivial::logger::get(),\
                                              (::boost::log::keywords::severity = lvl))\
    << "[" << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "] "
#else
static const int DEBUG   = LOG_DEBUG;
static const int INFO    = LOG_INFO;
static const int WARNING = LOG_WARNING;
static const int ERROR   = LOG_ERR;
static const int FATAL   = LOG_CRIT;

extern int logLevel;

// Fall back to logging to standard out in a format that systemd can
// understand
class Logger {
public:
    ~Logger() {
        std::cout << buffer_.str() << std::endl;
    }

    std::ostream& stream() { return buffer_; }

    std::ostringstream buffer_;

};
#define LOG(lvl)                                                        \
    if (lvl <= logLevel)                                                \
        ovsagent::Logger().stream() << "<" << lvl << "> "               \
                         << "[" << __FILE__ << ":" << __LINE__          \
                         << ":" << __FUNCTION__ << "] "
#endif

} /* namespace ovsagent */

#endif /* AGENT_LOGGING_H */
