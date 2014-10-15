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

void ClassIndex::addChild(const URI& parent, prop_id_t parent_prop, 
                          const URI& child) {
    uri_prop_map_t::iterator result = parent_map.find(child);
    if (result != parent_map.end()) {
        delChild(result->second.first, result->second.second, child);
    }
    child_map[parent][parent_prop].insert(child);
    parent_map.insert(std::make_pair(child, std::make_pair(parent,parent_prop)));
}

bool ClassIndex::delChild(const URI& parent, prop_id_t parent_prop, 
                          const URI& child) {
    try {
        prop_uri_map_t& pmap = child_map.at(parent);
        uri_set_t& uset = pmap.at(parent_prop);
        bool removed = uset.erase(child);
        if (uset.size() == 0) {
            pmap.erase(parent_prop);
            if (pmap.size() == 0)
                child_map.erase(parent);
        }

        parent_map.erase(child);

        return removed;
    } catch (std::out_of_range) {
        return false;
    }
}

void ClassIndex::clearChildren(const URI& parent, prop_id_t parent_prop) {
    try {
        prop_uri_map_t& pmap = child_map.at(parent);
        uri_set_t& uset = pmap.at(parent_prop);
        uri_set_t::iterator it;

        for (it = uset.begin(); it != uset.end(); ++it) {
            try {
                parent_map.erase(*it);
            } catch (std::out_of_range) { }
        }
        pmap.erase(parent_prop);
        if (pmap.size() == 0)
            child_map.erase(parent);
    } catch (std::out_of_range) {
        return;
    }
}

void ClassIndex::getChildren(const URI& parent, prop_id_t parent_prop,
                             std::vector<URI>& output) const {
    try {
        const uri_set_t& uset = child_map.at(parent).at(parent_prop);
        uri_set_t::iterator it;
        for (it = uset.begin(); it != uset.end(); ++it) {
            output.push_back(*it);
        }
    } catch (std::out_of_range) {
        return;
    }
}

const std::pair<URI, prop_id_t>& ClassIndex::getParent(const URI& child) const {
    return parent_map.at(child);
}

} /* namespace modb */
} /* namespace opflex */
