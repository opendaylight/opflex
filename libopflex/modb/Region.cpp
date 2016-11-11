/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for Region class.
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

#include <boost/foreach.hpp>

#include "opflex/modb/internal/Region.h"
#include "opflex/modb/internal/ObjectStore.h"
#include "LockGuard.h"

namespace opflex {
namespace modb {

using std::string;
using std::vector;
using std::pair;
using std::make_pair;
using opflex::util::LockGuard;
using mointernal::ObjectInstance;

Region::Region(ObjectStore* parent, string owner_)
    : client(parent, this), owner(owner_) {
    uv_mutex_init(&region_mutex);
}

Region::~Region() {
    uv_mutex_destroy(&region_mutex);
}

void Region::addClass(const ClassInfo& class_info) {
    class_map[class_info.getId()];
}

bool Region::isPresent(const URI& uri) {
    LockGuard guard(&region_mutex);
    return uri_map.find(uri) != uri_map.end();
}

OF_SHARED_PTR<const ObjectInstance> Region::get(const URI& uri) {
    LockGuard guard(&region_mutex);
    return uri_map.at(uri);
}

bool Region::get(const URI& uri,
                 /*out*/ OF_SHARED_PTR<const ObjectInstance>& oi) {
    LockGuard guard(&region_mutex);
    uri_map_t::const_iterator itr = uri_map.find(uri);
    if (itr != uri_map.end()) {
        oi = itr->second;
        return true;
    }
    return false;
}

void Region::put(class_id_t class_id, const URI& uri,
                 const OF_SHARED_PTR<const ObjectInstance>& oi) {
    LockGuard guard(&region_mutex);
    try {
        ClassIndex& ci = class_map.at(class_id);
        uri_map[uri] = oi;
        ci.addInstance(uri);
        if (!ci.hasParent(uri)) roots.insert(make_pair(class_id, uri));
    } catch (std::out_of_range e) {
        throw std::out_of_range("Unknown class ID");
    }
}

bool Region::putIfModified(class_id_t class_id, const URI& uri,
                           const OF_SHARED_PTR<const ObjectInstance>& oi) {
    LockGuard guard(&region_mutex);
    try {
        ClassIndex& ci = class_map.at(class_id);
        uri_map_t::iterator it = uri_map.find(uri);
        bool result = true;
        if (it != uri_map.end()) {
            if (*oi != *it->second) {
                it->second = oi;
            } else {
                result = false;
            }
        } else {
            uri_map[uri] = oi;
            ci.addInstance(uri);
        }

        if (!ci.hasParent(uri)) roots.insert(make_pair(class_id, uri));
        return result;
    } catch (std::out_of_range e) {
        throw std::out_of_range("Unknown class ID");
    }
}

bool Region::remove(class_id_t class_id, const URI& uri) {
    LockGuard guard(&region_mutex);
    ClassIndex& ci = class_map.at(class_id);
    ci.delInstance(uri);
    roots.erase(make_pair(class_id, uri));
    return (0 != uri_map.erase(uri));
}

bool Region::addChild(class_id_t parent_class,
                      const URI& parent_uri,
                      prop_id_t parent_prop,
                      class_id_t child_class,
                      const URI& child_uri) {
    LockGuard guard(&region_mutex);
    obj_set_t::iterator it = roots.find(make_pair(child_class, child_uri));
    if (it != roots.end())
        roots.erase(it);
    ClassIndex& ci = class_map.at(child_class);
    return ci.addChild(parent_uri, parent_prop, child_uri);
}

bool Region::delChild(class_id_t parent_class,
                      const URI& parent_uri,
                      prop_id_t parent_prop,
                      class_id_t child_class,
                      const URI& child_uri) {
    LockGuard guard(&region_mutex);
    ClassIndex& ci = class_map.at(child_class);
    bool r = ci.delChild(parent_uri, parent_prop, child_uri);
    if (uri_map.find(child_uri) != uri_map.end() && !ci.hasParent(child_uri))
        roots.insert(make_pair(child_class, child_uri));
    return r;
}

void Region::clearChildren(class_id_t parent_class,
                           const URI& parent_uri,
                           prop_id_t parent_prop,
                           class_id_t child_class) {
    LockGuard guard(&region_mutex);
    ClassIndex& ci = class_map.at(child_class);
    vector<URI> children;
    ci.getChildren(parent_uri, parent_prop, children);

    BOOST_FOREACH(const URI& child_uri, children) {
        delChild(parent_class, parent_uri, parent_prop,
                 child_class, child_uri);
    }
}

void Region::getChildren(class_id_t parent_class,
                         const URI& parent_uri,
                         prop_id_t parent_prop,
                         class_id_t child_class,
                         /* out */ vector<URI>& output) {
    LockGuard guard(&region_mutex);
    ClassIndex& ci = class_map.at(child_class);
    ci.getChildren(parent_uri, parent_prop, output);
}

std::pair<URI, prop_id_t> Region::getParent(class_id_t child_class,
                                            const URI& child) {
    LockGuard guard(&region_mutex);
    ClassIndex& ci = class_map.at(child_class);
    return ci.getParent(child);
}

bool Region::getParent(class_id_t child_class, const URI& child,
                       /* out */ std::pair<URI, prop_id_t>& parent) {
    LockGuard guard(&region_mutex);
    class_map_t::const_iterator citr = class_map.find(child_class);
    return citr != class_map.end() ? citr->second.getParent(child, parent)
                                   : false;
}

void Region::getRoots(/* out */ obj_set_t& output) {
    LockGuard guard(&region_mutex);
    output.insert(roots.begin(), roots.end());
}

void Region::getObjectsForClass(class_id_t class_id,
                                /* out */ OF_UNORDERED_SET<URI>& output) {
    LockGuard guard(&region_mutex);
    ClassIndex& ci = class_map.at(class_id);
    ci.getAll(output);
}

} /* namespace modb */
} /* namespace opflex */
