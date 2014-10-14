/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/logging/OFLogHandler.h"
#include "opflex/logging/internal/logging.hpp"

using opflex::logging::OFLogHandler;

namespace opflex {
namespace logging {
namespace internal {

Logger::~Logger() {
    OFLogHandler::getHandler()->handleMessage(file_,
                                              line_,
                                              function_,
                                              level_,
                                              buffer_.str());
}

} /* namespace internal */
} /* namespace logging */
} /* namespace opflex */
