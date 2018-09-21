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

#include <yajr/internal/buffers.hpp>

namespace yajr {
    namespace internal {

bool isLegitPunct(int c);

template <typename Encoding = rapidjson::UTF8<> >
struct GenericStringQueue {

    typedef typename Encoding::Ch Ch;

    void Put(Ch c) {
        buffers_.append(c);
        assert(::yajr::internal::isLegitPunct(c));
    }

    void Flush() {}

    void Clear() {
        buffers_.clear();
    }

    void ShrinkToFit() {
        buffers_.shrink_to_fit();
    }

    size_t GetSize() const {
        return buffers_.size();
    }

    Buffers<Ch, mm::Allocator<Ch> > buffers_;
#ifdef EXTRA_CHECKS
    void const * cP_;
#endif
};

//! String buffer with UTF8 encoding
typedef GenericStringQueue<rapidjson::UTF8<> > StringQueue;

} /* yajr::internal namespace */
} /* yajr namespace */

#endif /* _____COMMS__INCLUDE__OPFLEX__RPC__SEND_HANDLER_HPP */

