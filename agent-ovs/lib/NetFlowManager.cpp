/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for NetFlowManager class.
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
#include <opflex/modb/PropertyInfo.h>
#include <boost/optional/optional_io.hpp>
#include <opflex/modb/Mutator.h>



namespace opflexagent {

    using namespace std;
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
        LOG(DEBUG) << "update on NetFlowUniverseListener URI " << uri;

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
                LOG(DEBUG) << "update on exporterconfig and uri is " << exporterconfig.get()->getURI()
			<< " dst address is " << exporterconfig.get()->getDstAddr()
			<< " dst port is " << exporterconfig.get()->getDstPort();
                //process exporter config
                processExporterConfig(exporterconfig.get());
                netflowmanager.notifyUpdate.insert(uri);
            } else {
                LOG(DEBUG) << "exporterconfig removed " << uri;
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
        LOG(DEBUG) << "registering netflow listener";
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

    void NetFlowManager::notifyListeners(const URI& exporterURI) {
        LOG(DEBUG) << "notifying netflow update listener";
        lock_guard<mutex> guard(listener_mutex);
        for (NetFlowListener *listener : netflowListeners) {
             LOG(DEBUG) << "calling netflow listener updated method";
            listener->netflowUpdated(exporterURI);
        }
    }

    void NetFlowManager::notifyListeners(const shared_ptr<ExporterConfigState>& expSt) {
        LOG(DEBUG) << "notifying delete listener";
        lock_guard<mutex> guard(listener_mutex);
        for (NetFlowListener *listener : netflowListeners) {
            listener->netflowDeleted();
        }
    }
        void NetFlowManager::notifyListeners() {
        LOG(DEBUG) << "notifying delete listener";
        lock_guard<mutex> guard(listener_mutex);
        for (NetFlowListener* listener : netflowListeners) {
            listener->netflowDeleted();
        }
    }
     optional<shared_ptr<ExporterConfigState>>
         NetFlowManager::getExporterConfigState(const URI& uri) const {
         unordered_map<URI, shared_ptr<ExporterConfigState>>
                 ::const_iterator itr = exporter_map.find(uri);
         if (itr == exporter_map.end()) {
             return boost::none;
         } else {
             return itr->second;
         }
     }

    void NetFlowManager::NetFlowUniverseListener::processExporterConfig(const shared_ptr<modelgbp::netflow::ExporterConfig>& exporterconfig) {
         shared_ptr<ExporterConfigState> exportState;
         auto itr = netflowmanager.exporter_map.find(exporterconfig->getURI());
         if (itr != netflowmanager.exporter_map.end()) {
             netflowmanager.exporter_map.erase(itr);
         }
        exportState = make_shared<ExporterConfigState>(exporterconfig->getURI(), exporterconfig->getName().get());
        boost::optional<const string& > dstAddr =   exporterconfig->getDstAddr();
        if(dstAddr) {
             exportState->setDstAddress(dstAddr.get());

        }
        boost::optional<uint16_t > dstPort =   exporterconfig->getDstPort();
        if(dstPort) {
             exportState->setDestinationPort(dstPort.get());
        }
        boost::optional<uint32_t> activeTimeout = exporterconfig->getActiveFlowTimeOut();
        if(activeTimeout) {
            exportState->setActiveFlowTimeOut(activeTimeout.get());
        }
        netflowmanager.exporter_map.insert(make_pair(exporterconfig->getURI(), exportState));
    }
 }


