/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _____COMMS__INCLUDE__OPFLEX__COMMS__INTERNAL__JSON_HPP
#define _____COMMS__INCLUDE__OPFLEX__COMMS__INTERNAL__JSON_HPP

#include <istream>

#include <rapidjson/rapidjson.h>

namespace yajr {
    namespace comms {
        namespace internal {
            namespace wrapper {

/* TODO: make an I/O wrapper, or resort to something else that
 * allows us to perform *in-situ* parsing... */

class IStreamWrapper {
  public:
    typedef char Ch;

    IStreamWrapper(std::istream& is)
        : is_(is)
        {}

    Ch Peek() const {
        int c = is_.peek();
        return c == std::char_traits<char>::eof() ? '\0' : (Ch)c;
    }

    Ch Take() {
        int c = is_.get();
        return c == std::char_traits<char>::eof() ? '\0' : (Ch)c;
    }

    size_t Tell() const {
        return (size_t)is_.tellg();
    }

    Ch* PutBegin() {
        assert(false);
        return 0;
    }

    void Put(Ch) {
        assert(false);
    }

    void Flush() {
        assert(false);
    }

    size_t PutEnd(Ch*) {
        assert(false);
        return 0;
    }

  private:
    IStreamWrapper(const IStreamWrapper&);
    IStreamWrapper& operator=(const IStreamWrapper&);

    std::istream& is_;
};

} /* yajr::comms::internal::wrapper namespace */
} /* yajr::comms::internal namespace */
} /* yajr::comms namespace */
} /* yajr namespace */

#endif /* _____COMMS__INCLUDE__OPFLEX__COMMS__INTERNAL__JSON_HPP */

