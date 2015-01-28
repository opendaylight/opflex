/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for Mutator class.
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
#include <stdexcept>

#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>

#include "opflex/ofcore/OFFramework.h"
#include "opflex/modb/Mutator.h"
#include "opflex/modb/mo-internal/StoreClient.h"
#include "opflex/modb/internal/ObjectStore.h"
#include "opflex/logging/internal/logging.hpp"

namespace opflex {
namespace modb {

using boost::unordered_map;
using boost::unordered_set;
using boost::shared_ptr;
using boost::make_shared;
using std::pair;
using std::make_pair;

using mointernal::StoreClient;
using mointernal::ObjectInstance;

typedef unordered_set<reference_t > uri_set_t;
typedef unordered_map<prop_id_t, uri_set_t> prop_uri_map_t;
typedef unordered_map<reference_t, prop_uri_map_t> uri_prop_uri_map_t;
typedef unordered_map<URI, shared_ptr<ObjectInstance> > obj_map_t;

class Mutator::MutatorImpl {
public:
    MutatorImpl(ofcore::OFFramework& framework_,
                const std::string& owner)
        : framework(framework_),
          client(framework.getStore().getStoreClient(owner)) { }
    MutatorImpl(const std::string& owner)
        : framework(ofcore::OFFramework::defaultInstance()),
          client(framework.getStore().getStoreClient(owner)) { }

    ofcore::OFFramework& framework;
    StoreClient& client;

    // modified objects
    obj_map_t obj_map;
    
    // removed objects
    unordered_set<pair<class_id_t, URI> > removed_objects;

    // added children
    uri_prop_uri_map_t added_children;
};

/**
 * Compute a hash value for a class_id/URI pair, making it suitable as
 * a key in an unordered_map
 */
size_t hash_value(pair<class_id_t, URI> const& p) {
    std::size_t seed = 0;
    boost::hash_combine(seed, p.first);
    boost::hash_combine(seed, p.second);
    return seed;
}

Mutator::Mutator(const std::string& owner) 
    : pimpl(new MutatorImpl(owner)) {
    pimpl->framework.registerTLMutator(*this);
}

Mutator::Mutator(ofcore::OFFramework& framework, 
                 const std::string& owner) 
    : pimpl(new MutatorImpl(framework, owner)) {
    pimpl->framework.registerTLMutator(*this);
}

Mutator::~Mutator() {
    pimpl->framework.clearTLMutator();
    delete pimpl;
}

shared_ptr<ObjectInstance>& Mutator::addChild(class_id_t parent_class,
                                              const URI& parent_uri, 
                                              prop_id_t parent_prop,
                                              class_id_t child_class,
                                              const URI& child_uri) {
    pimpl->added_children
        [make_pair(child_class,child_uri)]
        [parent_prop].insert(make_pair(parent_class,parent_uri));
    return modify(child_class, child_uri);
}

shared_ptr<ObjectInstance>& Mutator::modify(class_id_t class_id,
                                            const URI& uri) {
    // check for copy in mutator
    obj_map_t::iterator it = pimpl->obj_map.find(uri);
    if (it != pimpl->obj_map.end()) return it->second;
    shared_ptr<ObjectInstance> copy;
    try {
        // check for existing object
        copy = make_shared<ObjectInstance>(*pimpl->client.get(class_id,
                                                              uri).get());
    } catch (std::out_of_range e) {
        // create new object
        copy = make_shared<ObjectInstance>(class_id);
    }

    pair<obj_map_t::iterator, bool> r = 
        pimpl->obj_map.insert(obj_map_t::value_type(uri, copy));
    return r.first->second;
}

void Mutator::remove(class_id_t class_id, const URI& uri) {
    pimpl->removed_objects.insert(make_pair(class_id, uri));
}

void Mutator::commit() {
    StoreClient::notif_t raw_notifs;
    StoreClient::notif_t notifs;
    BOOST_FOREACH(obj_map_t::value_type& objt, pimpl->obj_map) {
        pimpl->client.put(objt.second->getClassId(), objt.first, 
                          objt.second);
        raw_notifs[objt.first] = objt.second->getClassId();

    }
    BOOST_FOREACH(uri_prop_uri_map_t::value_type& upt, pimpl->added_children) {
        BOOST_FOREACH(prop_uri_map_t::value_type& pt, upt.second) {
            BOOST_FOREACH(const reference_t& ut, pt.second) {
                raw_notifs[upt.first.second] = upt.first.first;
                pimpl->client.addChild(ut.first, ut.second, pt.first,
                                upt.first.first, upt.first.second);

            }
        }
    }

    BOOST_FOREACH(StoreClient::notif_t::value_type nt, raw_notifs) {
        pimpl->client.queueNotification(nt.second, nt.first, notifs);
    }

    BOOST_FOREACH(const reference_t& rt, pimpl->removed_objects) {
        pimpl->client.remove(rt.first, rt.second, false);
        pimpl->client.queueNotification(rt.first, rt.second, notifs);
    }

    pimpl->obj_map.clear();
    pimpl->removed_objects.clear();
    pimpl->added_children.clear();

    pimpl->client.deliverNotifications(notifs);
}

} /* namespace modb */
} /* namespace opflex */
