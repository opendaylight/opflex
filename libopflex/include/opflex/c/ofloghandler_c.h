/* -*- C -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file ofloghandler_c.h
 * @brief C wrapper for log handler
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "ofcore_c.h"

#pragma once
#ifndef OPFLEX_C_OFLOGHANDLER_H
//! @cond Doxygen_Suppress
#define OPFLEX_C_OFLOGHANDLER_H
//! @endcond

/**
 * @addtogroup cwrapper
 * @{
 * @defgroup clogging Logging
 * Logging for the OpFlex framework.
 * @{
 */

/**
 * @defgroup cofloghandler Log Handler
 * A log message handler for the OpFlex framework.  You will need to
 * create a log handler and register it if you want to want to log to
 * your custom handler.
 *
 * By default, the OpFlex Framework will simply log to standard out at
 * INFO or above, but you can override this behavior by registering a
 * customer log handler.
 *
 * @{
 */

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
typedef void (*loghandler_p)(const char* file, int line, 
                             const char* function, int level, 
                             const char* message);

/**
 * Trace log level
 */
#define LOG_TRACE 10

/**
 * Debug4 (lowest debug) log level
 */
#define LOG_DEBUG4 20

/**
 * Debug 3 log level
 */
#define LOG_DEBUG3 30

/**
 * Debug 2 log level
 */
#define LOG_DEBUG2 40

/**
 * Debug 1 (highest debug) log level
 */
#define LOG_DEBUG1 50

/**
 * Info log level
 */
#define LOG_INFO 60

/**
 * Warning log level
 */
#define LOG_WARNING 70

/**
 * Error log level
 */
#define LOG_ERROR 80

/**
 * Fatal log level
 */
#define LOG_FATAL 90

#ifdef __cplusplus
extern "C" {
#endif
    /**
     * Register a new log handler.  
     * 
     * @param level the minimum log level for log messages that you
     * want to recieve.
     * @param handler a function pointer to your handler function to be
     * invoked when the listener is notified.
     * @return a status code
     */
    ofstatus ofloghandler_register(int level, loghandler_p handler);

#ifdef __cplusplus
} /* extern "C" */
#endif

/** @} cofloghandler */
/** @} cmodb */
/** @} cwrapper */

#endif /* OPFLEX_C_OFLOGHANDLER_H */
