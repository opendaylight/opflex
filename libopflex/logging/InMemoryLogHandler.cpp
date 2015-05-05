/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for StdOutLogHandler class.
 *
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <opflex/logging/InMemoryLogHandler.hpp>

#include <InlineLockGuard.hpp>

#include <algorithm>
#include <iostream>

#include <cstring>

namespace opflex {
namespace logging {

InMemoryLogHandler::InMemoryLogHandler(
        Level logLevel,
        char * from,
        size_t size
    )
    :
        OFLogHandler(logLevel),
        from_(reinterpret_cast<Buffer * const>(from))
    {
        uv_mutex_init(&bufferMutex_);
        from_->to_ = from + size - 1;
        from_->begin_ = buffer();
        from_->end_ = from_->begin_;
        from[size] = '\0';
    }

InMemoryLogHandler::~InMemoryLogHandler() {
    uv_mutex_destroy(&bufferMutex_);
}

void InMemoryLogHandler::write(char const * source, size_t size) {

    size_t burst;

    /* TODO: optimize for large writes that wrap around more than once */

    do {
        burst = std::min<size_t>(size, (from_->to_ - from_->end_));

        /* not thread safe! */
        memcpy(from_->end_, source, burst);
        size -= burst;
        source += burst;
        if (size) {
            from_->end_ = buffer();
        } else {
            from_->end_ += burst;
        }
    } while (size);

}

void InMemoryLogHandler::write(std::string const str) {

    write(str.data(), str.size());

    static char const nul = '\0';

    write(&nul, sizeof(nul));
}

void InMemoryLogHandler::handleMessage(Logger const & logger) {
    if (logger.level_ < logLevel_) {
        return;
    }

    util::InlineLockGuard guard(&bufferMutex_);

    write(reinterpret_cast<char const *>(&static_cast<LoggingTag const &>(logger)), sizeof(LoggingTag));
    write(logger.message());
}

} /* namespace logging */
} /* namespace opflex */


