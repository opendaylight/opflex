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
using namespace boost::asio::ip;


namespace  std {
    /**
     * template for hash function for address::ip.
     */
    template<>
    struct hash<boost::asio::ip::address> {
        /**
         * hash ip::address
         * @param i ip::address
         * @return hash
         */
        size_t operator()(const boost::asio::ip::address &i) const {
            return hash<string>()(i.to_string());
        }
    };


}

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
     * filter criteria for sourcing traffic
     */
    typedef struct {
        /**
         * name of filter
         */
        string name;
        /**
         * ip address to be filtered
         */
        address ip;
        /**
         * start of port range to be filtered
         */
        uint16_t portStart;
        /**
         * end pf port range to be filtered
         */
        uint16_t portEnd;
    } filter_t;

    /**
     * hash function for filter struct to use in sets and maps
     */
    struct filter_t_hash {
        /**
         * calculate hash of filter
         * @param k a filter_t struct
         * @return hash
         */
        size_t operator()(const opflexagent::SpanManager::filter_t& k) const {
            size_t v = 0;
            boost::hash_combine(v, k.portEnd);
            boost::hash_combine(v, k.portStart);
            boost::hash_combine(v, hash<address>()(k.ip));
            return v;
        }
    };

    /**
     * class to represent a destination end point
     */
    class DstEndPoint {
    public:
        /**
         * constructor that accepts destination IP address.
         * @param ip ip address of destination end point
         */
        DstEndPoint(address ip) : dstIp(ip) {};
        /**
         * get the name of this destination end point
         * @return string name of destination end point
         */
        const string& getName() const { return name; };
        /**
         * set name of this destination point
         * @param[in] name name of the destination end point
         */
        void setName(const string& name_)  { name = name_;};
        /**
         * get the address of this destination end point
         * @return address address of this destination end point.
         */
        const address& getAddress() const { return dstIp; };
    private:
        string name;
        address dstIp;
    };

    /**
     * class to represent a source end point
     */
    class SourceEndPoint {
    public:
        /**
         * constructor takes a name and port
         * @param[in] name name of localEp
         * @param[in] port source port name on the vswitch
         */
        SourceEndPoint(const string& name, const string& port);
        /**
         * gets the name of the source end point
         * @return name of source end point
         */
        const string& getName() const { return name; };
        /**
         * gets the IP address of the source end point
         * @return IP address of source end point
         */
        const address &getSrcEndPoint() const { return srcEndPoint; };
        /**
         * set srcEndPoint address
         * @param[in] address IP address of the source end point
         */
        void setSrcEndPoint(const address& address_) { srcEndPoint = address_;};
        /**
         * gets the port name of the source end point
         * @return name of port source end point
         */
        const string& getPort() const { return port; };
        /**
         * gets the direction of spanned traffic
         * @return a value from DirectionEnumT.
         */
        const uint8_t getDirection() const { return dir; };
        /**
         * set direction to one of gbp::DirectionEnumT values
         * @param[in] dir direction to be set
         */
        void setDirection(const uint8_t dir_) { dir = dir_;};
        /**
         * gets the map of filters keyed by filter name
         * @returns a reference to a map of name to filters.
         */
        const unordered_map<string, filter_t>& getFilterSet() const {
            return filters;
        }

    private:
        string name;
        address srcEndPoint;
        string port;
        uint8_t dir;
        unordered_map<string, filter_t> filters;

    };

    /**
    * class to represent a span session.
    */
    class SessionState {
    public:
        /**
         * constructor that takes a SessionState as reference.
         * @param s reference to a SessionState object
         */
        SessionState(const SessionState &s);
        /**
         * constructor that takes a URI that points to a Session object
         * @param uri URI to a Session object
         * @param name name of Session object
         */
        SessionState(const URI& uri, const string& name);
        /**
         * gets the URI, which points to a Session object
         * @return a URI
         */
        const URI& getUri() const { return uri;};
        /**
         * set the URI to the one passed in
         * @param uri URI to a Session object
         */
        void setUri(URI &uri_) { uri = uri_;};

        /**
         * add a destination end point to the internal map
         * @param uri uri pointing to the DstSummary object
         * @param dEp shared pointer to a DstEndPoint object.
         */
        void addDstEndPoint(const URI& uri, shared_ptr<DstEndPoint> dEp);

        /**
         * add a source end point to the internal map
         * @param uri uri pointing to a LocalEp object
         * @param srcEp shared pointer to a SourceEndPoint object.
         */
        void addSrcEndPoint(const URI& uri, shared_ptr<SourceEndPoint> srcEp);

        /**
         * get the source end point map reference
         * @returns a reference to the source end point map
         */
        const unordered_map<URI, shared_ptr<SourceEndPoint>>&
             getSrcEndPointMap() const;

        /**
         * gets the destination end point map reference
         * @returns a reference to the destination end point map.
         */
        const unordered_map<URI, shared_ptr<DstEndPoint>>&
             getDstEndPointMap() const;

        /**
         * gets the name string for this object
         * @returns the name attribute string.
         */
        const string getName() { return name; };

    private:
        URI uri;
        string name;
        // mapping LocalEp to SourceEndPoint
        unordered_map<URI, shared_ptr<SourceEndPoint>> srcEndPoints;
        // mapping DstSummary to DstEndPoint
        unordered_map<URI, shared_ptr<DstEndPoint>> dstEndPoints;
        unordered_map<string, filter_t> filters;
    };

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
    void registerListener(PolicyListener* listener);

    /**
     * Unregister Listener for span change events
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     * @see PolicyListener
     */
    void unregisterListener(PolicyListener* listener);

    /**
     * Notify span listeners about an update to the span
     * configuration.
     * @param spanURI the URI of the updated span object
     */
    void notifyListeners(const URI& spanURI);

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
    list<PolicyListener*> spanListeners;
    mutex listener_mutex;
    TaskQueue taskQueue;
    unordered_map<opflex::modb::URI, shared_ptr<SessionState>>
            sess_map;
    unordered_map<URI, shared_ptr<LocalEp>> l2EpUri;
    // list of URIs to send to listeners
    unordered_set<URI> notifyUri;
};
}


#endif /* OPFLEXAGENT_SPANMANAGER_H */
