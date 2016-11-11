/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ClassIndex class.
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


#include <utility>

#include "opflex/modb/internal/ClassIndex.h"

namespace opflex {
namespace modb {

ClassIndex::ClassIndex() {

}

ClassIndex::~ClassIndex() { }


void ClassIndex::addInstance(const URI& uri) {
    instance_map.insert(uri);
}

void ClassIndex::delInstance(const URI& uri) {
    instance_map.erase(uri);
}

bool ClassIndex::addChild(const URI& parent, prop_id_t parent_prop, 
                          const URI& child) {
    uri_prop_map_t::iterator result = parent_map.find(child);
    if (result != parent_map.end()) {
        if (result->second.first == parent &&
            result->second.second == parent_prop &&
            result->first == child) {
            return false;
        } else {
            delChild(result->second.first, result->second.second, child);
        }
    }
    child_map[parent][parent_prop].insert(child);
    parent_map.insert(std::make_pair(child, std::make_pair(parent,parent_prop)));
    return true;
}

bool ClassIndex::delChild(const URI& parent, prop_id_t parent_prop, 
                          const URI& child) {
    uri_prop_uri_map_t::iterator cit = child_map.find(parent);
    if (cit == child_map.end()) return false;
    prop_uri_map_t& pmap = cit->second;

    prop_uri_map_t::iterator pit = pmap.find(parent_prop);
    if (pit == pmap.end()) return false;
    uri_set_t& uset = pit->second;

    bool removed = uset.erase(child);
    if (uset.size() == 0) {
        pmap.erase(parent_prop);
        if (pmap.size() == 0)
            child_map.erase(parent);
    }

    parent_map.erase(child);

    return removed;
}

void ClassIndex::getChildren(const URI& parent, prop_id_t parent_prop,
                             std::vector<URI>& output) const {
    uri_prop_uri_map_t::const_iterator cit = child_map.find(parent);
    if (cit == child_map.end()) return;
    const prop_uri_map_t& pmap = cit->second;

    prop_uri_map_t::const_iterator pit = pmap.find(parent_prop);
    if (pit == pmap.end()) return;
    const uri_set_t& uset = pit->second;

    uri_set_t::const_iterator it;
    for (it = uset.begin(); it != uset.end(); ++it) {
        output.push_back(*it);
    }
}

const std::pair<URI, prop_id_t>& ClassIndex::getParent(const URI& child) const {
    return parent_map.at(child);
}

bool ClassIndex::getParent(const URI& child,
                           /* out */ std::pair<URI, prop_id_t>& parent) const {
    uri_prop_map_t::const_iterator itr = parent_map.find(child);
    if (itr != parent_map.end()) {
        parent = itr->second;
        return true;
    }
    return false;
}

bool ClassIndex::hasParent(const URI& child) const {
    return parent_map.find(child) != parent_map.end();
}

void ClassIndex::getAll(uri_set_t& output) const {
    output.insert(instance_map.begin(), instance_map.end());
}

} /* namespace modb */
} /* namespace opflex */
