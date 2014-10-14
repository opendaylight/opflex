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

#include <utility>
#include <stdexcept>

#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include "opflex/ofcore/OFFramework.h"
#include "opflex/modb/Mutator.h"
#include "opflex/modb/mo-internal/StoreClient.h"
#include "opflex/modb/internal/ObjectStore.h"

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

typedef unordered_set<pair<class_id_t, URI> > uri_set_t;
typedef unordered_map<prop_id_t, uri_set_t> prop_uri_map_t;
typedef unordered_map<pair<class_id_t, URI>, prop_uri_map_t> uri_prop_uri_map_t;
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
    unordered_map<URI, class_id_t> notifs;
    obj_map_t::iterator objit;
    for (objit = pimpl->obj_map.begin(); 
         objit != pimpl->obj_map.end(); ++objit) {
        pimpl->client.put(objit->second->getClassId(), objit->first, 
                          objit->second);
        pimpl->client.queueNotification(objit->second->getClassId(),
                                 objit->first,
                                 notifs);
    }
    uri_prop_uri_map_t::iterator upit;
    for (upit = pimpl->added_children.begin(); 
         upit != pimpl->added_children.end(); ++upit) {
        prop_uri_map_t::iterator pit;
        for (pit = upit->second.begin(); pit != upit->second.end(); ++pit) {
            uri_set_t::iterator uit;
            for (uit = pit->second.begin(); uit != pit->second.end(); ++uit) {
                pimpl->client.addChild(uit->first, uit->second, pit->first,
                                upit->first.first, upit->first.second);
                pimpl->client.queueNotification(upit->first.first,
                                         upit->first.second,
                                         notifs);

            }
        }

    }
    unordered_set<pair<class_id_t, URI> >::iterator rit;
    for (rit = pimpl->removed_objects.begin(); 
         rit != pimpl->removed_objects.end(); ++rit) {
        pimpl->client.remove(rit->first, rit->second, false);
        pimpl->client.queueNotification(rit->first, rit->second, notifs);
    }
    pimpl->client.deliverNotifications(notifs);
}

} /* namespace modb */
} /* namespace opflex */
