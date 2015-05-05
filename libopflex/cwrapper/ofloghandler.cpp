/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ofobjectlistener.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif


#include <opflex/logging/OFLogHandler.h>
#include <opflex/c/ofloghandler_c.h>

namespace opflex {
namespace logging {

class COFLogHandler : public OFLogHandler {
public:
    COFLogHandler(Level level, loghandler_p callback)
        : OFLogHandler(level), callback_(callback) {}

    virtual ~COFLogHandler() {}

    virtual void handleMessage(Logger const & logger) {
        callback_(
                logger.file_,
                logger.line_,
                logger.function_,
                logger.level_,
                logger.message().c_str()
                );
    }

    void setCallback(loghandler_p callback) {
        callback_ = callback;
    }

    loghandler_p callback_;
};

static COFLogHandler logHandler(opflex::logging::OFLogHandler::Level(OFLogHandler::NO_LOGGING), NULL);

}}

ofstatus ofloghandler_register(int level, loghandler_p callback) {
    if (callback == NULL)
        return OF_EINVALID_ARG;

    opflex::logging::logHandler.setCallback(callback);
    opflex::logging::logHandler.setLevel(opflex::logging::OFLogHandler::Level(level));
    opflex::logging::OFLogHandler::registerHandler(opflex::logging::logHandler);
    return OF_ESUCCESS;
}
