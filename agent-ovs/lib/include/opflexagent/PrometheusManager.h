/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for PrometheusManager class.
 *
 * Copyright (c) 2019-2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef __OPFLEXAGENT_PROMETHEUS_MANAGER_H__
#define __OPFLEXAGENT_PROMETHEUS_MANAGER_H__

#include <opflex/ofcore/OFFramework.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <mutex>

#include <prometheus/gauge.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

namespace opflexagent {

using std::string;
using std::map;
using std::unordered_map;
using std::unique_ptr;
using std::shared_ptr;
using std::mutex;
using std::pair;
using boost::optional;
using namespace prometheus;

class Agent;

// Optional pair of label attr hash and Gauge ptr
typedef optional<pair<size_t, Gauge *> >  hgauge_pair_t;
// Optional pair of label attr map and Gauge ptr
typedef optional<pair<map<string, string>, Gauge *> >  mgauge_pair_t;

/**
 * Prometheus manager is responsible for maintaining state of all
 * the metrics exposed from opflex agent to prometheus. It is also
 * responsible for opening a http server to accept get requests for
 * exporting available metrics to prometheus.
 */
class PrometheusManager {
public:
    /**
     * Instantiate a new prometheus manager
     */
    PrometheusManager(Agent &agent_, opflex::ofcore::OFFramework &fwk_);
    /**
     * Destroy the prometheus manager and clean up all state
     */
    ~PrometheusManager() {};
    /**
     * Start the prometheus manager
     */
    void start();
    /**
     * Stop the prometheus manager
     */
    void stop();

    /* EpCounter related APIs */
    /**
     * Return a rolling hash of attribute map for the ep
     *
     * @param ep_name     Name of the endpoint. Usually the
     * access interface name
     * @param attr_map    The map of endpoint attributes
     * @return            the hash value of endpoint attributes
     */
    static size_t calcHashEpAttributes(const string& ep_name,
            const unordered_map<string, string>&    attr_map);
    /**
     * Create EpCounter metric family if its not present.
     * Update EpCounter metric family if its already present
     *
     * @param uuid        uuid of ep
     * @param ep_name     the name of the ep
     * @param attr_hash   hash of prometheus compatible ep attr
     * @param attr_map    map of all ep attributes
     */
    void addNUpdateEpCounter(const string& uuid,
                             const string& ep_name,
                             const size_t& attr_hash,
        const unordered_map<string, string>&    attr_map);
    /**
     * Remove EpCounter metric given the ep name
     */
    void removeEpCounter(const string& uuid,
                         const string& ep_name);

    /* PodSvcCounter related APIs */
    /**
     * Create PodSvcCounter metric family if its not present.
     * Update PodSvcCounter metric family if its already present
     *
     * @param isEpToSvc       true if EpToSvc reporting
     * @param uuid            uuid of POD+SVC
     * @param ep_attr_map     map of all ep attributes
     * @param svc_attr_map    map of all svc attributes
     */
    void addNUpdatePodSvcCounter(bool isEpToSvc,
                                 const string& uuid,
        const unordered_map<string, string>& ep_attr_map,
        const unordered_map<string, string>& svc_attr_map);
    /**
     * Remove PodSvcCounter metric given the direciton and uuid
     *
     * @param isEpToSvc     true if EpToSvc reporting
     * @param uuid          uuid of POD+SVC
     */
    void removePodSvcCounter(bool isEpToSvc,
                             const string& uuid);

    /* OFPeerStats related APIs */
    /**
     * Create OFPeerStats metric family if its not present.
     * Update OFPeerStats metric family if its already present
     */
    void addNUpdateOFPeerStats(void);


    /* RDDropCounter related APIs */
    /**
     * Create RDDropCounter metric family if its not present.
     * Update RDDropCounter metric family if its already present
     *
     * @param rdURI       URI of routing domain
     * @param isAdd       flag to indicate if its create or update
     */
    void addNUpdateRDDropCounter(const string& rdURI,
                                 bool isAdd);
    /**
     * Remove RDDropCounter metric given URI of routing domain
     *
     * @param rdURI     URI of routing domain
     */
    void removeRDDropCounter(const string& rdURI);


    // TODO: Other Counter related APIs

private:
    // opflex agent handle
    Agent&     agent;
    // opflex framwork handle
    opflex::ofcore::OFFramework& framework;
    // The http server handle
    unique_ptr<Exposer>     exposer_ptr;
    /**
     * Registry which keeps track of all the metric
     * families to scrape.
     */
    shared_ptr<Registry>    registry_ptr;
    // Create any gauge metrics during start
    void createStaticGauges(void);
    // remove any gauge metrics during stop
    void removeStaticGauges(void);
    // create any gauge metric families during start
    void createStaticGaugeFamilies(void);
    // remove any gauge metric families during stop
    void removeStaticGaugeFamilies(void);
    // create any counters at start
    void createStaticCounters(void);
    // remove any counters at stop
    void removeStaticCounters(void);
    // create any counter families at start
    void createStaticCounterFamilies(void);
    // remove any counter families at stop
    void removeStaticCounterFamilies(void);
    // Remove apis for dynamic families and metrics
    void removeDynamicCounterFamilies(void);
    void removeDynamicGaugeFamilies(void);
    void removeDynamicCounters(void);
    void removeDynamicGauges(void);

    //Utility apis
    /**
     * Prometheus expects label/metric family names to be given
     * in a particular format.
     * More info: https://prometheus.io/docs/concepts/data_model/
     * In short: the names can be like "[a-zA-Z_:][a-zA-Z0-9_:]*"
     * and cannot start with "__" (used internally).
     *
     * @param: the name that we have to sanitize
     *
     * @return: the sanitized name that can be given to prometheus
     */
    static string sanitizeMetricName(string metric_name);
    // Api to check if the input metric_name is prometheus compatible.
    static bool checkMetricName(const string& metric_name);

    /* Start of EpCounter related apis and state */
    // Lock to safe guard EpCounter related state
    mutex ep_counter_mutex;

    enum EP_METRICS {
        EP_RX_BYTES, EP_RX_PKTS, EP_RX_DROPS,
        EP_RX_UCAST, EP_RX_MCAST, EP_RX_BCAST,
        EP_TX_BYTES, EP_TX_PKTS, EP_TX_DROPS,
        EP_TX_UCAST, EP_TX_MCAST, EP_TX_BCAST,
        EP_METRICS_MAX
    };

    // Static Metric families and metrics
    // metric families to track all EpCounter metrics
    Family<Gauge>      *gauge_ep_family_ptr[EP_METRICS_MAX];
    // Counter family to track all EpCounter creates
    Family<Counter>    *counter_ep_create_family_ptr;
    // Counter family to track all EpCounter removes
    Family<Counter>    *counter_ep_remove_family_ptr;
    // Gauge family to track the  total # of EpCounters
    Family<Gauge>      *gauge_ep_total_family_ptr;
    // Counter to track ep creates
    Counter            *counter_ep_create_ptr;
    // Counter to track ep removes
    Counter            *counter_ep_remove_ptr;
    // Gauge to track total Eps
    Gauge              *gauge_ep_total_ptr;
    // Actual ep total count
    double              gauge_ep_total;
    // func to increment EpCounter create
    void incStaticCounterEpCreate(void);
    // func to decrement EpCounter remove
    void incStaticCounterEpRemove(void);
    // func to set total EpCounter
    void updateStaticGaugeEpTotal(bool add);
    // create any ep gauge metric families during start
    void createStaticGaugeFamiliesEp(void);
    // remove any ep gauge metric families during stop
    void removeStaticGaugeFamiliesEp(void);
    // create any ep counter metric families during start
    void createStaticCounterFamiliesEp(void);
    // remove any ep counter metric families during stop
    void removeStaticCounterFamiliesEp(void);
    // create any ep gauge metric during start
    void createStaticGaugesEp(void);
    // remove any ep gauge metric during stop
    void removeStaticGaugesEp(void);
    // create any ep counter metric during start
    void createStaticCountersEp(void);
    // remove any ep counter metric during stop
    void removeStaticCountersEp(void);

    // Dynamic Metric families and metrics
    // CRUD for every EP Counter metric
    // func to create gauge for EpCounter given metric type, uuid & attr map
    bool createDynamicGaugeEp(EP_METRICS metric,
                              const string& uuid,
                              const string& ep_name,
                              const size_t& attr_hash,
        const unordered_map<string, string>&    attr_map);
    // func to get gauge for EpCounter given metric type, uuid
    hgauge_pair_t getDynamicGaugeEp(EP_METRICS metric, const string& uuid);
    // func to remove gauge for EpCounter given metric type, uuid
    bool removeDynamicGaugeEp(EP_METRICS metric, const string& uuid);
    // func to remove all gauge of every EpCounter for a metric type
    void removeDynamicGaugeEp(EP_METRICS metric);
    // func to remove all gauges of every EpCounter
    void removeDynamicGaugeEp(void);

    /**
     * cache the pair of (label map hash, gauge ptr) for every ep uuid
     * The hash is created utilizing prometheus lib, which is basically
     * a rolling hash  of all the key,value pairs of the ep attributes.
     */
    unordered_map<string, hgauge_pair_t> ep_gauge_map[EP_METRICS_MAX];

    //Utility apis
    // Create a label map that can be used for annotation, given the ep attr map
    static map<string,string> createLabelMapFromEpAttr(const string& ep_name_,
                           const unordered_map<string, string>&    attr_map);
    // Maximum number of labels that can be used for annotating a metric
    static int max_metric_attr_count;
    /* End of EpCounter related apis and state */


    /* Start of PodSvcCounter related apis and state */
    // Lock to safe guard PodSvcCounter related state
    mutex podsvc_counter_mutex;

    enum PODSVC_METRICS {
        PODSVC_METRICS_MIN,
        PODSVC_EP2SVC_MIN = PODSVC_METRICS_MIN,
        PODSVC_EP2SVC_BYTES = PODSVC_EP2SVC_MIN,
        PODSVC_EP2SVC_PKTS,
        PODSVC_EP2SVC_MAX = PODSVC_EP2SVC_PKTS,
        PODSVC_SVC2EP_MIN,
        PODSVC_SVC2EP_BYTES = PODSVC_SVC2EP_MIN,
        PODSVC_SVC2EP_PKTS,
        PODSVC_SVC2EP_MAX = PODSVC_SVC2EP_PKTS,
        PODSVC_METRICS_MAX = PODSVC_SVC2EP_MAX
    };

    // Static Metric families and metrics
    // metric families to track all PodSvcCounter metrics
    Family<Gauge>      *gauge_podsvc_family_ptr[PODSVC_METRICS_MAX+1];

    // create any podsvc gauge metric families during start
    void createStaticGaugeFamiliesPodSvc(void);
    // remove any podsvc gauge metric families during stop
    void removeStaticGaugeFamiliesPodSvc(void);

    // Dynamic Metric families and metrics
    // CRUD for every PodSvc counter metric
    // func to create gauge for PodSvcCounter given metric type,
    // uuid of ep+svc & attr map of ep and svc
    void createDynamicGaugePodSvc(PODSVC_METRICS metric,
                                  const string& uuid,
        const unordered_map<string, string>& ep_attr_map,
        const unordered_map<string, string>& svc_attr_map);

    // func to get label map and Gauge for PodSvcCounter given metric type, uuid
    mgauge_pair_t getDynamicGaugePodSvc(PODSVC_METRICS metric, const string& uuid);

    // func to remove gauge for PodSvcCounter given metric type, uuid
    bool removeDynamicGaugePodSvc(PODSVC_METRICS metric, const string& uuid);
    // func to remove all gauge of every PodSvcCounter for a metric type
    void removeDynamicGaugePodSvc(PODSVC_METRICS metric);
    // func to remove all gauges of every PodSvcCounter
    void removeDynamicGaugePodSvc(void);

    /**
     * cache the label map and Gauge ptr for every (endpoint + service) uuid
     */
    unordered_map<string, mgauge_pair_t> podsvc_gauge_map[PODSVC_METRICS_MAX+1];

    //Utility apis
    // Create a label map that can be used for annotation, given the ep+svc attr map
    static const map<string,string> createLabelMapFromPodSvcAttr(
                           const unordered_map<string, string>&  ep_attr_map,
                           const unordered_map<string, string>&  svc_attr_map);
    /* End of PodSvcCounter related apis and state */


    /* Start of OFPeerStats related apis and state */
    // Lock to safe guard OFPeerStats related state
    mutex ofpeer_stats_mutex;

    enum OFPEER_METRICS {
        OFPEER_METRICS_MIN,
        OFPEER_IDENT_REQS = OFPEER_METRICS_MIN,
        OFPEER_IDENT_RESPS,
        OFPEER_IDENT_ERRORS,
        OFPEER_POL_RESOLVES,
        OFPEER_POL_RESOLVE_RESPS,
        OFPEER_POL_RESOLVE_ERRS,
        OFPEER_POL_UNRESOLVES,
        OFPEER_POL_UNRESOLVE_RESPS,
        OFPEER_POL_UNRESOLVE_ERRS,
        OFPEER_POL_UPDATES,
        OFPEER_EP_DECLARES,
        OFPEER_EP_DECLARE_RESPS,
        OFPEER_EP_DECLARE_ERRS,
        OFPEER_EP_UNDECLARES,
        OFPEER_EP_UNDECLARE_RESPS,
        OFPEER_EP_UNDECLARE_ERRS,
        OFPEER_STATE_REPORTS,
        OFPEER_STATE_REPORT_RESPS,
        OFPEER_STATE_REPORT_ERRS,
        OFPEER_METRICS_MAX = OFPEER_STATE_REPORT_ERRS
    };

    // Static Metric families and metrics
    // metric families to track all OFPeerStats metrics
    Family<Gauge>      *gauge_ofpeer_family_ptr[OFPEER_METRICS_MAX+1];

    // create any ofpeer stats gauge metric families during start
    void createStaticGaugeFamiliesOFPeer(void);
    // remove any ofpeer stats gauge metric families during stop
    void removeStaticGaugeFamiliesOFPeer(void);

    // Dynamic Metric families and metrics
    // CRUD for every OFPeer counter metric
    // func to create gauge for OFPeerStats given metric type,
    // peer: the unique peer (IP,port) tuple
    void createDynamicGaugeOFPeer(OFPEER_METRICS metric,
                                  const string& peer);

    // func to get label map and Gauge for OFPeerStats given metric type, peer
    Gauge * getDynamicGaugeOFPeer(OFPEER_METRICS metric, const string& peer);

    // func to remove gauge for OFPeerStats given metric type, peer
    bool removeDynamicGaugeOFPeer(OFPEER_METRICS metric, const string& peer);
    // func to remove all gauge of every OFPeerStats for a metric type
    void removeDynamicGaugeOFPeer(OFPEER_METRICS metric);
    // func to remove all gauges of every OFPeerStats
    void removeDynamicGaugeOFPeer(void);

    /**
     * cache Gauge ptr for every peer metric
     */
    unordered_map<string, Gauge*> ofpeer_gauge_map[OFPEER_METRICS_MAX+1];
    /* End of OFPeerStats related apis and state */


    /* Start of RDDropCounter related apis and state */
    // Lock to safe guard RDDropCounter related state
    mutex rddrop_stats_mutex;

    enum RDDROP_METRICS {
        RDDROP_METRICS_MIN,
        RDDROP_BYTES = RDDROP_METRICS_MIN,
        RDDROP_PACKETS,
        RDDROP_METRICS_MAX = RDDROP_PACKETS
    };

    // Static Metric families and metrics
    // metric families to track all RDDropCounter metrics
    Family<Gauge>      *gauge_rddrop_family_ptr[RDDROP_METRICS_MAX+1];

    // create any rddrop stats gauge metric families during start
    void createStaticGaugeFamiliesRDDrop(void);
    // remove any rddrop stats gauge metric families during stop
    void removeStaticGaugeFamiliesRDDrop(void);

    // Dynamic Metric families and metrics
    // CRUD for every RDDrop counter metric
    // func to create gauge for RDDrop given metric type,
    // rdURI
    void createDynamicGaugeRDDrop(RDDROP_METRICS metric,
                                  const string& rdURI);

    // func to get label map and Gauge for RDDropCounter given metric type, rdURI
    Gauge * getDynamicGaugeRDDrop(RDDROP_METRICS metric, const string& rdURI);

    // func to remove gauge for RDDropCounter given metric type, rdURI
    bool removeDynamicGaugeRDDrop(RDDROP_METRICS metric, const string& rdURI);
    // func to remove all gauge of every RDDropCounter for a metric type
    void removeDynamicGaugeRDDrop(RDDROP_METRICS metric);
    // func to remove all gauges of every RDDropCounter
    void removeDynamicGaugeRDDrop(void);

    // RDDropCounter diffs are stored in a circular buffer. Each buffer element
    // has a unique running genID. Keep track of the last processed genId by
    // PrometheusManager to avoid double counting
    uint32_t rddrop_last_genId;

    /**
     * cache Gauge ptr for every RDDropCounter metric
     */
    unordered_map<string, Gauge*> rddrop_gauge_map[RDDROP_METRICS_MAX+1];
    /* End of RDDropCounter related apis and state */


    /* TODO: Other Counter related apis and state */
};

} /* namespace opflexagent */

#endif // __OPFLEXAGENT_PROMETHEUS_MANAGER_H__
