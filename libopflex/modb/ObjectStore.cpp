/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ObjectStore class.
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

#include <stdexcept>

#include <boost/foreach.hpp>

#include "opflex/modb/internal/ObjectStore.h"
#include "LockGuard.h"

namespace opflex {
namespace modb {

using mointernal::StoreClient;
using boost::unordered_set;

ObjectStore::ObjectStore(util::ThreadManager& threadManager_)
    : systemClient(this, NULL), readOnlyClient(this, NULL, true),
      notif_proc(this), notif_queue(&notif_proc, threadManager_) {
    uv_mutex_init(&listener_mutex);
}

ObjectStore::~ObjectStore() {
    stop();

    region_owner_map_t::const_iterator it;
    for (it = region_owner_map.begin();
         it != region_owner_map.end(); it++) {
        delete it->second;
    }

    uv_mutex_destroy(&listener_mutex);
}

void ObjectStore::init(const ModelMetadata& model) {
    std::vector<ClassInfo>::const_iterator it;
    for (it = model.getClasses().begin();
         it != model.getClasses().end();
         ++it) {

        if (region_owner_map.find(it->getOwner()) == region_owner_map.end())
            region_owner_map[it->getOwner()] = new Region(this, it->getOwner());

        Region* r = region_owner_map[it->getOwner()];
        r->addClass(*it);
        ClassContext& cc = class_map[it->getId()];
        cc.region = r;
        cc.classInfo = *it;
        class_name_map[cc.classInfo.getName()] = &cc.classInfo;

        ClassInfo::property_map_t::const_iterator pit;
        for (pit = cc.classInfo.getProperties().begin();
             pit != cc.classInfo.getProperties().end();
             ++pit) {
            prop_map[pit->second.getId()] = &cc.classInfo;
        }
    }
}

ObjectStore::NotifQueueProc::NotifQueueProc(ObjectStore* store_)
    : store(store_) {}

void ObjectStore::NotifQueueProc::processItem(const URI& uri,
                                              const boost::any& data) {
    util::LockGuard guard(&store->listener_mutex);
    std::list<ObjectListener*>::const_iterator it;
    class_id_t class_id = boost::any_cast<class_id_t>(data);
    std::list<ObjectListener*>& listeners =
        store->class_map.at(class_id).listeners;
    for (it = listeners.begin(); it != listeners.end(); ++it) {
        (*it)->objectUpdated(class_id, uri);
    }
}

const std::string& ObjectStore::NotifQueueProc::taskName() {
    static const std::string name("modb_notif");
    return name;
}

void ObjectStore::start() {
    notif_queue.start();
}

void ObjectStore::stop() {
    notif_queue.stop();
}

Region* ObjectStore::getRegion(const std::string& owner) {
    region_owner_map_t::iterator it = region_owner_map.find(owner);
    if (it == region_owner_map.end())
        throw std::out_of_range("No region with owner " + owner);
    return it->second;
}

Region* ObjectStore::getRegion(class_id_t class_id) {
    return class_map.at(class_id).region;
}

void ObjectStore::getOwners(/* out */ unordered_set<std::string>& output) {
    BOOST_FOREACH(const region_owner_map_t::value_type v, region_owner_map) {
        output.insert(v.first);
    }
}

StoreClient& ObjectStore::getReadOnlyStoreClient() {
    return readOnlyClient;
}

StoreClient& ObjectStore::getStoreClient(const std::string& owner) {
    if (owner == "_SYSTEM_") return systemClient;

    Region* r = getRegion(owner);
    return r->getStoreClient();
}

const ClassInfo& ObjectStore::getClassInfo(class_id_t class_id) const {
    return class_map.at(class_id).classInfo;
}

const ClassInfo& ObjectStore::getClassInfo(std::string class_name) const {
    return *class_name_map.at(class_name);
}

const ClassInfo& ObjectStore::getPropClassInfo(prop_id_t prop_id) const {
    return *prop_map.at(prop_id);
}

void ObjectStore::forEachClass(void (*apply)(void*, const ClassInfo&), void* data) {
    class_map_t::iterator it;
    for (it = class_map.begin(); it != class_map.end(); ++it) {
        apply(data, it->second.classInfo);
    }
}

void ObjectStore::registerListener(class_id_t class_id,
                                   ObjectListener* listener) {
    util::LockGuard guard(&listener_mutex);
    class_map.at(class_id).listeners.push_back(listener);
}

void ObjectStore::unregisterListener(class_id_t class_id,
                                     ObjectListener* listener) {
    util::LockGuard guard(&listener_mutex);
    class_map_t::iterator it = class_map.find(class_id);
    if (it == class_map.end()) return;
    it->second.listeners.remove(listener);
}

void ObjectStore::queueNotification(class_id_t class_id, const URI& uri) {
    notif_queue.queueItem(uri, class_id);
}

} /* namespace modb */
} /* namespace opflex */
