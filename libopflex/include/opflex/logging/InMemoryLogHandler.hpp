/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file InMemoryHandler.h
 * @brief Interface definition file for InMemoryHandler
 */
/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _INCLUDE__OPFLEX__LOGGING__INMEMORYLOGHANDLER_HPP
#define _INCLUDE__OPFLEX__LOGGING__INMEMORYLOGHANDLER_HPP

#include <uv.h>

#include <opflex/logging/OFLogHandler.h>

#include <deque>

namespace opflex {
namespace logging {

/**
 * An @ref OFLogHandler that simply logs to standard output.
 */
class InMemoryLogHandler : public OFLogHandler {
  public:
    /**
     * Allocate a log handler that will log any messages with equal or
     * greater severity than the specified log level.
     * 
     * @param logLevel the minimum log level
     */
    InMemoryLogHandler(Level logLevel, char * from, size_t size)
        __attribute__((no_instrument_function));
    virtual ~InMemoryLogHandler()
        __attribute__((no_instrument_function));

    /* see OFLogHandler */
    virtual void handleMessage(Logger const & logger)
        __attribute__((no_instrument_function));

    struct Buffer {
        char * to_;
        char * cursor_;
        char full_;
        char circular_[];
    };

    void write(char const * source, size_t size);
    void write(std::string const source);
    void write(char c);
    void write(int i);
    void write(char * cstr);

    char * buffer() const {
        return reinterpret_cast<char *>(from_)
            + sizeof(from_->to_)
            + sizeof(from_->cursor_)
            + sizeof(from_->full_)
        ;
    }

    void dump(std::deque<char> & dq) const;

  private:
    uv_mutex_t bufferMutex_;
    Buffer * const from_;
};

} /* namespace logging */
} /* namespace opflex */

#endif /* _INCLUDE__OPFLEX__LOGGING__INMEMORYLOGHANDLER_HPP */
