/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OVSAGENT_OFPBUF_H_
#define OVSAGENT_OFPBUF_H_
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
#endif

#include <openvswitch/ofpbuf.h>

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif

#ifdef __cplusplus
}

/**
 * Smart pointer wrapper to hold an ofpbuf
 */
class OfpBuf : public std::unique_ptr<struct ofpbuf,
                                      void(*)(struct ofpbuf*)> {
public:
    OfpBuf(size_t size): unique_ptr(ofpbuf_new(size), ofpbuf_delete) {}
    OfpBuf(struct ofpbuf* _buf): unique_ptr(_buf, ofpbuf_delete) {}

    void clear() { ofpbuf_clear(get()); }
    void reserve(size_t size) { ofpbuf_reserve(get(), size); }
    void* push_zeros(size_t size) { return ofpbuf_push_zeros(get(), size); }
    void* put_zeros(size_t size) { return ofpbuf_put_zeros(get(), size); }
    void* at_assert(size_t offset, size_t size) {
        return ofpbuf_at_assert(get(), offset, size);
    }
    void* data() const { return get()->data; }
    size_t size() const { return get()->size; }
};

#endif /* __cplusplus */

#endif /* OVSAGENT_OFPBUF_H_ */
