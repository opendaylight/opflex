/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for SpanManager class.
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include <opflexagent/NetFlowManager.h>
#include <opflexagent/logging.h>
#include <modelgbp/platform/Config.hpp>
#include <modelgbp/netflow/ExporterConfig.hpp>

//#include <modelgbp/domain/Config.hpp>
#include <opflex/modb/PropertyInfo.h>

//#include <modelgbp/gbp/EpGroup.hpp>
//#include <modelgbp/span/LocalEp.hpp>
#include <boost/optional/optional_io.hpp>
#include <opflex/modb/Mutator.h>

//#include <modelgbp/epr/L2Universe.hpp>
//#include <modelgbp/epdr/EndPointToGroupRSrc.hpp>

namespace opflexagent {

    using namespace std;
   
   // using namespace modelgbp::gbp;
    using namespace opflex::modb;
    using boost::optional;
    using opflex::modb::class_id_t;
    using opflex::modb::URI;
    using boost::posix_time::milliseconds;
    using opflex::modb::Mutator;

   

    NetFlowManager::NetFlowManager(opflex::ofcore::OFFramework &framework_,
                             boost::asio::io_service& agent_io_) :
            netflowUniverseListener(*this), framework(framework_),
            taskQueue(agent_io_){

    }

    template<typename K, typename V>
    static void print_map(unordered_map<K,V> const& m) {
        for (auto const &pair: m) {
            LOG(DEBUG) << "{" << pair.first << ":" << pair.second << "}\n";
        }
    }

    void NetFlowManager::start() {
        LOG(DEBUG) << "starting netflow manager";
        ExporterConfig::registerListener(framework, &netflowUniverseListener);
       
    
    }

    void NetFlowManager::stop() {
        ExporterConfig::unregisterListener(framework, &netflowUniverseListener);
       
    }

    NetFlowManager::NetFlowUniverseListener::NetFlowUniverseListener(NetFlowManager &netflowManager) :
            netflowmanager(netflowManager) {}

    NetFlowManager::NetFlowUniverseListener::~NetFlowUniverseListener() {}

    void NetFlowManager::NetFlowUniverseListener::objectUpdated(class_id_t classId,
                                                          const URI &uri) {
        LOG(DEBUG) << "update on class ID " << classId << " URI " << uri;

        // updates on parent container for exporterconfig are received for
        // exporterconfig creation. Deletion and modification updates are
        // sent to the object itself.
        if (classId == modelgbp::platform::Config::CLASS_ID) {
            optional <shared_ptr<modelgbp::platform::Config>> config_opt =
                    modelgbp::platform::Config::resolve(netflowmanager.framework, uri);
             if (config_opt) {
                vector <shared_ptr<modelgbp::netflow::ExporterConfig>> netflowexporterVec;
                 config_opt.get()->resolveNetflowExporterConfig(netflowexporterVec);
                 for (shared_ptr <modelgbp::netflow::ExporterConfig> newflowexporter : netflowexporterVec) {
                     auto itr = netflowmanager.exporter_map.find(newflowexporter->getURI());
                    if (itr == netflowmanager.exporter_map.end()) {
                        LOG(DEBUG) << "creating netflow exporter config " << newflowexporter->getURI();
                        processExporterConfig(newflowexporter);
                     }
                   netflowmanager.notifyUpdate.insert(newflowexporter->getURI());
                  }
             }
        } else if (classId == modelgbp::netflow::ExporterConfig::CLASS_ID) {
            optional<shared_ptr<modelgbp::netflow::ExporterConfig>> exporterconfig =
                    modelgbp::netflow::ExporterConfig::resolve(netflowmanager.framework, uri);
            if (exporterconfig) {
                LOG(DEBUG) << "update on exporterconfig " << exporterconfig.get()->getURI();
                processExporterConfig(exporterconfig.get());
                netflowmanager.notifyUpdate.insert(uri);
            } else {
                LOG(DEBUG) << "exporterconfig removed " << uri;
                shared_ptr<ExporterConfigState> exporterState;
                auto itr = netflowmanager.exporter_map.find(uri);
                if (itr != netflowmanager.exporter_map.end()) {
                    netflowmanager.notifyDelete.insert(itr->second);
                    netflowmanager.exporter_map.erase(itr);
                   
                }
            }
        }
        // notify all listeners. put it on a task Q for non blocking notification.
        for (URI uri : netflowmanager.notifyUpdate) {
            netflowmanager.taskQueue.dispatch(uri.toString(), [=]() {
                netflowmanager.notifyListeners(uri);
            });
        }
        netflowmanager.notifyUpdate.clear();
        for (shared_ptr<ExporterConfigState> expSt : netflowmanager.notifyDelete) {
            netflowmanager.taskQueue.dispatch(expSt->getName(), [=]() {
                netflowmanager.notifyListeners(expSt);
            });
        }
        netflowmanager.notifyDelete.clear();
    }

    void NetFlowManager::registerListener(NetFlowListener *listener) {
        LOG(DEBUG) << "registering listener";
        lock_guard <mutex> guard(listener_mutex);
        netflowListeners.push_back(listener);
         if (isDeletePending) {
            notifyListeners();
        }
        isDeletePending = false;
    }

    void NetFlowManager::unregisterListener(NetFlowListener* listener) {
        lock_guard <mutex> guard(listener_mutex);
        netflowListeners.remove(listener);
    }

    void NetFlowManager::notifyListeners(const URI& spanURI) {
        LOG(DEBUG) << "notifying update listener";
        lock_guard<mutex> guard(listener_mutex);
        for (NetFlowListener *listener : netflowListeners) {
            listener->netflowUpdated(spanURI);
        }
    }

    void NetFlowManager::notifyListeners(shared_ptr<ExporterConfigState> expSt) {
        LOG(DEBUG) << "notifying delete listener";
        lock_guard<mutex> guard(listener_mutex);
        for (NetFlowListener *listener : netflowListeners) {
            listener->netflowDeleted(expSt);
        }
    }
        void NetFlowManager::notifyListeners() {
        LOG(DEBUG) << "notifying delete listener";
        lock_guard<mutex> guard(listener_mutex);
        for (NetFlowListener* listener : netflowListeners) {
            listener->netflowDeleted();
        }
    }
    // optional<shared_ptr<SessionState>>
    //     SpanManager::getSessionState(const URI& uri) const {
    //     unordered_map<URI, shared_ptr<SessionState>>
    //             ::const_iterator itr = sess_map.find(uri);
    //     if (itr == sess_map.end()) {
    //         return boost::none;
    //     } else {
    //         return itr->second;
    //     }
    // }

    void NetFlowManager::NetFlowUniverseListener::processExporterConfig(shared_ptr<modelgbp::netflow::ExporterConfig> exporterconfig) {
        // shared_ptr<SessionState> sessState;
        // auto itr = spanmanager.sess_map.find(sess->getURI());
        // if (itr != spanmanager.sess_map.end()) {
        //     spanmanager.sess_map.erase(itr);
        // }
        // sessState = make_shared<SessionState>(sess->getURI(), sess->getName().get());
        // spanmanager.sess_map.insert(make_pair(sess->getURI(), sessState));
        // print_map(spanmanager.sess_map);

    
    }

   

}


