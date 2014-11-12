/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for Processor class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <time.h>
#include <uv.h>
#include <limits>

#include <boost/foreach.hpp>

#include "opflex/engine/internal/OpflexPEHandler.h"
#include "opflex/engine/internal/ProcessorMessage.h"
#include "opflex/engine/Processor.h"
#include "LockGuard.h"
#include "opflex/logging/internal/logging.hpp"

namespace opflex {
namespace engine {

using boost::shared_ptr;
using std::vector;
using std::pair;
using std::make_pair;
using modb::ObjectStore;
using modb::ClassInfo;
using modb::PropertyInfo;
using modb::class_id_t;
using modb::prop_id_t;
using modb::reference_t;
using modb::URI;
using modb::mointernal::StoreClient;
using modb::mointernal::ObjectInstance;
using modb::hash_value;

using namespace internal;

static const uint64_t LOCAL_REFRESH_RATE = 1000*60*30;
static const uint64_t DEFAULT_DELAY = 250;

size_t hash_value(pair<class_id_t, URI> const& p);

Processor::Processor(ObjectStore* store_)
    : AbstractObjectListener(store_),
      serializer(store_),
      pool(*this),
      proc_shouldRun(false),
      processingDelay(DEFAULT_DELAY) {
    uv_mutex_init(&item_mutex);
    uv_cond_init(&item_cond);
}

Processor::~Processor() {
    stop();
    uv_cond_destroy(&item_cond);
    uv_mutex_destroy(&item_mutex);
   
}

// get the current time in milliseconds since something
inline uint64_t now(uv_loop_t* loop) {
    uv_update_time(loop);
    return uv_now(loop);
}

Processor::change_expiration::change_expiration(uint64_t new_exp_) 
    : new_exp(new_exp_) {}

void Processor::change_expiration::operator()(Processor::item& i) {
    i.expiration = new_exp;
}

// check whether the object state index has work for us
bool Processor::hasWork(/* out */ obj_state_by_exp::iterator& it) {
    if (obj_state.size() == 0) return false;
    obj_state_by_exp& exp_index = obj_state.get<expiration_tag>();
    it = exp_index.begin();
    if (it->expiration == 0 || now(&timer_loop) >= it->expiration) return true;
    return false;
}

// update the expiration timer for the item according to the refresh
// rate.
void Processor::updateItemExpiration(obj_state_by_exp::iterator& it) {
    uint64_t newexp = std::numeric_limits<uint64_t>::max();
    if (it->details->refresh_rate > 0) {
        newexp = now(&timer_loop) + it->details->refresh_rate;
    }
    obj_state_by_exp& exp_index = obj_state.get<expiration_tag>();
    exp_index.modify(it, Processor::change_expiration(newexp));
}

// add a reference if it doesn't already exist
void Processor::addRef(obj_state_by_exp::iterator& it,
                       const reference_t& up) {
    if (it->details->urirefs.find(up) == it->details->urirefs.end()) {
        obj_state_by_uri& uri_index = obj_state.get<uri_tag>();
        obj_state_by_uri::iterator uit = uri_index.find(up.second);
        if (uit == uri_index.end()) {
            obj_state.insert(item(up.second, up.first, 
                                  0, LOCAL_REFRESH_RATE,
                                  UNRESOLVED, false));
            // XXX - TODO create the resolver object as well
            uit = uri_index.find(up.second);
        } 
        uit->details->refcount += 1;
        LOG(DEBUG) << "Addref " << uit->uri.toString()
                   << " " << uit->details->refcount
                   << " state " << uit->details->state;
        
        it->details->urirefs.insert(up);
    }
}

// remove a reference if it already exists.  If refcount is zero,
// schedule the reference for collection
void Processor::removeRef(obj_state_by_exp::iterator& it,
                          const reference_t& up) {
    if (it->details->urirefs.find(up) != it->details->urirefs.end()) {
        it->details->refcount -= 1;
        obj_state_by_uri& uri_index = obj_state.get<uri_tag>();
        obj_state_by_uri::iterator uit = uri_index.find(up.second);
        if (uit != uri_index.end()) {
            uit->details->refcount -= 1;
            if (uit->details->refcount <= 0) {
                LOG(DEBUG) << "Refcount zero " << uit->uri.toString();
                uint64_t nexp = now(&timer_loop)+processingDelay;
                uri_index.modify(uit, Processor::change_expiration(nexp));
            }
        }
        it->details->urirefs.erase(up);
    }
}

size_t Processor::getRefCount(const URI& uri) {
    util::LockGuard guard(&item_mutex);
    obj_state_by_uri& uri_index = obj_state.get<uri_tag>();
    obj_state_by_uri::iterator uit = uri_index.find(uri);
    if (uit != uri_index.end()) {
        return uit->details->refcount;
    }
    return 0;
}

// check if the object has a zero refcount and it has no remote
// ancestor that has a zero refcount.
bool Processor::isOrphan(const item& item) {
    // simplest case: refcount is nonzero or item is local
    if (item.details->local || item.details->refcount > 0)
        return false;

    try {
        const std::pair<URI, prop_id_t>& parent =
            client->getParent(item.details->class_id, item.uri);

        obj_state_by_uri& uri_index = obj_state.get<uri_tag>();
        obj_state_by_uri::iterator uit = uri_index.find(parent.first);
        // parent missing
        if (uit == uri_index.end()) return true;

        // the parent is local, so there can be no remote parent with
        // a nonzero refcount
        if (uit->details->local) return true;

        return isOrphan(*uit);
    } catch (std::out_of_range e) {
        return true;
    }
}

// Check if an object is the highest-rank ancestor for objects that
// are synced to the server.  We don't bother syncing child objects
// since those will get synced when we sync the parent.
bool Processor::isParentSyncObject(const item& item) {
    try {
        const ClassInfo& ci = store->getPropClassInfo(item.details->class_id);
        const std::pair<URI, prop_id_t>& parent =
            client->getParent(item.details->class_id, item.uri);
        const ClassInfo& parent_ci = store->getPropClassInfo(parent.second);
        
        // The parent object will be synchronized
        if (ci.getType() == parent_ci.getType()) return false;

        return true;
    } catch (std::out_of_range e) {
        // possibly an unrooted object; sync anyway since it won't be
        // garbage-collected
        return true;
    }
}

// Process the item.  This is where we do most of the actual work of
// syncing the managed object over opflex
void Processor::processItem(obj_state_by_exp::iterator& it) {
    ItemState newState = IN_SYNC;
    StoreClient::notif_t notifs;

    util::LockGuard guard(&item_mutex);
    ItemState curState = it->details->state;
    size_t curRefCount = it->details->refcount;
    bool local = it->details->local;

    const ClassInfo& ci = store->getClassInfo(it->details->class_id);
    shared_ptr<const ObjectInstance> oi;
    try {
        oi = client->get(it->details->class_id, it->uri);
    } catch (std::out_of_range e) {
        // item removed
        switch (curState) {
        case UNRESOLVED:
            break;
        default:
            newState = DELETED;
            break;
        }
    }

    LOG(DEBUG) << "Processing item " << it->uri.toString() 
               << " of class " << ci.getId()
               << " and type " << ci.getType()
               << " in state " << curState;

    // Check whether this item needs to be garbage collected
    if (oi && isOrphan(*it)) {
        switch (curState) {
        case NEW:
            {
                // requeue new items so if there are any pending references
                // we won't remove them right away
                newState = PENDING_DELETE;
                obj_state_by_exp& exp_index = obj_state.get<expiration_tag>();
                exp_index.modify(it, 
                                 change_expiration(now(&timer_loop)+
                                                   processingDelay));
                break;
            }
        default:
            {
                // Remove object from store and dispatch a notification
                LOG(DEBUG) << "Removing nonlocal object with zero refcount "
                           << it->uri.toString();
                client->remove(it->details->class_id,
                               it->uri,
                               false, &notifs);
                client->queueNotification(it->details->class_id, it->uri, 
                                          notifs);
                oi.reset();
                newState = DELETED;
                break;
            }
        }
    }

    // check for references to other objects and update the reference
    // count.  Create a new state object or clear the object as
    // needed.
    boost::unordered_set<reference_t> visited;
    if (oi && ci.getType() != ClassInfo::REVERSE_RELATIONSHIP) {
        BOOST_FOREACH(const ClassInfo::property_map_t::value_type& p, 
                      ci.getProperties()) {
            if (p.second.getType() == PropertyInfo::REFERENCE) {
                if (p.second.getCardinality() == PropertyInfo::SCALAR) {
                    if (oi->isSet(p.first,
                                  PropertyInfo::REFERENCE, 
                                  PropertyInfo::SCALAR)) {
                        reference_t u = oi->getReference(p.first);
                        visited.insert(u);
                        addRef(it, u);
                    }
                } else {
                    size_t c = oi->getReferenceSize(p.first);
                    for (size_t i = 0; i < c; ++i) {
                        reference_t u = oi->getReference(p.first, i);
                        visited.insert(u);
                        addRef(it, u);
                    }
                }
            }
        }
    }
    boost::unordered_set<reference_t> existing(it->details->urirefs);
    BOOST_FOREACH(const reference_t& up, existing) {
        if (visited.find(up) == visited.end()) {
            removeRef(it, up);
        }
    } 

    if (curRefCount > 0) {
        switch (ci.getType()) {
        case ClassInfo::POLICY:
            {
                vector<reference_t> refs;
                refs.push_back(make_pair(it->details->class_id, it->uri));
                PolicyResolveReq req(this, refs);
                pool.writeToRole(req, OpflexHandler::POLICY_REPOSITORY);
                newState = RESOLVED;
            }
            break;
        case ClassInfo::REMOTE_ENDPOINT:
            {
                vector<reference_t> refs;
                refs.push_back(make_pair(it->details->class_id, it->uri));
                EndpointResolveReq req(this, refs);
                pool.writeToRole(req, OpflexHandler::ENDPOINT_REGISTRY);
                newState = RESOLVED;
            }
            break;
            break;
        default:
            // do nothing
            break;
        }

        it->details->state = newState;
    }

    if (oi) {
        switch (ci.getType()) {
        case ClassInfo::LOCAL_ENDPOINT:
            if (isParentSyncObject(*it)) {
                vector<reference_t> refs;
                refs.push_back(make_pair(it->details->class_id, it->uri));
                EndpointDeclareReq req(this, refs);
                pool.writeToRole(req, OpflexHandler::ENDPOINT_REGISTRY);
            }
            newState = IN_SYNC;
            break;
        case ClassInfo::OBSERVABLE:
            if (isParentSyncObject(*it)) {
                vector<reference_t> refs;
                refs.push_back(make_pair(it->details->class_id, it->uri));
                StateReportReq req(this, refs);
                pool.writeToRole(req, OpflexHandler::OBSERVER);
            }
            newState = IN_SYNC;
            break;
        default:
            // do nothing
            break;
        }

        it->details->state = newState;
    } else if (newState == DELETED) {
        LOG(DEBUG) << "Purging state for " << it->uri.toString();

        client->removeChildren(it->details->class_id,
                               it->uri,
                               &notifs);

        switch (ci.getType()) {
        case ClassInfo::POLICY:
            if (curState == RESOLVED) {
                vector<reference_t> refs;
                refs.push_back(make_pair(it->details->class_id, it->uri));
                PolicyUnresolveReq req(this, refs);
                pool.writeToRole(req, OpflexHandler::POLICY_REPOSITORY);
            }
            break;
        case ClassInfo::REMOTE_ENDPOINT:
            if (curState == RESOLVED) {
                vector<reference_t> refs;
                refs.push_back(make_pair(it->details->class_id, it->uri));
                EndpointUnresolveReq req(this, refs);
                pool.writeToRole(req, OpflexHandler::ENDPOINT_REGISTRY);
            }
            break;
        case ClassInfo::LOCAL_ENDPOINT:
            {
                vector<reference_t> refs;
                refs.push_back(make_pair(it->details->class_id, it->uri));
                EndpointUndeclareReq req(this, refs);
                pool.writeToRole(req, OpflexHandler::ENDPOINT_REGISTRY);
            }
            break;
        default:
            // do nothing
            break;
        }

        obj_state_by_exp& exp_index = obj_state.get<expiration_tag>();
        exp_index.erase(it);
    }

    guard.release();

    if (notifs.size() > 0)
        client->deliverNotifications(notifs);
}

// listen on the item queue and dispatch events where required
void Processor::proc_thread_func(void* processor_) {
    Processor* processor = (Processor*)processor_;
    obj_state_by_exp::iterator it;

    while (processor->proc_shouldRun) {
        {
            util::LockGuard guard(&processor->item_mutex);
            while (processor->proc_shouldRun && !processor->hasWork(it))
                uv_cond_wait(&processor->item_cond, &processor->item_mutex);
            if (!processor->proc_shouldRun) return;

            processor->updateItemExpiration(it);
        }
        processor->processItem(it);
    }
}

static void register_listeners(void* processor, const modb::ClassInfo& ci) {
    Processor* p = (Processor*)processor;
    p->listen(ci.getId());
}

void Processor::timer_thread_func(void* processor_) {
    Processor* processor = (Processor*)processor_;
    uv_run(&processor->timer_loop, UV_RUN_DEFAULT);
}
void Processor::timer_callback(uv_timer_t* handle) {
    Processor* processor = (Processor*)handle->data;
    // wake up the processor thread periodically
    {
        util::LockGuard guard(&processor->item_mutex);
        uv_cond_signal(&processor->item_cond);
    }
}

void Processor::start() {
    LOG(DEBUG) << "Starting OpFlex Processor";

    pool.start();

    client = &store->getStoreClient("_SYSTEM_");
    store->forEachClass(&register_listeners, this);

    uv_loop_init(&timer_loop);
    uv_timer_init(&timer_loop, &proc_timer);
    proc_timer.data = this;
    uv_timer_start(&proc_timer, &timer_callback, 
                   processingDelay, processingDelay);
    uv_thread_create(&timer_loop_thread, timer_thread_func, this);

    proc_shouldRun = true;
    uv_thread_create(&proc_thread, proc_thread_func, this);
}

void Processor::stop() {
    if (proc_shouldRun) {
        LOG(DEBUG) << "Stopping OpFlex Processor";
        proc_shouldRun = false;

        unlisten();

        uv_timer_stop(&proc_timer);
        uv_close((uv_handle_t*)&proc_timer, NULL);
        uv_thread_join(&timer_loop_thread);
        uv_loop_close(&timer_loop);
        
        {
            util::LockGuard guard(&item_mutex);
            uv_cond_signal(&item_cond);
        }
        uv_thread_join(&proc_thread);

        pool.stop();
    }
}

void Processor::objectUpdated(modb::class_id_t class_id, 
                              const modb::URI& uri) {
    doObjectUpdated(class_id, uri, false);
}

void Processor::remoteObjectUpdated(modb::class_id_t class_id, 
                                    const modb::URI& uri) {
    doObjectUpdated(class_id, uri, true);
}

// if remote is true, then the object is coming from the server,
// otherwise the object is coming from the listener, which could mean
// either local or remote.
void Processor::doObjectUpdated(modb::class_id_t class_id, 
                                const modb::URI& uri,
                                bool remote) {
    util::LockGuard guard(&item_mutex);
    obj_state_by_uri& uri_index = obj_state.get<uri_tag>();
    obj_state_by_uri::iterator uit = uri_index.find(uri);

    uint64_t nexp = now(&timer_loop)+processingDelay;
    if (uit == uri_index.end()) {
        obj_state.insert(item(uri, class_id, 
                              nexp, LOCAL_REFRESH_RATE,
                              NEW, remote == false));
    } else if (uit->details->local) {
        uit->details->state = UPDATED;
        uri_index.modify(uit, Processor::change_expiration(nexp));
    }
    uv_cond_signal(&item_cond);

}

void Processor::setOpflexIdentity(const std::string& name,
                                  const std::string& domain) {
    pool.setOpflexIdentity(name, domain);
}

void Processor::addPeer(const std::string& hostname,
                        int port) {
    pool.addPeer(hostname, port);
}

OpflexHandler* Processor::newHandler(OpflexConnection* conn) {
    return new OpflexPEHandler(conn, this);
}

} /* namespace engine */
} /* namespace opflex */
