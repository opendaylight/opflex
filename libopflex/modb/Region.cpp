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

#include "opflex/modb/internal/Region.h"
#include "opflex/modb/internal/ObjectStore.h"
#include "LockGuard.h"

namespace opflex {
namespace modb {

using std::string;
using std::vector;
using std::pair;
using boost::shared_ptr;
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

shared_ptr<const ObjectInstance> Region::get(const URI& uri) {
    LockGuard guard(&region_mutex);
    return uri_map.at(uri);
}

void Region::put(class_id_t class_id, const URI& uri, 
                 const shared_ptr<const ObjectInstance>& oi) {
    LockGuard guard(&region_mutex);
    try {
        ClassIndex& ci = class_map.at(class_id);
        uri_map[uri] = oi;
        ci.addInstance(uri);
    } catch (std::out_of_range e) {
        throw std::out_of_range("Unknown class ID");
    }
}

bool Region::remove(class_id_t class_id, const URI& uri) {
    LockGuard guard(&region_mutex);
    ClassIndex& ci = class_map.at(class_id);
    ci.delInstance(uri);
    return (0 != uri_map.erase(uri));
}

void Region::addChild(class_id_t parent_class, 
                      const URI& parent_uri, 
                      prop_id_t parent_prop,
                      class_id_t child_class, 
                      const URI& child_uri) {
    LockGuard guard(&region_mutex);
    ClassIndex& ci = class_map.at(child_class);
    ci.addChild(parent_uri, parent_prop, child_uri);
}

bool Region::delChild(class_id_t parent_class,
                      const URI& parent_uri, 
                      prop_id_t parent_prop,
                      class_id_t child_class,
                      const URI& child_uri) {
    LockGuard guard(&region_mutex);
    ClassIndex& ci = class_map.at(child_class);
    return ci.delChild(parent_uri, parent_prop, child_uri);
}

void Region::clearChildren(class_id_t parent_class,
                           const URI& parent_uri, 
                           prop_id_t parent_prop,
                           class_id_t child_class) {
    LockGuard guard(&region_mutex);
    ClassIndex& ci = class_map.at(child_class);
    ci.clearChildren(parent_uri, parent_prop);
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

const std::pair<URI, prop_id_t>& Region::getParent(class_id_t child_class,
                                                   const URI& child) {
    LockGuard guard(&region_mutex);
    ClassIndex& ci = class_map.at(child_class);
    return ci.getParent(child);
}

} /* namespace modb */
} /* namespace opflex */
