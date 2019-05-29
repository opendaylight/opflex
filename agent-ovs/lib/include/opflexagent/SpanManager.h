/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for SpanManager listener
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_SPANMANAGER_H
#define OPFLEXAGENT_SPANMANAGER_H

#include <opflex/ofcore/OFFramework.h>
#include <opflex/modb/ObjectListener.h>
#include <opflexagent/PolicyListener.h>
#include <opflexagent/SpanListener.h>
#include <modelgbp/span/Universe.hpp>
#include <modelgbp/span/Session.hpp>
#include <modelgbp/span/SrcGrp.hpp>
#include <modelgbp/span/SrcMember.hpp>
#include <modelgbp/span/LocalEp.hpp>
#include <opflexagent/TaskQueue.h>
#include <opflex/modb/URI.h>
#include <modelgbp/epr/L2Ep.hpp>
#include <modelgbp/gbp/DirectionEnumT.hpp>

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

    using namespace modelgbp::epr;
    namespace span = modelgbp::span;
    using namespace span;

/**
 * class to represent information on span
 */
class SpanManager {

public:


    /**
     * Instantiate a new span manager
     */
    SpanManager(opflex::ofcore::OFFramework& framework_,
                boost::asio::io_service& agent_io_);

    /**
     * Destroy the span manager and clean up all state
     */
    ~SpanManager() {};

    /**
     * Start the span manager
     */
    void start();

    /**
     * Stop the span manager
     */
    void stop();

     /**
      * retrieve the session state pointer using Session URI as key.
      * @param[in] uri URI pointing to the Session object.
      * @return shared pointer to SessionState or none.
      */
      boost::optional<shared_ptr<SessionState>>
          getSessionState(const URI& uri) const;

    /**
     * Register a listener for span change events
     *
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     * @see PolicyListener
     */
    void registerListener(SpanListener* listener);

    /**
     * Unregister Listener for span change events
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     * @see PolicyListener
     */
    void unregisterListener(SpanListener* listener);

    /**
     * Notify span listeners about an update to the span
     * configuration.
     * @param spanURI the URI of the updated span object
     */
    void notifyListeners(const URI& spanURI);

    /**
     * Notify span listeners about a session removal
     * @param seSt shared pointer to a SessionState object
     */
    void notifyListeners(const shared_ptr<SessionState> seSt);

    /**
     * Listener for changes related to span
     */
    class SpanUniverseListener : public opflex::modb::ObjectListener {
    public:
        /**
         * constructor for SpanUniverseListener
         * @param[in] spanmanager reference to span manager
         */
        SpanUniverseListener(SpanManager& spanmanager);
        virtual ~SpanUniverseListener();

        /**
         * callback for handling updates to Span universe
         * @param[in] class_id class id of updated object
         * @param[in] uri of updated object
         */
        virtual void objectUpdated(opflex::modb::class_id_t class_id,
                                   const URI& uri);

        /**
         * process session update
         * @param[in] sess shared pointer to a Session object
         */
         void processSession(shared_ptr<Session> sess);

        /**
        * process source group update
        * @param[in] srcGrp a shared pointer to the source group object in MODB.
        */
        void processSrcGrp(shared_ptr<SrcGrp> srcGrp);

        /**
         * process destination group update
         * @param[in] dstGrp  shared pointer to a destination group object
         * @param[in] sess a reference to a Session object
         */
        void processDstGrp(DstGrp& dstGrp, Session& sess);
        /**
        * process LocalEp update
        * @param[in] uri a uri pointing to the LocalEp object in  MODB.
        */
        void processLocalEp(const URI& uri);

        /**
         * process L2EP update
         * @param[in] l2Ep shared pointer to L2EP object
         */
         void processL2Ep(shared_ptr<L2Ep> l2Ep);

         /**
          * add an end point to session state object
          * @param[in] lEp shared pointer to LocalEp object
          * @param[in] l2Ep shared pointer to L2EP object
          */
          void addEndPoint(shared_ptr<LocalEp> lEp, shared_ptr<L2Ep> l2Ep);

    private:
        SpanManager& spanmanager;

    };

    /**
     * instance of span universe listener class.
     */
    SpanUniverseListener spanUniverseListener;
    friend class SpanUniverseListener;

private:

    opflex::ofcore::OFFramework& framework;
    /**
     * The span listeners that have been registered
     */
    list<SpanListener*> spanListeners;
    mutex listener_mutex;
    TaskQueue taskQueue;
    unordered_map<opflex::modb::URI, shared_ptr<SessionState>>
            sess_map;
    unordered_map<URI, shared_ptr<LocalEp>> l2EpUri;
    // list of URIs to send to listeners
    unordered_set<URI> notifyUpdate;
    unordered_set<shared_ptr<SessionState>> notifyDelete;
};
}


#endif /* OPFLEXAGENT_SPANMANAGER_H */
