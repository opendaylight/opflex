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
using std::unordered_set;
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
     *
     * @param exposeLocalHostOnly     flag to indicate if the the exposer
     *                                should be bound with local host only.
     */
    void start(bool exposeLocalHostOnly);
    /**
     * Stop the prometheus manager
     */
    void stop();

    /* EpCounter related APIs */
    /**
     * Return a rolling hash of attribute map for the ep
     *
     * @param ep_name       Name of the endpoint. Usually the
     * access interface name
     * @param attr_map      The map of endpoint attributes
     * @param allowed_set   The set of allowed ep attributes from
     * agent configuration file
     * @return            the hash value of endpoint attributes
     */
    static size_t calcHashEpAttributes(const string& ep_name,
            const unordered_map<string, string>&    attr_map,
            const unordered_set<string>&        allowed_set);
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

    /* SvcTargetCounter related APIs */
    /**
     * Create SvcTargetCounter metric family if its not present.
     * Update SvcTargetCounter metric family if its already present
     *
     * @param uuid            uuid of SVC
     * @param nhip            IP addr of svc-target
     * @param rx_bytes        ingress bytes
     * @param rx_pkts         ingress packets
     * @param tx_bytes        egress bytes
     * @param tx_pkts         egress packets
     * @param svc_attr_map    map of svc attributes
     * @param ep_attr_map     map of ep/pod attributes
     * @param updateLabels    update label annotations during cfg updates
     */
    void addNUpdateSvcTargetCounter(const string& uuid,
                                    const string& nhip,
                                    uint64_t rx_bytes,
                                    uint64_t rx_pkts,
                                    uint64_t tx_bytes,
                                    uint64_t tx_pkts,
        const unordered_map<string, string>& svc_attr_map,
        const unordered_map<string, string>& ep_attr_map,
                                    bool updateLabels);
    /**
     * Remove SvcTargetCounter metrics given it's uuid
     *
     * @param uuid          uuid of SVC
     * @param nhip          IP addr of svc-target
     */
    void removeSvcTargetCounter(const string& uuid,
                                const string& nhip);

    /* SvcCounter related APIs */
    /**
     * Create SvcCounter metric family if its not present.
     * Update SvcCounter metric family if its already present
     *
     * @param uuid            uuid of SVC
     * @param rx_bytes        ingress bytes
     * @param rx_pkts         ingress packets
     * @param tx_bytes        egress bytes
     * @param tx_pkts         egress packets
     * @param svc_attr_map    map of all svc attributes
     */
    void addNUpdateSvcCounter(const string& uuid,
                              uint64_t rx_bytes,
                              uint64_t rx_pkts,
                              uint64_t tx_bytes,
                              uint64_t tx_pkts,
        const unordered_map<string, string>& svc_attr_map);
    /**
     * Remove SvcCounter metrics given it's uuid
     *
     * @param uuid          uuid of SVC
     */
    void removeSvcCounter(const string& uuid);

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


    /* SvcCounter related APIs */
    /**
     * Increment svc count
     */
    void incSvcCounter(void);
    /**
     * Decrement svc count
     */
    void decSvcCounter(void);


    /* RemoteEp related APIs */
    /**
     * Create RemoteEp metric family if its not present.
     * Update RemoteEp metric family if its already present
     *
     * @param count       total number of remote EPs under same uplink
     */
    void addNUpdateRemoteEpCount(size_t count);


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

    /**
     * Add TableDropGauge metric given bridge name and table name
     *
     * @param bridge_name     Name of the bridge
     * @param table_name      Name of the table in the bridge
     */
    void addTableDropGauge(const string& bridge_name,
                           const string& table_name) {
        createStaticGaugeTableDrop(bridge_name, table_name);
    }
    /**
     * Remove TableDropGauge metrics given the bridge and table
     *
     * @param bridge_name     Name of the bridge
     * @param table_name      Name of the table in the bridge
     */
    void removeTableDropGauge(const string& bridge_name,
                              const string& table_name);

    /**
     * Update TableDropGauge metrics given bridge name and table name
     * @param bridge_name     Name of the bridge
     * @param table_name      Name of the table in the bridge
     * @param bytes           Bytes for this drop gauge
     * @param packets         Packets for this drop gauge
     */
    void updateTableDropGauge(const string& bridge_name,
                              const string& table_name,
                              const uint64_t &bytes,
                              const uint64_t &packets);

    /* SecGrpClassifierCounter related APIs */
    /**
     * Create SGClassifierCounter metric family if its not present.
     * Update SGClassifierCounter metric family if its already present
     *
     * @param classifier       name of the classifier
     */
    void addNUpdateSGClassifierCounter(const string& classifier);
    /**
     * Remove SGClassifierCounter metric given the classifier
     *
     * @param classifier       name of the classifier
     */
    void removeSGClassifierCounter(const string& classifier);


    /* ContractClassifierCounter related APIs */
    /**
     * Create ContractClassifierCounter metric family if its not present.
     * Update ContractClassifierCounter metric family if its already present
     *
     * @param srcEpg           name of the srcEpg
     * @param dstEpg           name of the dstEpg
     * @param classifier       name of the classifier
     */
    void addNUpdateContractClassifierCounter(const string& srcEpg,
                                             const string& dstEpg,
                                             const string& classifier);
    /**
     * Remove ContractClassifierCounter metric given the classifier
     *
     * @param srcEpg           name of the srcEpg
     * @param dstEpg           name of the dstEpg
     * @param classifier       name of the classifier
     */
    void removeContractClassifierCounter(const string& srcEpg,
                                         const string& dstEpg,
                                         const string& classifier);


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

    // Init state
    void init(void);

    // If the annotations are same, then we could land up in a situation where
    // the gauge pointers are same across metrics. This could lead to memory
    // corruption. Maintain a set of Gauge pointers to track any duplicates and
    // use it to avoid creating duplicate metrics.
    // 'T' can be one of the 4 supported metric types
    template <typename T>
    class MetricDupChecker {
    public:
        MetricDupChecker() {};
        ~MetricDupChecker() {};
        // check if a metric already exists
        bool is_dup(T *);
        // add metric ptr to checker
        void add(T *);
        // remove the metric from checker
        void remove(T *);
        // Erase all state
        void clear(void);
    private:
        // Lock to safe guard duplicate metric check state
        mutex dup_mutex;
        unordered_set<T *>  metrics;
    };

    MetricDupChecker<Gauge> gauge_check;
    MetricDupChecker<Counter> counter_check;

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
    static string sanitizeMetricName(const string& metric_name);
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
    // and agent config's allowed ep-attribute-set
    static map<string,string> createLabelMapFromEpAttr(const string& ep_name_,
                           const unordered_map<string, string>&      attr_map,
                           const unordered_set<string>&          allowed_set);
    // Maximum number of labels that can be used for annotating a metric
    static int max_metric_attr_count;
    /* End of EpCounter related apis and state */


    /* Start of SvcTargetCounter related apis and state */
    // Lock to safe guard SvcTargetCounter related state
    mutex svc_target_counter_mutex;

    // create any svc target gauge metric families during start
    void createStaticGaugeFamiliesSvcTarget(void);
    // remove any svc target gauge metric families during stop
    void removeStaticGaugeFamiliesSvcTarget(void);

    enum SVC_TARGET_METRICS {
        SVC_TARGET_METRICS_MIN,
        SVC_TARGET_RX_BYTES = SVC_TARGET_METRICS_MIN,
        SVC_TARGET_RX_PKTS,
        SVC_TARGET_TX_BYTES,
        SVC_TARGET_TX_PKTS,
        SVC_TARGET_METRICS_MAX = SVC_TARGET_TX_PKTS
    };

    // metric families to track all SvcTargetCounter metrics
    Family<Gauge>      *gauge_svc_target_family_ptr[SVC_TARGET_METRICS_MAX+1];

    // Dynamic Metric families and metrics
    // CRUD for every SvcTarget counter metric
    // func to create gauge for SvcTargetCounter given metric type,
    // uuid of svc-target & attr map of ep
    void createDynamicGaugeSvcTarget(SVC_TARGET_METRICS metric,
                                     const string& uuid,
                                     const string& nhip,
        const unordered_map<string, string>& svc_attr_map,
        const unordered_map<string, string>& ep_attr_map,
                                     bool updateLabels);

    // func to get label map and Gauge for SvcTargetCounter given metric type, uuid
    mgauge_pair_t getDynamicGaugeSvcTarget(SVC_TARGET_METRICS metric, const string& uuid);

    // func to remove gauge for SvcTargetCounter given metric type, uuid
    bool removeDynamicGaugeSvcTarget(SVC_TARGET_METRICS metric, const string& uuid);
    // func to remove all gauge of every SvcTargetCounter for a metric type
    void removeDynamicGaugeSvcTarget(SVC_TARGET_METRICS metric);
    // func to remove all gauges of every SvcTargetCounter
    void removeDynamicGaugeSvcTarget(void);

    /**
     * cache the label map and Gauge ptr for every service target uuid
     */
    unordered_map<string, mgauge_pair_t> svc_target_gauge_map[SVC_TARGET_METRICS_MAX+1];

    //Utility apis
    // Create a label map that can be used for annotation, given the ep attr map
    static const map<string,string> createLabelMapFromSvcTargetAttr(
                                                          const string& nhip,
                           const unordered_map<string, string>&  svc_attr_map,
                           const unordered_map<string, string>&  ep_attr_map);
    /* End of SvcTargetCounter related apis and state */


    /* Start of SvcCounter related apis and state */
    // Lock to safe guard SvcCounter related state
    mutex svc_counter_mutex;

    // Counter family to track all SvcCounter creates
    Family<Counter>    *counter_svc_create_family_ptr;
    // Counter family to track all SvcCounter removes
    Family<Counter>    *counter_svc_remove_family_ptr;
    // Gauge family to track the  total # of SvcCounters
    Family<Gauge>      *gauge_svc_total_family_ptr;
    // Counter to track svc creates
    Counter            *counter_svc_create_ptr;
    // Counter to track svc removes
    Counter            *counter_svc_remove_ptr;
    // Gauge to track total SVCs
    Gauge              *gauge_svc_total_ptr;
    // Actual svc total count
    double              gauge_svc_total;

    // func to increment SvcCounter create
    void incStaticCounterSvcCreate(void);
    // func to decrement SvcCounter remove
    void incStaticCounterSvcRemove(void);
    // func to set total SvcCounter
    void updateStaticGaugeSvcTotal(bool add);
    // create any svc gauge metric families during start
    void createStaticGaugeFamiliesSvc(void);
    // remove any svc gauge metric families during stop
    void removeStaticGaugeFamiliesSvc(void);
    // create any svc counter metric families during start
    void createStaticCounterFamiliesSvc(void);
    // remove any svc counter metric families during stop
    void removeStaticCounterFamiliesSvc(void);
    // create any svc gauge metric during start
    void createStaticGaugesSvc(void);
    // remove any svc gauge metric during stop
    void removeStaticGaugesSvc(void);
    // create any svc counter metric during start
    void createStaticCountersSvc(void);
    // remove any svc counter metric during stop
    void removeStaticCountersSvc(void);

    enum SVC_METRICS {
        SVC_METRICS_MIN,
        SVC_RX_BYTES = SVC_METRICS_MIN,
        SVC_RX_PKTS,
        SVC_TX_BYTES,
        SVC_TX_PKTS,
        SVC_METRICS_MAX = SVC_TX_PKTS
    };

    // metric families to track all SvcCounter metrics
    Family<Gauge>      *gauge_svc_family_ptr[SVC_METRICS_MAX+1];

    // Dynamic Metric families and metrics
    // CRUD for every Svc counter metric
    // func to create gauge for SvcCounter given metric type,
    // uuid of svc & attr map of svc
    void createDynamicGaugeSvc(SVC_METRICS metric,
                               const string& uuid,
        const unordered_map<string, string>& svc_attr_map);

    // func to get label map and Gauge for SvcCounter given metric type, uuid
    mgauge_pair_t getDynamicGaugeSvc(SVC_METRICS metric, const string& uuid);

    // func to remove gauge for SvcCounter given metric type, uuid
    bool removeDynamicGaugeSvc(SVC_METRICS metric, const string& uuid);
    // func to remove all gauge of every SvcCounter for a metric type
    void removeDynamicGaugeSvc(SVC_METRICS metric);
    // func to remove all gauges of every SvcCounter
    void removeDynamicGaugeSvc(void);

    /**
     * cache the label map and Gauge ptr for every service uuid
     */
    unordered_map<string, mgauge_pair_t> svc_gauge_map[SVC_METRICS_MAX+1];

    //Utility apis
    // Create a label map that can be used for annotation, given the svc attr map
    static const map<string,string> createLabelMapFromSvcAttr(
                           const unordered_map<string, string>&  svc_attr_map);
    /* End of SvcCounter related apis and state */


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
    // func to dump PodSvcCounter metric state
    void dumpPodSvcState(void);

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


    /* Start of RemoteEp related apis and state */
    // Lock to safe guard RemoteEp related state
    mutex remote_ep_mutex;

    enum REMOTE_EP_METRICS {
        REMOTE_EP_METRICS_MIN,
        REMOTE_EP_COUNT = REMOTE_EP_METRICS_MIN,
        REMOTE_EP_METRICS_MAX = REMOTE_EP_COUNT
    };

    // Static Metric families and metrics
    // metric families to track all RemoteEp metrics
    Family<Gauge>      *gauge_remote_ep_family_ptr[REMOTE_EP_METRICS_MAX+1];

    // create any remote ep stats gauge metric families during start
    void createStaticGaugeFamiliesRemoteEp(void);
    // remove any remote ep stats gauge metric families during stop
    void removeStaticGaugeFamiliesRemoteEp(void);

    // Dynamic Metric families and metrics
    // CRUD for every Remote ep metric
    // func to create gauge for remote ep given metric type
    void createDynamicGaugeRemoteEp(REMOTE_EP_METRICS metric);

    // func to get label map and Gauge for RemoteEp given metric type
    Gauge * getDynamicGaugeRemoteEp(REMOTE_EP_METRICS metric);

    // func to remove gauge for RemoteEp given metric type
    bool removeDynamicGaugeRemoteEp(REMOTE_EP_METRICS metric);
    // func to remove all gauges of every RemoteEp
    void removeDynamicGaugeRemoteEp(void);

    /**
     * cache Gauge ptr for every RemoteEp metric
     */
    Gauge* remote_ep_gauge_map[REMOTE_EP_METRICS_MAX+1];
    /* End of RemoteEp related apis and state */


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
    uint64_t rddrop_last_genId;

    /**
     * cache Gauge ptr for every RDDropCounter metric
     */
    unordered_map<string, Gauge*> rddrop_gauge_map[RDDROP_METRICS_MAX+1];
    /* End of RDDropCounter related apis and state */

    /* Start of TableDropCounter related apis and state */
    // Lock to safe guard TableDropCounter related state
    mutex table_drop_counter_mutex;

    enum TABLE_DROP_METRICS {
        TABLE_DROP_METRICS_MIN,
        TABLE_DROP_MIN = TABLE_DROP_METRICS_MIN,
        TABLE_DROP_BYTES = TABLE_DROP_MIN,
        TABLE_DROP_PKTS,
        TABLE_DROP_MAX = TABLE_DROP_PKTS,
        TABLE_DROP_METRICS_MAX = TABLE_DROP_MAX
    };

    // Static Metric families and metrics
    // metric families to track all TableDrop metrics
    Family<Gauge>      *gauge_table_drop_family_ptr[TABLE_DROP_METRICS_MAX+1];

    // create table drop gauge metric families during start
    void createStaticGaugeFamiliesTableDrop(void);

    // func to get label map and Gauge for TableDrop given metric type, bridge/table-name
    mgauge_pair_t getStaticGaugeTableDrop(TABLE_DROP_METRICS metric,
                                          const string& bridge_name,
                                          const string& table_name);

    // Create TableDrop gauge given metric type, bridge name and table name
    void createStaticGaugeTableDrop (const string& bridge_name,
                                     const string& table_name);

    void removeStaticGaugesTableDrop ();

    void removeStaticGaugeFamiliesTableDrop();
    /**
     * cache the label map and Gauge ptr for every table drop
     */
    unordered_map<string, mgauge_pair_t> table_drop_gauge_map[TABLE_DROP_METRICS_MAX+1];

    //Utility apis
    // Create a label map that can be used for annotation, given the bridge and table name
    static const map<string,string> createLabelMapFromTableDropKey(
            const string& bridge_name,
            const string& table_name);
    /* End of TableDropCounter related apis and state */

    /* Start of SGClassifierCounter related apis and state */
    // Lock to safe guard SGClassifierCounter related state
    mutex sgclassifier_stats_mutex;

    enum SGCLASSIFIER_METRICS {
        SGCLASSIFIER_METRICS_MIN,
        SGCLASSIFIER_TX_BYTES = SGCLASSIFIER_METRICS_MIN,
        SGCLASSIFIER_TX_PACKETS,
        SGCLASSIFIER_RX_BYTES,
        SGCLASSIFIER_RX_PACKETS,
        SGCLASSIFIER_METRICS_MAX = SGCLASSIFIER_RX_PACKETS
    };

    // Static Metric families and metrics
    // metric families to track all SGClassifierCounter metrics
    Family<Gauge>      *gauge_sgclassifier_family_ptr[SGCLASSIFIER_METRICS_MAX+1];

    // create any sgclassifier stats gauge metric families during start
    void createStaticGaugeFamiliesSGClassifier(void);
    // remove any sgclassifier stats gauge metric families during stop
    void removeStaticGaugeFamiliesSGClassifier(void);

    // Dynamic Metric families and metrics
    // CRUD for every SGClassifier counter metric
    // func to create gauge for SGClassifier given metric type,
    // name of classifier.
    // return false if the metric is already created
    bool createDynamicGaugeSGClassifier(SGCLASSIFIER_METRICS metric,
                                        const string& classifier);

    // func to get label map and Gauge for SGClassifierCounter given
    // metric type, name of classifier
    Gauge * getDynamicGaugeSGClassifier(SGCLASSIFIER_METRICS metric,
                                        const string& classifier);

    // func to remove gauge for SGClassifierCounter given metric type,
    // name of classifier
    bool removeDynamicGaugeSGClassifier(SGCLASSIFIER_METRICS metric,
                                        const string& classifier);
    // func to remove all gauge of every SGClassifierCounter for a metric type
    void removeDynamicGaugeSGClassifier(SGCLASSIFIER_METRICS metric);
    // func to remove all gauges of every SGClassifierCounter
    void removeDynamicGaugeSGClassifier(void);

    // SGClassifierCounter diffs are stored in a circular buffer. Each buffer element
    // has a unique running genID. Keep track of the last processed genId by
    // PrometheusManager to avoid double counting
    uint64_t sgclassifier_last_genId;

    /**
     * cache Gauge ptr for every SGClassifierCounter metric
     */
    unordered_map<string, Gauge*> sgclassifier_gauge_map[SGCLASSIFIER_METRICS_MAX+1];

    // Utility APIs
    // API to compress a classifier to human readable format
    string stringizeClassifier(const string& tenant,
                               const string& classifier);
    // API to construct a label based out of classifier URI and flag to indicate
    // contract vs secGrp
    string constructClassifierLabel(const string& classifier, bool isSecGrp);
    /* End of SGClassifierCounter related apis and state */


    /* Start of ContractClassifierCounter related apis and state */
    // Lock to safe guard ContractClassifierCounter related state
    mutex contract_stats_mutex;

    enum CONTRACT_METRICS {
        CONTRACT_METRICS_MIN,
        CONTRACT_BYTES = CONTRACT_METRICS_MIN,
        CONTRACT_PACKETS,
        CONTRACT_METRICS_MAX = CONTRACT_PACKETS
    };

    // Static Metric families and metrics
    // metric families to track all ContractClassifierCounter metrics
    Family<Gauge>      *gauge_contract_family_ptr[CONTRACT_METRICS_MAX+1];

    // create any contract stats gauge metric families during start
    void createStaticGaugeFamiliesContractClassifier(void);
    // remove any contract stats gauge metric families during stop
    void removeStaticGaugeFamiliesContractClassifier(void);

    // Dynamic Metric families and metrics
    // CRUD for every ContractClassifier counter metric
    // func to create gauge for ContractClassifier given metric type,
    // name of srcEpg, dstEpg, & classifier.
    // return false if the metric is already created
    bool createDynamicGaugeContractClassifier(CONTRACT_METRICS metric,
                                              const string& srcEpg,
                                              const string& dstEpg,
                                              const string& classifier);

    // func to get label map and Gauge for ContractClassifierCounter given
    // metric type, name of srcEpg, dstEpg, & classifier
    Gauge * getDynamicGaugeContractClassifier(CONTRACT_METRICS metric,
                                              const string& srcEpg,
                                              const string& dstEpg,
                                              const string& classifier);

    // func to remove gauge for ContractClassifierCounter given metric type,
    // name of srcEpg, dstEpg & classifier
    bool removeDynamicGaugeContractClassifier(CONTRACT_METRICS metric,
                                              const string& srcEpg,
                                              const string& dstEpg,
                                              const string& classifier);
    // func to remove all gauge of every ContractClassifierCounter for a metric type
    void removeDynamicGaugeContractClassifier(CONTRACT_METRICS metric);
    // func to remove all gauges of every ContractClassifierCounter
    void removeDynamicGaugeContractClassifier(void);

    // ContractClassifierCounter diffs are stored in a circular buffer. Each buffer element
    // has a unique running genID. Keep track of the last processed genId by
    // PrometheusManager to avoid double counting
    uint64_t contract_last_genId;

    /**
     * cache Gauge ptr for every ContractClassifierCounter metric
     */
    unordered_map<string, Gauge*> contract_gauge_map[CONTRACT_METRICS_MAX+1];

    // Utility APIs
    // API to construct a label based out of EPG URI
    string constructEpgLabel(const string& epg);
    /* End of ContractClassifierCounter related apis and state */


    // PrometheusManager specific state
    /**
     * True if shutting down or not start()'ed
     */
    std::atomic<bool> disabled;

    /* TODO: Other Counter related apis and state */
};

} /* namespace opflexagent */

#endif // __OPFLEXAGENT_PROMETHEUS_MANAGER_H__
