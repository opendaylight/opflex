/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _____COMMS__INCLUDE__OPFLEX__RPC__SEND_HANDLER_HPP
#define _____COMMS__INCLUDE__OPFLEX__RPC__SEND_HANDLER_HPP

#include <rapidjson/rapidjson.h>

#include <rapidjson/encodings.h>

#include <deque>

namespace yajr {
namespace internal {

bool isLegitPunct(int c);

/**
 * Generic string queue
 * @tparam Encoding String encoding
 */
template <typename Encoding = rapidjson::UTF8<> >
struct GenericStringQueue {

    /** Character */
    typedef typename Encoding::Ch Ch;

    /** add char to queue */
    void Put(Ch c) {
        deque_.push_back(c);
        assert(::yajr::internal::isLegitPunct(c));
    }

    /** Flush the buffer */
    void Flush() {}

    /** Clear the buffer */
    void Clear() {
        deque_.clear();
    }

    /** Shrink to fit */
    void ShrinkToFit() {
        deque_.shrink_to_fit();
    }

    /**
     * Size of string queue
     * @return size
     */
    size_t GetSize() const {
        return deque_.size();
    }

    /** deque of chars */
    std::deque<Ch> deque_;
};

//! String buffer with UTF8 encoding
typedef GenericStringQueue<rapidjson::UTF8<> > StringQueue;

} /* yajr::internal namespace */
} /* yajr namespace */

#endif /* _____COMMS__INCLUDE__OPFLEX__RPC__SEND_HANDLER_HPP */

