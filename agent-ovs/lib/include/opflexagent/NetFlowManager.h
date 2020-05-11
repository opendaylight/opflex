/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for NetFlowManager listener
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_NETFLOWMANAGER_H
#define OPFLEXAGENT_NETFLOWMANAGER_H

#include <opflex/ofcore/OFFramework.h>
#include <opflex/modb/ObjectListener.h>
#include <opflexagent/NetFlowListener.h>

#include <modelgbp/platform/Config.hpp>
#include <modelgbp/netflow/ExporterConfig.hpp>
#include <modelgbp/netflow/CollectorVersionEnumT.hpp>
#include <opflexagent/TaskQueue.h>
#include <opflex/modb/URI.h>

#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <string>

using boost::asio::deadline_timer;
using namespace std;
using namespace opflex::modb;

namespace opflexagent {

namespace netflow = modelgbp::netflow;
using namespace netflow;

/**
 * class to represent information on net flow
 */
class NetFlowManager {

public:


    /**
     * Instantiate a new NetFlowManager
     */
    NetFlowManager(opflex::ofcore::OFFramework& framework_,
                boost::asio::io_service& agent_io_);

    /**
     * Destroy the NetFlowManager  and clean up all state
     */
    ~NetFlowManager() {};

    /**
     * Start the NetFlowManager
     */
    void start();

    /**
     * Stop the NetFlowManager
     */
    void stop();

    /**
     * retrieve the session state pointer using Session URI as key.
     * @param[in] uri URI pointing to the Session object.
     * @return shared pointer to ExporterConfigState or none.
     */
    boost::optional<shared_ptr<ExporterConfigState>> getExporterConfigState(const URI& uri) const;

    /**
     * Update the exporter config state
     * @param exporterconfig Pointer to updated exporter config
     */
    void updateExporterConfigState(const shared_ptr<modelgbp::netflow::ExporterConfig>& exporterconfig);

    /**
     * Are there any exporters
     *
     */
    bool anyExporters() const;

    /**
     * Register a listener for NetFlow change events
     *
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     * @see PolicyListener
     */
    void registerListener(NetFlowListener* listener);

    /**
     * Unregister Listener for net flow change events
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     * @see PolicyListener
     */
    void unregisterListener(NetFlowListener* listener);

    /**
     * Notify net flow listeners about an update to the net flow
     * configuration.
     * @param netflowURI the URI of the updated net flow object
     */
    void notifyListeners(const URI& netflowURI);

    /**
     * Notify net flow listeners about a session removal
     * @param expSt shared pointer to a ExporterConfigState object
     */
    void notifyListeners(const shared_ptr<ExporterConfigState>& expSt);

    /**
     * Listener for changes related to net flow
     */
    class NetFlowUniverseListener : public opflex::modb::ObjectListener {
    public:
        /**
         * constructor for NetFlowUniverseListener
         * @param[in] netflowmanager reference to net flow manager
         */
        NetFlowUniverseListener(NetFlowManager& netflowmanager);
        virtual ~NetFlowUniverseListener();

        /**
         * callback for handling updates to NetFlow universe
         * @param[in] class_id class id of updated object
         * @param[in] uri of updated object
         */
        virtual void objectUpdated(opflex::modb::class_id_t class_id,
                                   const URI& uri);

        /**
         * process ExporterConfig update
         * @param[in] exporterconfig shared pointer to a ExporterConfig object
         */
         void processExporterConfig(const shared_ptr<modelgbp::netflow::ExporterConfig>& exporterconfig);

    private:
        NetFlowManager& netflowmanager;

    };

    /**
     * instance of net flow universe listener class.
     */
    NetFlowUniverseListener netflowUniverseListener;

private:

    opflex::ofcore::OFFramework& framework;
    /**
     * The NetFlowListener exporter listeners that have been registered
     */
    list<NetFlowListener*> netflowListeners;
    mutex listener_mutex;
    TaskQueue taskQueue;
    static recursive_mutex exporter_mutex;
    unordered_map<opflex::modb::URI, shared_ptr<ExporterConfigState>> exporter_map;
   // list of URIs to send to listeners
    unordered_set<URI> notifyUpdate;
    unordered_set<shared_ptr<ExporterConfigState>> notifyDelete;
};
}


#endif /* OPFLEXAGENT_NETFLOWMANAGER_H */
