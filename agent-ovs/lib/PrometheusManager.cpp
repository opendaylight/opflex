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

#include <opflex/modb/Mutator.h>
#include <opflexagent/logging.h>
#include <opflexagent/Agent.h>
#include <opflexagent/PrometheusManager.h>
#include <opflexagent/EndpointManager.h>
#include <modelgbp/gbpe/L24Classifier.hpp>
#include <map>
#include <boost/optional.hpp>
#include <regex>

#include <prometheus/gauge.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/detail/utils.h>

namespace opflexagent {

using std::to_string;
using std::lock_guard;
using std::regex;
using std::regex_match;
using std::make_shared;
using std::make_pair;
using namespace prometheus::detail;

static string ep_family_names[] =
{
  "opflex_endpoint_rx_bytes",
  "opflex_endpoint_rx_packets",
  "opflex_endpoint_rx_drop_packets",
  "opflex_endpoint_rx_ucast_packets",
  "opflex_endpoint_rx_mcast_packets",
  "opflex_endpoint_rx_bcast_packets",
  "opflex_endpoint_tx_packets",
  "opflex_endpoint_tx_bytes",
  "opflex_endpoint_tx_drop_packets",
  "opflex_endpoint_tx_ucast_packets",
  "opflex_endpoint_tx_mcast_packets",
  "opflex_endpoint_tx_bcast_packets"
};

static string ep_family_help[] =
{
  "Local endpoint rx bytes",
  "Local endpoint rx packets",
  "Local endpoint rx drop packets",
  "Local endpoint rx unicast packets",
  "Local endpoint rx multicast packets",
  "Local endpoint rx broadcast packets",
  "Local endpoint tx packets",
  "Local endpoint tx bytes",
  "Local endpoint tx drop packets",
  "Local endpoint tx unicast packets",
  "Local endpoint tx multicast packets",
  "Local endpoint tx broadcast packets"
};

static string podsvc_family_names[] =
{
  "opflex_endpoint_to_svc_bytes",
  "opflex_endpoint_to_svc_packets",
  "opflex_svc_to_endpoint_bytes",
  "opflex_svc_to_endpoint_packets"
};

static string podsvc_family_help[] =
{
  "endpoint to service bytes",
  "endpoint to service packets",
  "service to endpoint bytes",
  "service to endpoint packets"
};

static string ofpeer_family_names[] =
{
  "opflex_peer_identity_req_count",
  "opflex_peer_identity_resp_count",
  "opflex_peer_identity_err_count",
  "opflex_peer_policy_resolve_req_count",
  "opflex_peer_policy_resolve_resp_count",
  "opflex_peer_policy_resolve_err_count",
  "opflex_peer_policy_unresolve_req_count",
  "opflex_peer_policy_unresolve_resp_count",
  "opflex_peer_policy_unresolve_err_count",
  "opflex_peer_policy_update_receive_count",
  "opflex_peer_ep_declare_req_count",
  "opflex_peer_ep_declare_resp_count",
  "opflex_peer_ep_declare_err_count",
  "opflex_peer_ep_undeclare_req_count",
  "opflex_peer_ep_undeclare_resp_count",
  "opflex_peer_ep_undeclare_err_count",
  "opflex_peer_state_report_req_count",
  "opflex_peer_state_report_resp_count",
  "opflex_peer_state_report_err_count"
};

static string ofpeer_family_help[] =
{
  "number of identity requests sent to opflex peer",
  "number of identity responses received from opflex peer",
  "number of identity error responses from opflex peer",
  "number of policy resolves sent to opflex peer",
  "number of policy resolve responses received from opflex peer",
  "number of policy resolve error responses from opflex peer",
  "number of policy unresolves sent to opflex peer",
  "number of policy unresolve responses received from opflex peer",
  "number of policy unresolve error responses from opflex peer",
  "number of policy updates received from opflex peer",
  "number of endpoint declares sent to opflex peer",
  "number of endpoint declare responses received from opflex peer",
  "number of endpoint declare error responses from opflex peer",
  "number of endpoint undeclares sent to opflex peer",
  "number of endpoint undeclare responses received from opflex peer",
  "number of endpoint undeclare error responses from opflex peer",
  "number of state reports sent to opflex peer",
  "number of state reports responses received from opflex peer",
  "number of state reports error repsonses from opflex peer"
};

static string rddrop_family_names[] =
{
  "opflex_policy_drop_bytes",
  "opflex_policy_drop_packets"
};

static string rddrop_family_help[] =
{
  "number of policy/contract dropped bytes per routing domain",
  "number of policy/contract dropped packets per routing domain"
};

static string sgclassifier_family_names[] =
{
  "opflex_hpp_tx_bytes",
  "opflex_hpp_tx_packets",
  "opflex_hpp_rx_bytes",
  "opflex_hpp_rx_packets"
};

static string sgclassifier_family_help[] =
{
  "security-group/host-protection-policy classifier tx bytes",
  "security-group/host-protection-policy classifier tx packets",
  "security-group/host-protection-policy classifier rx bytes",
  "security-group/host-protection-policy classifier rx packets"
};

static string metric_annotate_skip[] =
{
  "vm-name",
  "namespace",
  "interface-name",
  "pod-template-hash",
  "controller-revision-hash",
  "pod-template-generation"
};

// construct PrometheusManager
PrometheusManager::PrometheusManager(Agent &agent_,
                                     opflex::ofcore::OFFramework &fwk_) :
                                     agent(agent_),
                                     framework(fwk_),
                                     gauge_ep_total{0},
                                     rddrop_last_genId{0},
                                     sgclassifier_last_genId{0} {}

// create all ep counter families during start
void PrometheusManager::createStaticCounterFamiliesEp (void)
{
    // add a new counter family to the registry (families combine values with the
    // same name, but distinct label dimensions)
    // Note: There is a unique ptr allocated and referencing the below reference
    // during Register().

    /* Counter family to track the total calls made to EpCounter update/remove
     * from other clients */
    auto& counter_ep_create_family = BuildCounter()
                         .Name("opflex_ep_created_total")
                         .Help("Total number of local EP creates")
                         .Labels({})
                         .Register(*registry_ptr);
    counter_ep_create_family_ptr = &counter_ep_create_family;

    auto& counter_ep_remove_family = BuildCounter()
                         .Name("opflex_ep_removed_total")
                         .Help("Total number of local EP deletes")
                         .Labels({})
                         .Register(*registry_ptr);
    counter_ep_remove_family_ptr = &counter_ep_remove_family;
}



// create all counter families during start
void PrometheusManager::createStaticCounterFamilies (void)
{
    // EpCounter families
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        createStaticCounterFamiliesEp();
    }
}

// create all static ep counters during start
void PrometheusManager::createStaticCountersEp ()
{
    auto& counter_ep_create = counter_ep_create_family_ptr->Add({});
    counter_ep_create_ptr = &counter_ep_create;

    auto& counter_ep_remove = counter_ep_remove_family_ptr->Add({});
    counter_ep_remove_ptr = &counter_ep_remove;
}

// create all static counters during start
void PrometheusManager::createStaticCounters ()
{
    // EpCounter related metrics
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        createStaticCountersEp();
    }
}

// remove all dynamic counters during stop
void PrometheusManager::removeDynamicCounters ()
{
    // No dynamic counters as of now
}

// remove all dynamic counters during stop
void PrometheusManager::removeDynamicGauges ()
{
    // Remove EpCounter related gauges
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        removeDynamicGaugeEp();
    }

    // Remove PodSvcCounter related gauges
    {
        const lock_guard<mutex> lock(podsvc_counter_mutex);
        removeDynamicGaugePodSvc();
    }

    // Remove OFPeerStat related gauges
    {
        const lock_guard<mutex> lock(ofpeer_stats_mutex);
        removeDynamicGaugeOFPeer();
    }

    // Remove RDDropCounter related gauges
    {
        const lock_guard<mutex> lock(rddrop_stats_mutex);
        removeDynamicGaugeRDDrop();
    }

    // Remove SGClassifierCounter related gauges
    {
        const lock_guard<mutex> lock(sgclassifier_stats_mutex);
        removeDynamicGaugeSGClassifier();
    }
}

// remove all static ep counters during stop
void PrometheusManager::removeStaticCountersEp ()
{
    counter_ep_create_family_ptr->Remove(counter_ep_create_ptr);
    counter_ep_create_ptr = nullptr;

    counter_ep_remove_family_ptr->Remove(counter_ep_remove_ptr);
    counter_ep_remove_ptr = nullptr;
}

// remove all static counters during stop
void PrometheusManager::removeStaticCounters ()
{

    // Remove EpCounter related counter metrics
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        removeStaticCountersEp();
    }
}

// create all OFPeer specific gauge families during start
void PrometheusManager::createStaticGaugeFamiliesOFPeer (void)
{
    // add a new gauge family to the registry (families combine values with the
    // same name, but distinct label dimensions)
    // Note: There is a unique ptr allocated and referencing the below reference
    // during Register().

    for (OFPEER_METRICS metric=OFPEER_METRICS_MIN;
            metric <= OFPEER_METRICS_MAX;
                metric = OFPEER_METRICS(metric+1)) {
        auto& gauge_ofpeer_family = BuildGauge()
                             .Name(ofpeer_family_names[metric])
                             .Help(ofpeer_family_help[metric])
                             .Labels({})
                             .Register(*registry_ptr);
        gauge_ofpeer_family_ptr[metric] = &gauge_ofpeer_family;
    }
}

// create all SGClassifier specific gauge families during start
void PrometheusManager::createStaticGaugeFamiliesSGClassifier (void)
{
    // add a new gauge family to the registry (families combine values with the
    // same name, but distinct label dimensions)
    // Note: There is a unique ptr allocated and referencing the below reference
    // during Register().

    for (SGCLASSIFIER_METRICS metric=SGCLASSIFIER_METRICS_MIN;
            metric <= SGCLASSIFIER_METRICS_MAX;
                metric = SGCLASSIFIER_METRICS(metric+1)) {
        auto& gauge_sgclassifier_family = BuildGauge()
                             .Name(sgclassifier_family_names[metric])
                             .Help(sgclassifier_family_help[metric])
                             .Labels({})
                             .Register(*registry_ptr);
        gauge_sgclassifier_family_ptr[metric] = &gauge_sgclassifier_family;
    }
}

// create all RDDrop specific gauge families during start
void PrometheusManager::createStaticGaugeFamiliesRDDrop (void)
{
    // add a new gauge family to the registry (families combine values with the
    // same name, but distinct label dimensions)
    // Note: There is a unique ptr allocated and referencing the below reference
    // during Register().

    for (RDDROP_METRICS metric=RDDROP_METRICS_MIN;
            metric <= RDDROP_METRICS_MAX;
                metric = RDDROP_METRICS(metric+1)) {
        auto& gauge_rddrop_family = BuildGauge()
                             .Name(rddrop_family_names[metric])
                             .Help(rddrop_family_help[metric])
                             .Labels({})
                             .Register(*registry_ptr);
        gauge_rddrop_family_ptr[metric] = &gauge_rddrop_family;
    }
}

// create all EP specific gauge families during start
void PrometheusManager::createStaticGaugeFamiliesEp (void)
{
    // add a new gauge family to the registry (families combine values with the
    // same name, but distinct label dimensions)
    // Note: There is a unique ptr allocated and referencing the below reference
    // during Register().

    auto& gauge_ep_total_family = BuildGauge()
                         .Name("opflex_active_local_endpoints")
                         .Help("Total active local end point count")
                         .Labels({})
                         .Register(*registry_ptr);
    gauge_ep_total_family_ptr = &gauge_ep_total_family;

    for (EP_METRICS metric=EP_RX_BYTES;
            metric < EP_METRICS_MAX;
                metric = EP_METRICS(metric+1)) {
        auto& gauge_ep_family = BuildGauge()
                             .Name(ep_family_names[metric])
                             .Help(ep_family_help[metric])
                             .Labels({})
                             .Register(*registry_ptr);
        gauge_ep_family_ptr[metric] = &gauge_ep_family;
    }
}

// create all PODSVC specific gauge families during start
void PrometheusManager::createStaticGaugeFamiliesPodSvc (void)
{
    // add a new gauge family to the registry (families combine values with the
    // same name, but distinct label dimensions)
    // Note: There is a unique ptr allocated and referencing the below reference
    // during Register().

    for (PODSVC_METRICS metric=PODSVC_METRICS_MIN;
            metric <= PODSVC_METRICS_MAX;
                metric = PODSVC_METRICS(metric+1)) {
        auto& gauge_podsvc_family = BuildGauge()
                             .Name(podsvc_family_names[metric])
                             .Help(podsvc_family_help[metric])
                             .Labels({})
                             .Register(*registry_ptr);
        gauge_podsvc_family_ptr[metric] = &gauge_podsvc_family;
    }
}

// create all gauge families during start
void PrometheusManager::createStaticGaugeFamilies (void)
{
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        createStaticGaugeFamiliesEp();
    }

    {
        const lock_guard<mutex> lock(podsvc_counter_mutex);
        createStaticGaugeFamiliesPodSvc();
    }

    {
        const lock_guard<mutex> lock(ofpeer_stats_mutex);
        createStaticGaugeFamiliesOFPeer();
    }

    {
        const lock_guard<mutex> lock(rddrop_stats_mutex);
        createStaticGaugeFamiliesRDDrop();
    }

    {
        const lock_guard<mutex> lock(sgclassifier_stats_mutex);
        createStaticGaugeFamiliesSGClassifier();
    }
}

// create EpCounter gauges during start
void PrometheusManager::createStaticGaugesEp ()
{
    auto& gauge_ep_total = gauge_ep_total_family_ptr->Add({});
    gauge_ep_total_ptr = &gauge_ep_total;
}

// create gauges during start
void PrometheusManager::createStaticGauges ()
{
    // EpCounter related gauges
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        createStaticGaugesEp();
    }
}

// remove ep gauges during stop
void PrometheusManager::removeStaticGaugesEp ()
{
    gauge_ep_total_family_ptr->Remove(gauge_ep_total_ptr);
    gauge_ep_total_ptr = nullptr;
    gauge_ep_total = 0;
}

// remove gauges during stop
void PrometheusManager::removeStaticGauges ()
{

    // Remove EpCounter related gauge metrics
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        removeStaticGaugesEp();
    }
}

// Start of PrometheusManager instance
void PrometheusManager::start()
{
    LOG(DEBUG) << "starting prometheus manager";
    /**
     * create an http server running on port 9612
     * Note: The third argument is the total worker thread count. Prometheus
     * follows boss-worker thread model. 1 boss thread will get created to
     * intercept HTTP requests. The requests will then be serviced by free
     * worker threads. We are using 1 worker thread to service the requests.
     * Note: Port #9612 has been reserved for opflex here:
     * https://github.com/prometheus/prometheus/wiki/Default-port-allocations
     */
    exposer_ptr = unique_ptr<Exposer>(new Exposer{"9612", "/metrics", 1});
    registry_ptr = make_shared<Registry>();

    /* Initialize Metric families which can be created during
     * init time */
    createStaticCounterFamilies();
    createStaticGaugeFamilies();

    // Add static metrics
    createStaticCounters();
    createStaticGauges();

    // ask the exposer to scrape the registry on incoming scrapes
    exposer_ptr->RegisterCollectable(registry_ptr);
}

// Stop of PrometheusManager instance
void PrometheusManager::stop()
{
    LOG(DEBUG) << "stopping prometheus manager";

    // Gracefully delete state

    // Remove metrics
    removeDynamicGauges();
    removeDynamicCounters();
    removeStaticCounters();
    removeStaticGauges();

    // Remove metric families
    removeStaticCounterFamilies();
    removeStaticGaugeFamilies();
    removeDynamicCounterFamilies();
    removeDynamicGaugeFamilies();

    exposer_ptr.reset();
    exposer_ptr = nullptr;

    registry_ptr.reset();
    registry_ptr = nullptr;
}

// Increment Ep count
void PrometheusManager::incStaticCounterEpCreate ()
{
    counter_ep_create_ptr->Increment();
}

// decrement ep count
void PrometheusManager::incStaticCounterEpRemove ()
{
    counter_ep_remove_ptr->Increment();
}

// track total ep count
void PrometheusManager::updateStaticGaugeEpTotal (bool add)
{
    if (add)
        gauge_ep_total_ptr->Set(++gauge_ep_total);
    else
        gauge_ep_total_ptr->Set(--gauge_ep_total);
}

// Check if a given metric name is Prometheus compatible
bool PrometheusManager::checkMetricName (const string& metric_name)
{
    // Prometheus doesnt like anything other than [a-zA-Z_:][a-zA-Z0-9_:]*
    // https://prometheus.io/docs/concepts/data_model/
    static const regex metric_name_regex("[a-zA-Z_:][a-zA-Z0-9_:]*");
    return regex_match(metric_name, metric_name_regex);
}

// sanitize metric family name for prometheus to accept
string PrometheusManager::sanitizeMetricName (string metric_name)
{
    char replace_from = '-', replace_to = '_';
    size_t found = metric_name.find_first_of(replace_from);

    // Prometheus doesnt like anything other than [a-zA-Z_:][a-zA-Z0-9_:]*
    // https://prometheus.io/docs/concepts/data_model/
    while (found != string::npos) {
        metric_name[found] = replace_to;
        found = metric_name.find_first_of(replace_from, found+1);
    }

    return metric_name;
}

// Create OFPeerStats gauge given metric type, peer (IP,port) tuple
void PrometheusManager::createDynamicGaugeOFPeer (OFPEER_METRICS metric,
                                                  const string& peer)
{
    // Retrieve the Gauge if its already created
    if (getDynamicGaugeOFPeer(metric, peer))
        return;

    LOG(DEBUG) << "creating ofpeer dyn gauge family"
               << " metric: " << metric
               << " peer: " << peer;
    auto& gauge = gauge_ofpeer_family_ptr[metric]->Add({{"peer", peer}});
    ofpeer_gauge_map[metric][peer] = &gauge;
}

// Create SGClassifierCounter gauge given metric type, classifier
bool PrometheusManager::createDynamicGaugeSGClassifier (SGCLASSIFIER_METRICS metric,
                                                        const string& classifier)
{
    // Retrieve the Gauge if its already created
    if (getDynamicGaugeSGClassifier(metric, classifier))
        return false;

    LOG(DEBUG) << "creating sgclassifier dyn gauge family"
               << " metric: " << metric
               << " classifier: " << classifier;

    /* Example classifier:
     * /PolicyUniverse/PolicySpace/common/GbpeL24Classifier/34%7c2%7cIPv6/
     * We want to produce these annotations for every metric:
     * label_map: {{name: gibberishName},
     *             {classifier: <string version of classifier>}}
     */
    std::size_t gL24Start = classifier.rfind("GbpeL24Classifier");
    std::size_t nHigh = classifier.size()-2;
    std::size_t nLow = gL24Start+18;
    std::string name = classifier.substr(nLow,nHigh-nLow+1);

    const auto& compressed = stringizeClassifier(classifier);
    auto& gauge = gauge_sgclassifier_family_ptr[metric]->Add(
                    {{"name", name},
                     {"classifier", compressed}});
    sgclassifier_gauge_map[metric][classifier] = &gauge;
    return true;
}

// Get a compressed human readable form of classifier
string PrometheusManager::stringizeClassifier (const string& classifier)
{
    using namespace modelgbp::gbpe;
    string compressed;
    const auto& counter = L24Classifier::resolve(agent.getFramework(),
                                                 "PolicySpace",
                                                 classifier);
    if (counter) {
        auto arp_opc = counter.get()->getArpOpc();
        compressed += "arp_opc:";
        if (arp_opc)
            compressed += to_string(arp_opc.get());

        auto etype = counter.get()->getEtherT();
        compressed += ",etype:";
        if (etype)
            compressed += to_string(etype.get());

        auto proto = counter.get()->getProt();
        compressed += ",proto:";
        if (proto)
            compressed += to_string(proto.get());

        auto s_from_port = counter.get()->getSFromPort();
        compressed += ",sport:";
        if (s_from_port)
            compressed += to_string(s_from_port.get());

        auto s_to_port = counter.get()->getSToPort();
        compressed += "-";
        if (s_to_port)
            compressed += to_string(s_to_port.get());

        auto d_from_port = counter.get()->getDFromPort();
        compressed += ",dport:";
        if (d_from_port)
            compressed += to_string(d_from_port.get());

        auto d_to_port = counter.get()->getDToPort();
        compressed += "-";
        if (d_to_port)
            compressed += to_string(d_to_port.get());

        auto frag_flags = counter.get()->getFragmentFlags();
        compressed += ",frag_flags:";
        if (frag_flags)
            compressed += to_string(frag_flags.get());

        auto icmp_code = counter.get()->getIcmpCode();
        compressed += ",icmp_code:";
        if (icmp_code)
            compressed += to_string(icmp_code.get());

        auto icmp_type = counter.get()->getIcmpType();
        compressed += ",icmp_type:";
        if (icmp_type)
            compressed += to_string(icmp_type.get());

        auto tcp_flags = counter.get()->getTcpFlags();
        compressed += ",tcp_flags:";
        if (tcp_flags)
            compressed += to_string(tcp_flags.get());

        auto ct = counter.get()->getConnectionTracking();
        compressed += ",ct:";
        if (ct)
            compressed += to_string(ct.get());
    }

    return compressed;
}

// Create RDDropCounter gauge given metric type, rdURI
void PrometheusManager::createDynamicGaugeRDDrop (RDDROP_METRICS metric,
                                                  const string& rdURI)
{
    // Retrieve the Gauge if its already created
    if (getDynamicGaugeRDDrop(metric, rdURI))
        return;

    LOG(DEBUG) << "creating rddrop dyn gauge family"
               << " metric: " << metric
               << " rdURI: " << rdURI;

    /* Example rdURI: /PolicyUniverse/PolicySpace/test/GbpRoutingDomain/rd/
     * We want to just get the tenant name and the vrf. */
    std::size_t tLow = rdURI.find("PolicySpace") + 12;
    std::size_t gRDStart = rdURI.rfind("GbpRoutingDomain");
    std::string tenant = rdURI.substr(tLow,gRDStart-tLow-1);
    std::size_t rHigh = rdURI.size()-2;
    std::size_t rLow = gRDStart+17;
    std::string rd = rdURI.substr(rLow,rHigh-rLow+1);

    auto& gauge = gauge_rddrop_family_ptr[metric]->Add({{"routing_domain",
                                                         tenant+":"+rd}});
    rddrop_gauge_map[metric][rdURI] = &gauge;
}

// Create PodSvcCounter gauge given metric type, ep+svc uuid & attr_maps
void PrometheusManager::createDynamicGaugePodSvc (PODSVC_METRICS metric,
                                                  const string& uuid,
                    const unordered_map<string, string>&    ep_attr_map,
                    const unordered_map<string, string>&    svc_attr_map)
{
    // During counter update from stats manager, dont create new gauge metric
    if ((ep_attr_map.size() == 0) && (svc_attr_map.size() == 0))
        return;

    auto const &label_map = createLabelMapFromPodSvcAttr(ep_attr_map, svc_attr_map);
    auto hash_new = hash_labels(label_map);

    // Retrieve the Gauge if its already created
    auto const &mgauge = getDynamicGaugePodSvc(metric, uuid);
    if (mgauge) {
        /**
         * Detect attribute change by comparing hashes of cached label map
         * with new label map
         */
        if (hash_new == hash_labels(mgauge.get().first))
            return;
        else {
            LOG(DEBUG) << "addNupdate podsvccounter uuid " << uuid
                       << "existing podsvc metric, but deleting: hash modified;"
                       << " metric: " << podsvc_family_names[metric]
                       << " gaugeptr: " << mgauge.get().second;
            removeDynamicGaugePodSvc(metric, uuid);
        }
    }

    // We shouldnt add a gauge for PodSvc which doesnt have
    // ep name and svc name.
    if (!hash_new) {
        LOG(ERROR) << "label map is empty for podsvc dyn gauge family"
               << " metric: " << metric
               << " uuid: " << uuid;
        return;
    }

    LOG(DEBUG) << "creating podsvc dyn gauge family"
               << " metric: " << metric
               << " uuid: " << uuid
               << " label hash: " << hash_new;
    auto& gauge = gauge_podsvc_family_ptr[metric]->Add(label_map);
    podsvc_gauge_map[metric][uuid] = make_pair(std::move(label_map), &gauge);
}

// Create EpCounter gauge given metric type and an uuid
bool PrometheusManager::createDynamicGaugeEp (EP_METRICS metric,
                                              const string& uuid,
                                              const string& ep_name,
                                              const size_t& attr_hash,
                    const unordered_map<string, string>&    attr_map)
{
    /**
     * We create a hash of all the key, value pairs in label attr_map
     * and then maintain a map of uuid to another pair of all attr hash
     * and gauge ptr
     * {uuid: pair(old_all_attr_hash, gauge_ptr)}
     */
    auto hgauge = getDynamicGaugeEp(metric, uuid);
    if (hgauge) {
        /**
         * Detect attribute change by comparing hashes:
         * Check incoming hash with the cached hash to detect attribute change
         * Note:
         * - we dont do a delete and create of metric for every attribute change.
         * Rather the dttribute's delete and create will get processed in EP Mgr.
         * Then during periodic update of epCounter, we will detect attr change in
         * PrometheusManager and do a delete/create of metric for latest label
         * annotations.
         * - by not doing del/add of metric for every attribute change, we reduce
         * # of metric+label creation in prometheus.
         */
        if (attr_hash == hgauge.get().first)
            return false;
        else {
            LOG(DEBUG) << "addNupdate epcounter: " << ep_name
                       << " incoming attr_hash: " << attr_hash << "\n"
                       << "existing ep metric, but deleting: hash modified;"
                       << " metric: " << ep_family_names[metric]
                       << " hash: " << hgauge.get().first
                       << " gaugeptr: " << hgauge.get().second;
            removeDynamicGaugeEp(metric, uuid);
        }
    }

    auto label_map = createLabelMapFromEpAttr(ep_name, attr_map);
    auto hash = hash_labels(label_map);
    LOG(DEBUG) << "creating ep dyn gauge family: " << ep_name
               << " label hash: " << hash;
    auto& gauge = gauge_ep_family_ptr[metric]->Add(label_map);
    ep_gauge_map[metric][uuid] = make_pair(hash, &gauge);

    return true;
}

// Create a label map that can be used for annotation, given the ep attr map
// and svc attr_map
const map<string,string> PrometheusManager::createLabelMapFromPodSvcAttr (
                          const unordered_map<string, string>&  ep_attr_map,
                          const unordered_map<string, string>&  svc_attr_map)
{
    map<string,string>   label_map;

    auto ep_name_itr = ep_attr_map.find("vm-name");
    auto svc_name_itr = svc_attr_map.find("name");
    // Ensuring both ep and svc's names are present in attributes
    // If not, there is no point in creating this metric
    if ((ep_name_itr != ep_attr_map.end())
            && (svc_name_itr != svc_attr_map.end())) {
        label_map["ep_name"] = ep_name_itr->second;
        label_map["svc_name"] = svc_name_itr->second;
    } else {
        return label_map;
    }

    auto ep_ns_itr = ep_attr_map.find("namespace");
    if (ep_ns_itr != ep_attr_map.end()) {
        label_map["ep_namespace"] = ep_ns_itr->second;
    }

    auto svc_ns_itr = svc_attr_map.find("namespace");
    if (svc_ns_itr != svc_attr_map.end()) {
        label_map["svc_namespace"] = svc_ns_itr->second;
    }

    return label_map;
}

// Max allowed annotations per metric
int PrometheusManager::max_metric_attr_count = 5;

// Create a label map that can be used for annotation, given the ep attr map
map<string,string> PrometheusManager::createLabelMapFromEpAttr (
                                                           const string& ep_name,
                                 const unordered_map<string, string>&    attr_map)
{
    map<string,string>   label_map;
    label_map["if_name"] = ep_name;
    int attr_count = 1; // Accounting for if_name

    auto ns_itr = attr_map.find("namespace");
    if (ns_itr != attr_map.end()) {
        label_map["namespace"] = ns_itr->second;
        attr_count++; // accounting for ns
    }

    auto pod_itr = attr_map.find("vm-name");
    if (pod_itr != attr_map.end()) {
        label_map["pod"] = pod_itr->second;
        attr_count++; // accounting for pod
    }

    for (const auto &p : attr_map) {
        if (attr_count == max_metric_attr_count) {
            LOG(DEBUG) << "Exceeding max attr count " << attr_count;
            break;
        }

        // empty values can be discarded
        if (p.second.empty())
            continue;

        bool metric_skip = false;
        for (auto &skip_str : metric_annotate_skip) {
            if (!p.first.compare(skip_str)) {
                metric_skip = true;
                break;
            }
        }
        if (metric_skip)
            continue;

        // Label values can be anything in prometheus
        if (checkMetricName(p.first)) {
            label_map[p.first] = p.second;
            /* Only prometheus compatible metrics are accounted against
             * attr_count. If user appends valid attributes to ep file that
             * exceeds the max_metric_attr_count, then only the first 5
             * attributes from the attr map will be used for metric
             * annotation */
            attr_count++;
        } else {
            LOG(ERROR) << "ep attr not compatible with prometheus"
                       << " K:" << p.first
                       << " V:" << p.second;
        }
    }

    return label_map;
}

// Get OFPeer stats gauge given the metric, peer (IP,port) tuple
Gauge * PrometheusManager::getDynamicGaugeOFPeer (OFPEER_METRICS metric,
                                                  const string& peer)
{
    Gauge *pgauge = nullptr;
    auto itr = ofpeer_gauge_map[metric].find(peer);
    if (itr == ofpeer_gauge_map[metric].end()) {
        LOG(DEBUG) << "Dyn Gauge OFPeer stats not found"
                   << " metric: " << metric
                   << " peer: " << peer;
    } else {
        pgauge = itr->second;
    }

    return pgauge;
}

// Get SGClassifierCounter gauge given the metric, classifier
Gauge * PrometheusManager::getDynamicGaugeSGClassifier (SGCLASSIFIER_METRICS metric,
                                                        const string& classifier)
{
    Gauge *pgauge = nullptr;
    auto itr = sgclassifier_gauge_map[metric].find(classifier);
    if (itr == sgclassifier_gauge_map[metric].end()) {
        LOG(DEBUG) << "Dyn Gauge SGClassifier stats not found"
                   << " metric: " << metric
                   << " classifier: " << classifier;
    } else {
        pgauge = itr->second;
    }

    return pgauge;
}

// Get RDDropCounter gauge given the metric, rdURI
Gauge * PrometheusManager::getDynamicGaugeRDDrop (RDDROP_METRICS metric,
                                                  const string& rdURI)
{
    Gauge *pgauge = nullptr;
    auto itr = rddrop_gauge_map[metric].find(rdURI);
    if (itr == rddrop_gauge_map[metric].end()) {
        LOG(DEBUG) << "Dyn Gauge RDDrop stats not found"
                   << " metric: " << metric
                   << " rdURI: " << rdURI;
    } else {
        pgauge = itr->second;
    }

    return pgauge;
}

// Get PodSvcCounter gauge given the metric, uuid of Pod+Svc
mgauge_pair_t PrometheusManager::getDynamicGaugePodSvc (PODSVC_METRICS metric,
                                                  const string& uuid)
{
    mgauge_pair_t mgauge = boost::none;
    auto itr = podsvc_gauge_map[metric].find(uuid);
    if (itr == podsvc_gauge_map[metric].end()) {
        LOG(DEBUG) << "Dyn Gauge PodSvcCounter not found"
                   << " metric: " << metric
                   << " uuid: " << uuid;
    } else {
        mgauge = itr->second;
    }

    return mgauge;
}

// Get EpCounter gauge given the metric, uuid of EP
hgauge_pair_t PrometheusManager::getDynamicGaugeEp (EP_METRICS metric,
                                                   const string& uuid)
{
    hgauge_pair_t hgauge = boost::none;
    auto itr = ep_gauge_map[metric].find(uuid);
    if (itr == ep_gauge_map[metric].end()) {
        LOG(DEBUG) << "Dyn Gauge EpCounter not found " << uuid;
    } else {
        hgauge = itr->second;
    }

    return hgauge;
}

// Remove dynamic SGClassifierCounter gauge given a metic type and classifier
bool PrometheusManager::removeDynamicGaugeSGClassifier (SGCLASSIFIER_METRICS metric,
                                                        const string& classifier)
{
    Gauge *pgauge = getDynamicGaugeSGClassifier(metric, classifier);
    if (pgauge) {
        sgclassifier_gauge_map[metric].erase(classifier);
        gauge_sgclassifier_family_ptr[metric]->Remove(pgauge);
    } else {
        LOG(DEBUG) << "remove dynamic gauge sgclassifier stats not found"
                   << " classifier:" << classifier;
        return false;
    }
    return true;
}

// Remove dynamic SGClassifierCounter gauge given a metric type
void PrometheusManager::removeDynamicGaugeSGClassifier (SGCLASSIFIER_METRICS metric)
{
    auto itr = sgclassifier_gauge_map[metric].begin();
    while (itr != sgclassifier_gauge_map[metric].end()) {
        LOG(DEBUG) << "Delete SGClassifierCounter"
                   << " classifier: " << itr->first
                   << " Gauge: " << itr->second;
        gauge_sgclassifier_family_ptr[metric]->Remove(itr->second);
        itr++;
    }

    sgclassifier_gauge_map[metric].clear();
}

// Remove dynamic SGClassifierCounter gauges for all metrics
void PrometheusManager::removeDynamicGaugeSGClassifier ()
{
    for (SGCLASSIFIER_METRICS metric=SGCLASSIFIER_METRICS_MIN;
            metric <= SGCLASSIFIER_METRICS_MAX;
                metric = SGCLASSIFIER_METRICS(metric+1)) {
        removeDynamicGaugeSGClassifier(metric);
    }
}

// Remove dynamic RDDropCounter gauge given a metic type and rdURI
bool PrometheusManager::removeDynamicGaugeRDDrop (RDDROP_METRICS metric,
                                                  const string& rdURI)
{
    Gauge *pgauge = getDynamicGaugeRDDrop(metric, rdURI);
    if (pgauge) {
        rddrop_gauge_map[metric].erase(rdURI);
        gauge_rddrop_family_ptr[metric]->Remove(pgauge);
    } else {
        LOG(DEBUG) << "remove dynamic gauge rddrop stats not found rdURI:" << rdURI;
        return false;
    }
    return true;
}

// Remove dynamic RDDropCounter gauge given a metric type
void PrometheusManager::removeDynamicGaugeRDDrop (RDDROP_METRICS metric)
{
    auto itr = rddrop_gauge_map[metric].begin();
    while (itr != rddrop_gauge_map[metric].end()) {
        LOG(DEBUG) << "Delete RDDropCounter rdURI: " << itr->first
                   << " Gauge: " << itr->second;
        gauge_rddrop_family_ptr[metric]->Remove(itr->second);
        itr++;
    }

    rddrop_gauge_map[metric].clear();
}

// Remove dynamic RDDropCounter gauges for all metrics
void PrometheusManager::removeDynamicGaugeRDDrop ()
{
    for (RDDROP_METRICS metric=RDDROP_METRICS_MIN;
            metric <= RDDROP_METRICS_MAX;
                metric = RDDROP_METRICS(metric+1)) {
        removeDynamicGaugeRDDrop(metric);
    }
}

// Remove dynamic OFPeerStats gauge given a metic type and peer (IP,port) tuple
// Note: The below api doesnt get called today. But keeping it in case we have
// a requirement to delete a gauge metric per peer, in case a leaf goes down
// or replaced. This can be used after changes to remove the corresponging
// observer mo.
bool PrometheusManager::removeDynamicGaugeOFPeer (OFPEER_METRICS metric,
                                                  const string& peer)
{
    Gauge *pgauge = getDynamicGaugeOFPeer(metric, peer);
    if (pgauge) {
        ofpeer_gauge_map[metric].erase(peer);
        gauge_ofpeer_family_ptr[metric]->Remove(pgauge);
    } else {
        LOG(DEBUG) << "remove dynamic gauge ofpeer stats not found peer:" << peer;
        return false;
    }
    return true;
}

// Remove dynamic OFPeerStats gauge given a metric type
void PrometheusManager::removeDynamicGaugeOFPeer (OFPEER_METRICS metric)
{
    auto itr = ofpeer_gauge_map[metric].begin();
    while (itr != ofpeer_gauge_map[metric].end()) {
        LOG(DEBUG) << "Delete OFPeer stats peer: " << itr->first
                   << " Gauge: " << itr->second;
        gauge_ofpeer_family_ptr[metric]->Remove(itr->second);
        itr++;
    }

    ofpeer_gauge_map[metric].clear();
}

// Remove dynamic OFPeerStats gauges for all metrics
void PrometheusManager::removeDynamicGaugeOFPeer ()
{
    for (OFPEER_METRICS metric=OFPEER_METRICS_MIN;
            metric <= OFPEER_METRICS_MAX;
                metric = OFPEER_METRICS(metric+1)) {
        removeDynamicGaugeOFPeer(metric);
    }
}

// Remove dynamic PodSvcCounter gauge given a metic type and podsvc uuid
bool PrometheusManager::removeDynamicGaugePodSvc (PODSVC_METRICS metric,
                                                  const string& uuid)
{
    auto mgauge = getDynamicGaugePodSvc(metric, uuid);
    if (mgauge) {
        auto &mpair = podsvc_gauge_map[metric][uuid];
        mpair.get().first.clear(); // free the label map
        podsvc_gauge_map[metric].erase(uuid);
        gauge_podsvc_family_ptr[metric]->Remove(mgauge.get().second);
    } else {
        LOG(DEBUG) << "remove dynamic gauge podsvc not found uuid:" << uuid;
        return false;
    }
    return true;
}

// Remove dynamic PodSvcCounter gauge given a metric type
void PrometheusManager::removeDynamicGaugePodSvc (PODSVC_METRICS metric)
{
    auto itr = podsvc_gauge_map[metric].begin();
    while (itr != podsvc_gauge_map[metric].end()) {
        LOG(DEBUG) << "Delete PodSvc uuid: " << itr->first
                   << " Gauge: " << itr->second.get().second;
        gauge_podsvc_family_ptr[metric]->Remove(itr->second.get().second);
        itr->second.get().first.clear(); // free the label map
        itr++;
    }

    podsvc_gauge_map[metric].clear();
}

// Remove dynamic PodSvcCounter gauges for all metrics
void PrometheusManager::removeDynamicGaugePodSvc ()
{
    for (PODSVC_METRICS metric=PODSVC_METRICS_MIN;
            metric <= PODSVC_METRICS_MAX;
                metric = PODSVC_METRICS(metric+1)) {
        removeDynamicGaugePodSvc(metric);
    }
}

// Remove dynamic EpCounter gauge given a metic type and ep uuid
bool PrometheusManager::removeDynamicGaugeEp (EP_METRICS metric,
                                              const string& uuid)
{
    auto hgauge = getDynamicGaugeEp(metric, uuid);
    if (hgauge) {
        ep_gauge_map[metric].erase(uuid);
        gauge_ep_family_ptr[metric]->Remove(hgauge.get().second);
    } else {
        LOG(DEBUG) << "remove dynamic gauge ep not found uuid:" << uuid;
        return false;
    }
    return true;
}

// Remove dynamic EpCounter gauge given a metic type
void PrometheusManager::removeDynamicGaugeEp (EP_METRICS metric)
{
    auto itr = ep_gauge_map[metric].begin();
    while (itr != ep_gauge_map[metric].end()) {
        LOG(DEBUG) << "Delete Ep uuid: " << itr->first
                   << " hash: " << itr->second.get().first
                   << " Gauge: " << itr->second.get().second;
        gauge_ep_family_ptr[metric]->Remove(itr->second.get().second);
        itr++;

        if (metric == (EP_METRICS_MAX-1)) {
            incStaticCounterEpRemove();
            updateStaticGaugeEpTotal(false);
        }
    }

    ep_gauge_map[metric].clear();
}

// Remove dynamic EpCounter gauges for all metrics
void PrometheusManager::removeDynamicGaugeEp ()
{
    for (EP_METRICS metric=EP_RX_BYTES;
            metric < EP_METRICS_MAX;
                metric = EP_METRICS(metric+1)) {
        removeDynamicGaugeEp(metric);
    }
}

// Remove all dynamically allocated counter families
void PrometheusManager::removeDynamicCounterFamilies ()
{
    // No dynamic counter families as of now
}

// Remove all dynamically allocated gauge families
void PrometheusManager::removeDynamicGaugeFamilies ()
{
    // No dynamic gauge families as of now
}

// Remove all statically  allocated ep counter families
void PrometheusManager::removeStaticCounterFamiliesEp ()
{
    counter_ep_create_family_ptr = nullptr;
    counter_ep_remove_family_ptr = nullptr;

}

// Remove all statically  allocated counter families
void PrometheusManager::removeStaticCounterFamilies ()
{
    // EpCounter specific
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        removeStaticCounterFamiliesEp();
    }
}

// Remove all statically allocated OFPeer gauge families
void PrometheusManager::removeStaticGaugeFamiliesOFPeer ()
{
    for (OFPEER_METRICS metric=OFPEER_METRICS_MIN;
            metric <= OFPEER_METRICS_MAX;
                metric = OFPEER_METRICS(metric+1)) {
        gauge_ofpeer_family_ptr[metric] = nullptr;
    }
}

// Remove all statically allocated SGClassifier gauge families
void PrometheusManager::removeStaticGaugeFamiliesSGClassifier ()
{
    for (SGCLASSIFIER_METRICS metric=SGCLASSIFIER_METRICS_MIN;
            metric <= SGCLASSIFIER_METRICS_MAX;
                metric = SGCLASSIFIER_METRICS(metric+1)) {
        gauge_sgclassifier_family_ptr[metric] = nullptr;
    }
}

// Remove all statically allocated RDDrop gauge families
void PrometheusManager::removeStaticGaugeFamiliesRDDrop ()
{
    for (RDDROP_METRICS metric=RDDROP_METRICS_MIN;
            metric <= RDDROP_METRICS_MAX;
                metric = RDDROP_METRICS(metric+1)) {
        gauge_rddrop_family_ptr[metric] = nullptr;
    }
}

// Remove all statically allocated podsvc gauge families
void PrometheusManager::removeStaticGaugeFamiliesPodSvc()
{
    for (PODSVC_METRICS metric=PODSVC_METRICS_MIN;
            metric <= PODSVC_METRICS_MAX;
                metric = PODSVC_METRICS(metric+1)) {
        gauge_podsvc_family_ptr[metric] = nullptr;
    }
}

// Remove all statically allocated ep gauge families
void PrometheusManager::removeStaticGaugeFamiliesEp()
{
    gauge_ep_total_family_ptr = nullptr;
    for (EP_METRICS metric=EP_RX_BYTES;
            metric < EP_METRICS_MAX;
                metric = EP_METRICS(metric+1)) {
        gauge_ep_family_ptr[metric] = nullptr;
    }
}

// Remove all statically allocated gauge families
void PrometheusManager::removeStaticGaugeFamilies()
{
    // EpCounter specific
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        removeStaticGaugeFamiliesEp();
    }

    // PodSvcCounter specific
    {
        const lock_guard<mutex> lock(podsvc_counter_mutex);
        removeStaticGaugeFamiliesPodSvc();
    }

    // OFPeer stats specific
    {
        const lock_guard<mutex> lock(ofpeer_stats_mutex);
        removeStaticGaugeFamiliesOFPeer();
    }

    // RDDropCounter specific
    {
        const lock_guard<mutex> lock(rddrop_stats_mutex);
        removeStaticGaugeFamiliesRDDrop();
    }

    // SGClassifierCounter specific
    {
        const lock_guard<mutex> lock(sgclassifier_stats_mutex);
        removeStaticGaugeFamiliesSGClassifier();
    }
}

// Return a rolling hash of attribute map for the ep
size_t PrometheusManager::calcHashEpAttributes (const string& ep_name,
                      const unordered_map<string, string>&    attr_map)
{
    auto label_map = createLabelMapFromEpAttr(ep_name, attr_map);
    auto hash = hash_labels(label_map);
    LOG(DEBUG) << ep_name << ":calculated label hash = " << hash;
    return hash;
}

/* Function called from IntFlowManager to update PodSvcCounter */
void PrometheusManager::addNUpdatePodSvcCounter (bool isEpToSvc,
                                                 const string& uuid,
                  const unordered_map<string, string>& ep_attr_map,
                  const unordered_map<string, string>& svc_attr_map)
{
    using namespace opflex::modb;
    using namespace modelgbp::gbpe;
    using namespace modelgbp::observer;

    const lock_guard<mutex> lock(podsvc_counter_mutex);
    Mutator mutator(framework, "policyelement");
    optional<shared_ptr<PolicyStatUniverse> > su =
                            PolicyStatUniverse::resolve(framework);
    if (!su)
        return;

    if (isEpToSvc) {
        optional<shared_ptr<EpToSvcCounter> > counter =
                        su.get()->resolveGbpeEpToSvcCounter(agent.getUuid(),
                                                            uuid);
        if (counter) {
            // Create the gauge counters if they arent present already
            for (PODSVC_METRICS metric=PODSVC_EP2SVC_MIN;
                    metric <= PODSVC_EP2SVC_MAX;
                        metric = PODSVC_METRICS(metric+1)) {
                createDynamicGaugePodSvc(metric,
                                         uuid,
                                         ep_attr_map,
                                         svc_attr_map);
            }

            // Update the metrics
            for (PODSVC_METRICS metric=PODSVC_EP2SVC_MIN;
                    metric <= PODSVC_EP2SVC_MAX;
                        metric = PODSVC_METRICS(metric+1)) {
                const mgauge_pair_t &mgauge = getDynamicGaugePodSvc(metric, uuid);
                optional<uint64_t>   metric_opt;
                switch (metric) {
                case PODSVC_EP2SVC_BYTES:
                    metric_opt = counter.get()->getBytes();
                    break;
                case PODSVC_EP2SVC_PKTS:
                    metric_opt = counter.get()->getPackets();
                    break;
                default:
                    LOG(ERROR) << "Unhandled eptosvc metric: " << metric;
                }
                if (metric_opt && mgauge)
                    mgauge.get().second->Set(static_cast<double>(metric_opt.get()));
            }
        } else {
            LOG(DEBUG) << "EpToSvcCounter yet to be created for uuid: " << uuid;
        }
    } else {
        optional<shared_ptr<SvcToEpCounter> > counter =
                        su.get()->resolveGbpeSvcToEpCounter(agent.getUuid(),
                                                            uuid);
        if (counter) {
            // Create the gauge counters if they arent present already
            for (PODSVC_METRICS metric=PODSVC_SVC2EP_MIN;
                    metric <= PODSVC_SVC2EP_MAX;
                        metric = PODSVC_METRICS(metric+1)) {
                createDynamicGaugePodSvc(metric,
                                         uuid,
                                         ep_attr_map,
                                         svc_attr_map);
            }

            // Update the metrics
            for (PODSVC_METRICS metric=PODSVC_SVC2EP_MIN;
                    metric <= PODSVC_SVC2EP_MAX;
                        metric = PODSVC_METRICS(metric+1)) {
                const mgauge_pair_t &mgauge = getDynamicGaugePodSvc(metric, uuid);
                optional<uint64_t>   metric_opt;
                switch (metric) {
                case PODSVC_SVC2EP_BYTES:
                    metric_opt = counter.get()->getBytes();
                    break;
                case PODSVC_SVC2EP_PKTS:
                    metric_opt = counter.get()->getPackets();
                    break;
                default:
                    LOG(ERROR) << "Unhandled svctoep metric: " << metric;
                }
                if (metric_opt && mgauge)
                    mgauge.get().second->Set(static_cast<double>(metric_opt.get()));
            }
        } else {
            LOG(DEBUG) << "SvcToEpCounter yet to be created for uuid: " << uuid;
        }
    }
}

/* Function called from SecGrpStatsManager to add/update SGClassifierCounter */
void PrometheusManager::addNUpdateSGClassifierCounter (const string& classifier)
{
    using namespace modelgbp::gbpe;
    using namespace modelgbp::observer;

    const lock_guard<mutex> lock(sgclassifier_stats_mutex);

    Mutator mutator(framework, "policyelement");
    optional<shared_ptr<PolicyStatUniverse> > su =
                    PolicyStatUniverse::resolve(agent.getFramework());
    if (su) {
        std::vector<OF_SHARED_PTR<modelgbp::gbpe::SecGrpClassifierCounter> > out;
        su.get()->resolveGbpeSecGrpClassifierCounter(out);
        for (const auto& counter : out) {
            if (!counter)
                continue;
            if (classifier.compare(counter.get()->getClassifier("")))
                continue;
            if (counter.get()->getGenId(0) != (sgclassifier_last_genId+1))
                continue;
            sgclassifier_last_genId++;

            for (SGCLASSIFIER_METRICS metric=SGCLASSIFIER_METRICS_MIN;
                    metric <= SGCLASSIFIER_METRICS_MAX;
                        metric = SGCLASSIFIER_METRICS(metric+1))
                if (!createDynamicGaugeSGClassifier(metric, classifier))
                    break;

            // Update the metrics
            for (SGCLASSIFIER_METRICS metric=SGCLASSIFIER_METRICS_MIN;
                    metric <= SGCLASSIFIER_METRICS_MAX;
                        metric = SGCLASSIFIER_METRICS(metric+1)) {
                Gauge *pgauge = getDynamicGaugeSGClassifier(metric,
                                                            classifier);
                optional<uint64_t>   metric_opt;
                switch (metric) {
                case SGCLASSIFIER_RX_BYTES:
                    metric_opt = counter.get()->getRxbytes();
                    break;
                case SGCLASSIFIER_RX_PACKETS:
                    metric_opt = counter.get()->getRxpackets();
                    break;
                case SGCLASSIFIER_TX_BYTES:
                    metric_opt = counter.get()->getTxbytes();
                    break;
                case SGCLASSIFIER_TX_PACKETS:
                    metric_opt = counter.get()->getTxpackets();
                    break;
                default:
                    LOG(ERROR) << "Unhandled sgclassifier metric: " << metric;
                }
                if (metric_opt && pgauge)
                    pgauge->Set(pgauge->Value() \
                                + static_cast<double>(metric_opt.get()));
            }
        }
        out.clear();
    }
}

/* Function called from ContractStatsManager to update RDDropCounter
 * This will be called from IntFlowManager to create metrics. */
void PrometheusManager::addNUpdateRDDropCounter (const string& rdURI,
                                                 bool isAdd)
{
    using namespace modelgbp::gbpe;
    using namespace modelgbp::observer;

    const lock_guard<mutex> lock(rddrop_stats_mutex);

    if (isAdd) {
        LOG(DEBUG) << "create RDDropCounter rdURI: " << rdURI;
        for (RDDROP_METRICS metric=RDDROP_METRICS_MIN;
                metric <= RDDROP_METRICS_MAX;
                    metric = RDDROP_METRICS(metric+1))
            createDynamicGaugeRDDrop(metric, rdURI);
        return;
    }

    Mutator mutator(framework, "policyelement");
    optional<shared_ptr<PolicyStatUniverse> > su =
                    PolicyStatUniverse::resolve(agent.getFramework());
    if (su) {
        std::vector<OF_SHARED_PTR<RoutingDomainDropCounter> > out;
        su.get()->resolveGbpeRoutingDomainDropCounter(out);
        for (const auto& counter : out) {
            if (!counter)
                continue;
            if (rdURI.compare(counter.get()->getRoutingDomain("")))
                continue;
            if (counter.get()->getGenId(0) != (rddrop_last_genId+1))
                continue;
            rddrop_last_genId++;

            // Update the metrics
            for (RDDROP_METRICS metric=RDDROP_METRICS_MIN;
                    metric <= RDDROP_METRICS_MAX;
                        metric = RDDROP_METRICS(metric+1)) {
                Gauge *pgauge = getDynamicGaugeRDDrop(metric, rdURI);
                optional<uint64_t>   metric_opt;
                switch (metric) {
                case RDDROP_BYTES:
                    metric_opt = counter.get()->getBytes();
                    break;
                case RDDROP_PACKETS:
                    metric_opt = counter.get()->getPackets();
                    break;
                default:
                    LOG(ERROR) << "Unhandled rddrop metric: " << metric;
                }
                if (metric_opt && pgauge)
                    pgauge->Set(pgauge->Value() \
                                + static_cast<double>(metric_opt.get()));
            }
        }
        out.clear();
    }
}

/* Function called from PolicyStatsManager to update OFPeerStats */
void PrometheusManager::addNUpdateOFPeerStats (void)
{
    using namespace modelgbp::observer;

    const lock_guard<mutex> lock(ofpeer_stats_mutex);
    Mutator mutator(framework, "policyelement");
    optional<shared_ptr<SysStatUniverse> > su =
                            SysStatUniverse::resolve(framework);
    std::unordered_map<string, OF_SHARED_PTR<OFStats>> stats;
    agent.getFramework().getOpflexPeerStats(stats);
    if (su) {
        for (const auto& peerStat : stats) {
            optional<shared_ptr<OpflexCounter>> counter =
                       su.get()->resolveObserverOpflexCounter(peerStat.first);
            if (!counter)
                continue;

            // Create gauge metrics if they arent present already
            for (OFPEER_METRICS metric=OFPEER_METRICS_MIN;
                    metric <= OFPEER_METRICS_MAX;
                        metric = OFPEER_METRICS(metric+1))
                createDynamicGaugeOFPeer(metric, peerStat.first);

            // Update the metrics
            for (OFPEER_METRICS metric=OFPEER_METRICS_MIN;
                    metric <= OFPEER_METRICS_MAX;
                        metric = OFPEER_METRICS(metric+1)) {
                Gauge *pgauge = getDynamicGaugeOFPeer(metric, peerStat.first);
                optional<uint64_t>   metric_opt;
                switch (metric) {
                case OFPEER_IDENT_REQS:
                    metric_opt = counter.get()->getIdentReqs();
                    break;
                case OFPEER_IDENT_RESPS:
                    metric_opt = counter.get()->getIdentResps();
                    break;
                case OFPEER_IDENT_ERRORS:
                    metric_opt = counter.get()->getIdentErrs();
                    break;
                case OFPEER_POL_RESOLVES:
                    metric_opt = counter.get()->getPolResolves();
                    break;
                case OFPEER_POL_RESOLVE_RESPS:
                    metric_opt = counter.get()->getPolResolveResps();
                    break;
                case OFPEER_POL_RESOLVE_ERRS:
                    metric_opt = counter.get()->getPolResolveErrs();
                    break;
                case OFPEER_POL_UNRESOLVES:
                    metric_opt = counter.get()->getPolUnresolves();
                    break;
                case OFPEER_POL_UNRESOLVE_RESPS:
                    metric_opt = counter.get()->getPolUnresolveResps();
                    break;
                case OFPEER_POL_UNRESOLVE_ERRS:
                    metric_opt = counter.get()->getPolUnresolveErrs();
                    break;
                case OFPEER_POL_UPDATES:
                    metric_opt = counter.get()->getPolUpdates();
                    break;
                case OFPEER_EP_DECLARES:
                    metric_opt = counter.get()->getEpDeclares();
                    break;
                case OFPEER_EP_DECLARE_RESPS:
                    metric_opt = counter.get()->getEpDeclareResps();
                    break;
                case OFPEER_EP_DECLARE_ERRS:
                    metric_opt = counter.get()->getEpDeclareErrs();
                    break;
                case OFPEER_EP_UNDECLARES:
                    metric_opt = counter.get()->getEpUndeclares();
                    break;
                case OFPEER_EP_UNDECLARE_RESPS:
                    metric_opt = counter.get()->getEpUndeclareResps();
                    break;
                case OFPEER_EP_UNDECLARE_ERRS:
                    metric_opt = counter.get()->getEpUndeclareErrs();
                    break;
                case OFPEER_STATE_REPORTS:
                    metric_opt = counter.get()->getStateReports();
                    break;
                case OFPEER_STATE_REPORT_RESPS:
                    metric_opt = counter.get()->getStateReportResps();
                    break;
                case OFPEER_STATE_REPORT_ERRS:
                    metric_opt = counter.get()->getStateReportErrs();
                    break;
                default:
                    LOG(ERROR) << "Unhandled ofpeer metric: " << metric;
                }
                if (metric_opt && pgauge)
                    pgauge->Set(static_cast<double>(metric_opt.get()));
            }
        }
    }
}

/* Function called from EP Manager to update EpCounter */
void PrometheusManager::addNUpdateEpCounter (const string& uuid,
                                             const string& ep_name,
                                             const size_t& attr_hash,
                  const unordered_map<string, string>&    attr_map)
{
    using namespace opflex::modb;
    using namespace modelgbp::gbpe;
    using namespace modelgbp::observer;

    const lock_guard<mutex> lock(ep_counter_mutex);
    Mutator mutator(framework, "policyelement");
    optional<shared_ptr<EpStatUniverse> > su =
                            EpStatUniverse::resolve(framework);
    if (su) {
        optional<shared_ptr<EpCounter>> ep_counter =
                            su.get()->resolveGbpeEpCounter(uuid);
        if (ep_counter) {

            // Create the gauge counters if they arent present already
            for (EP_METRICS metric=EP_RX_BYTES;
                    metric < EP_METRICS_MAX;
                        metric = EP_METRICS(metric+1)) {
                if (!createDynamicGaugeEp(metric,
                                          uuid,
                                          ep_name,
                                          attr_hash,
                                          attr_map)) {
                    break;
                }

                if (metric == (EP_METRICS_MAX-1)) {
                    incStaticCounterEpCreate();
                    updateStaticGaugeEpTotal(true);
                }
            }

            // Update the metrics
            for (EP_METRICS metric=EP_RX_BYTES;
                    metric < EP_METRICS_MAX;
                        metric = EP_METRICS(metric+1)) {
                hgauge_pair_t hgauge = getDynamicGaugeEp(metric, uuid);
                optional<uint64_t>   metric_opt;
                switch (metric) {
                case EP_RX_BYTES:
                    metric_opt = ep_counter.get()->getRxBytes();
                    break;
                case EP_RX_PKTS:
                    metric_opt = ep_counter.get()->getRxPackets();
                    break;
                case EP_RX_DROPS:
                    metric_opt = ep_counter.get()->getRxDrop();
                    break;
                case EP_RX_UCAST:
                    metric_opt = ep_counter.get()->getRxUnicast();
                    break;
                case EP_RX_MCAST:
                    metric_opt = ep_counter.get()->getRxMulticast();
                    break;
                case EP_RX_BCAST:
                    metric_opt = ep_counter.get()->getRxBroadcast();
                    break;
                case EP_TX_BYTES:
                    metric_opt = ep_counter.get()->getTxBytes();
                    break;
                case EP_TX_PKTS:
                    metric_opt = ep_counter.get()->getTxPackets();
                    break;
                case EP_TX_DROPS:
                    metric_opt = ep_counter.get()->getTxDrop();
                    break;
                case EP_TX_UCAST:
                    metric_opt = ep_counter.get()->getTxUnicast();
                    break;
                case EP_TX_MCAST:
                    metric_opt = ep_counter.get()->getTxMulticast();
                    break;
                case EP_TX_BCAST:
                    metric_opt = ep_counter.get()->getTxBroadcast();
                    break;
                default:
                    LOG(ERROR) << "Unhandled metric: " << metric;
                }
                if (metric_opt && hgauge)
                    hgauge.get().second->Set(static_cast<double>(metric_opt.get()));
            }
        }
    }
}

// Function called from IntFlowManager to remove PodSvcCounter
void PrometheusManager::removePodSvcCounter (bool isEpToSvc,
                                             const string& uuid)
{
    const lock_guard<mutex> lock(podsvc_counter_mutex);
    LOG(DEBUG) << "remove podsvc counter"
               << " isEpToSvc: " << isEpToSvc
               << " uuid: " << uuid;

    if (isEpToSvc) {
        for (PODSVC_METRICS metric=PODSVC_EP2SVC_MIN;
                metric <= PODSVC_EP2SVC_MAX;
                    metric = PODSVC_METRICS(metric+1)) {
            if (!removeDynamicGaugePodSvc(metric, uuid))
                break;
        }
    } else {
        for (PODSVC_METRICS metric=PODSVC_SVC2EP_MIN;
                metric <= PODSVC_SVC2EP_MAX;
                    metric = PODSVC_METRICS(metric+1)) {
            if (!removeDynamicGaugePodSvc(metric, uuid))
                break;
        }
    }
}

// Function called from EP Manager to remove EpCounter
void PrometheusManager::removeEpCounter (const string& uuid,
                                         const string& ep_name)
{
    const lock_guard<mutex> lock(ep_counter_mutex);
    LOG(DEBUG) << "remove ep counter " << ep_name;

    for (EP_METRICS metric=EP_RX_BYTES;
            metric < EP_METRICS_MAX;
                metric = EP_METRICS(metric+1)) {
        if (!removeDynamicGaugeEp(metric, uuid))
            break;

        if (metric == (EP_METRICS_MAX-1)) {
            incStaticCounterEpRemove();
            updateStaticGaugeEpTotal(false);
        }
    }
}

// Function called from SecGrpStatsManager to remove SGClassifierCounter
void PrometheusManager::removeSGClassifierCounter (const string& classifier)
{
    const lock_guard<mutex> lock(sgclassifier_stats_mutex);
    LOG(DEBUG) << "remove SGClassifierCounter"
               << " classifier: " << classifier;

    for (SGCLASSIFIER_METRICS metric=SGCLASSIFIER_METRICS_MIN;
            metric <= SGCLASSIFIER_METRICS_MAX;
                metric = SGCLASSIFIER_METRICS(metric+1)) {
        if (!removeDynamicGaugeSGClassifier(metric, classifier))
            break;
    }
}

// Function called from IntFlowManager to remove RDDropCounter
void PrometheusManager::removeRDDropCounter (const string& rdURI)
{
    const lock_guard<mutex> lock(rddrop_stats_mutex);
    LOG(DEBUG) << "remove RDDropCounter rdURI: " << rdURI;

    for (RDDROP_METRICS metric=RDDROP_METRICS_MIN;
            metric <= RDDROP_METRICS_MAX;
                metric = RDDROP_METRICS(metric+1)) {
        if (!removeDynamicGaugeRDDrop(metric, rdURI))
            break;
    }
}

} /* namespace opflexagent */
