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

#include <boost/log/trivial.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/global_logger_storage.hpp>

namespace ovsagent {

static const boost::log::trivial::severity_level DEBUG   = boost::log::trivial::debug;
static const boost::log::trivial::severity_level INFO    = boost::log::trivial::info;
static const boost::log::trivial::severity_level WARNING = boost::log::trivial::warning;
static const boost::log::trivial::severity_level ERROR   = boost::log::trivial::error;
static const boost::log::trivial::severity_level FATAL   = boost::log::trivial::fatal;

#define LOG(lvl) BOOST_LOG_STREAM_WITH_PARAMS(::boost::log::trivial::logger::get(),\
                                              (::boost::log::keywords::severity = lvl))\
    << "[" << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "] "

} /* namespace ovsagent */

#endif /* AGENT_LOGGING_H */
