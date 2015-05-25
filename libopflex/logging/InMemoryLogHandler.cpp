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

#include <LockGuard.hpp>

#include <boost/lexical_cast.hpp>

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
        from_->to_ = from + size - 2;
        from_->cursor_ = buffer();
        from_->full_ = 0;
        from[size-1] = '\0';
    }

InMemoryLogHandler::~InMemoryLogHandler() {
    uv_mutex_destroy(&bufferMutex_);
}

void InMemoryLogHandler::write(char const * source, size_t size) {

    size_t burst;

    /* TODO: optimize for large writes that wrap around more than once */

    do {
        burst = std::min<size_t>(size, (from_->to_ - from_->cursor_));

        /* not thread safe! */
        memcpy(from_->cursor_, source, burst);
        size -= burst;
        source += burst;
        if (size) {
            from_->cursor_ = buffer();
            from_->full_ = 255;
        } else {
            from_->cursor_ += burst;
        }

    } while (size);

}

void InMemoryLogHandler::write(std::string const str) {

    write(str.data(), str.size());

}

void InMemoryLogHandler::write(char c) {

    write(&c, 1);

}

void InMemoryLogHandler::write(int i) {

    write(boost::lexical_cast<std::string>(i));

}

void InMemoryLogHandler::write(char * cstr) {

    write(cstr, strlen(cstr));

}

void InMemoryLogHandler::handleMessage(Logger const & logger) {
    if (logger.level_ < logLevel_) {
        return;
    }

    util::LockGuard guard(&bufferMutex_);

    static char kSEPARATOR = ':';

    write(logger.level_);
    write(kSEPARATOR);
    write(logger.file_);
    write(kSEPARATOR);
    write(logger.line_);
    write(kSEPARATOR);
    write(logger.function_);
    write(kSEPARATOR);
    write(logger.message());

    static char const kNL = '\n';

    write(kNL);

}

void InMemoryLogHandler::dump(std::deque<char> & dq) const {

    if (from_->full_) {
        dq.insert(dq.end(), from_->cursor_, from_->to_);
    }
    dq.insert(dq.end(), buffer(), from_->cursor_);

}

} /* namespace logging */
} /* namespace opflex */


