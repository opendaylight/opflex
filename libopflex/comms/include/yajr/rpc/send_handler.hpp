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

#ifdef PERFORM_CRAZY_BYTE_BY_BYTE_INVARIANT_CHECK
#include <opflex/logging/internal/logging.hpp>
#endif
namespace yajr {
    namespace internal {

bool __checkInvariants(void const *);
bool isLegitPunct(int c);

template <typename Encoding = rapidjson::UTF8<> >
struct GenericStringQueue {

    typedef typename Encoding::Ch Ch;

    void Put(Ch c) {
        deque_.push_back(c);
        assert(::yajr::internal::isLegitPunct(c));
#ifdef PERFORM_CRAZY_BYTE_BY_BYTE_INVARIANT_CHECK
        assert(__checkInvariants(cP_));
        if(deque_.back()!=c){
            LOG(ERROR)
                << "inserted char already changed: \""
                << c
                << "\"->\""
                << deque_.back()
                << "\""
            ;
        }
#endif
    }

    void Flush() {}

    void Clear() {
        deque_.clear();
    }

    void ShrinkToFit() {
        deque_.shrink_to_fit();
    }

    size_t GetSize() const {
        return deque_.size();
    }

    std::deque<Ch> deque_;
#ifndef NDEBUG
    void const * cP_;
#endif
};

//! String buffer with UTF8 encoding
typedef GenericStringQueue<rapidjson::UTF8<> > StringQueue;

} /* yajr::internal namespace */
} /* yajr namespace */

#endif /* _____COMMS__INCLUDE__OPFLEX__RPC__SEND_HANDLER_HPP */

