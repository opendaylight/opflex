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
#include <opflexagent/PrometheusManager.h>
#include <opflexagent/EndpointManager.h>
#include <map>
#include <boost/optional.hpp>
#include <regex>

#include <prometheus/gauge.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

namespace opflexagent {

using boost::optional;
using std::lock_guard;
using std::regex;
using std::regex_match;
using std::map;
using std::make_shared;

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

// construct PrometheusManager
PrometheusManager::PrometheusManager(opflex::ofcore::OFFramework &fwk_) :
                                     framework(fwk_),
                                     gauge_ep_total{0}  {}

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
    createStaticCounterFamiliesEp();
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
    createStaticCountersEp();
}

// remove all dynamic counters during stop
void PrometheusManager::removeDynamicCounters ()
{
    // No dynamic counters as of now
}

// remove all dynamic counters during stop
void PrometheusManager::removeDynamicGauges ()
{
    removeDynamicGaugeEp();
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
    removeStaticCountersEp();
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


// create all gauge families during start
void PrometheusManager::createStaticGaugeFamilies (void)
{
    createStaticGaugeFamiliesEp();
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
    createStaticGaugesEp();
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
    removeStaticGaugesEp();
}

// Start of PrometheusManager instance
void PrometheusManager::start()
{
    const lock_guard<mutex> lock(ep_counter_mutex);
    LOG(DEBUG) << "starting prometheus manager";
    /**
     * create an http server running on port 8080
     * Note: The third argument is the total worker thread count. Prometheus
     * follows boss-worker thread model. 1 boss thread will get created to
     * intercept HTTP requests. The requests will then be serviced by free
     * worker threads. We are using 1 worker thread to service the requests.
     */
    exposer_ptr = unique_ptr<Exposer>(new Exposer{"127.0.0.1:8080", "/metrics", 1});
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
    const lock_guard<mutex> lock(ep_counter_mutex);
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

// Create EpCounter gauge given metric type and an uuid
bool PrometheusManager::createDynamicGaugeEp (EP_METRICS metric,
                                              const string& uuid,
                                              const string& ep_name_,
            const unordered_map<string, string>&    attr_map)
{
    LOG(DEBUG) << "creating dyn gauge family " << ep_name_;

    // TODO: Currently we just have a map of {UUID: gauge_ptr} per metric.
    // We ideally have to create a hash of all the key, value pairs in
    // label attr_map and then maintain a map of uuid to another pair of
    // all attr hash and gauge ptr
    // {uuid: pair(old_all_attr_hash, gauge_ptr)}
    auto gauge_ptr = getDynamicGaugeEp(metric, uuid);
    if (gauge_ptr) {
        LOG(DEBUG) << "EP already present for metric " << ep_family_names[metric];
        return false;
    }

    map<string,string>   label_map;
    auto ep_name = checkMetricName(ep_name_)?ep_name_:sanitizeMetricName(ep_name_);
    label_map["if_name"] = ep_name;
    LOG(DEBUG) << "attr_map size is " << attr_map.size();
    for (const auto &p : attr_map) {
        // empty values can be discarded
        if (p.second.empty())
            continue;

        if (!p.first.compare("pod-template-hash"))
            continue;

        if (checkMetricName(p.first) && checkMetricName(p.second))
            label_map[p.first] = p.second;
        else {
            LOG(ERROR) << "ep attr not compatible with prometheus"
                       << " K:" << p.first
                       << " V:" << p.second;
        }
    }

    auto& gauge = gauge_ep_family_ptr[metric]->Add(label_map);
    ep_gauge_map[metric][uuid] = &gauge;

    return true;
}

// Get EpCounter gauge given the metric, uuid of EP
Gauge* PrometheusManager::getDynamicGaugeEp (EP_METRICS metric,
                                             const string& uuid)
{
    Gauge* gauge_ptr = nullptr;
    auto itr = ep_gauge_map[metric].find(uuid);
    if (itr == ep_gauge_map[metric].end()) {
        LOG(DEBUG) << "Dyn Gauge EpCounter not found " << uuid;
    } else {
        gauge_ptr = itr->second;
    }

    return gauge_ptr;
}

// Remove dynamic EpCounter gauge given a metic type and ep uuid
bool PrometheusManager::removeDynamicGaugeEp (EP_METRICS metric,
                                              const string& uuid)
{
    auto gauge_ptr = getDynamicGaugeEp(metric, uuid);
    if (gauge_ptr) {
        ep_gauge_map[metric].erase(uuid);
        gauge_ep_family_ptr[metric]->Remove(gauge_ptr);
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
                   << " Gauge: " << itr->second;
        //ep_gauge_map[metric].erase(itr->first);
        gauge_ep_family_ptr[metric]->Remove(itr->second);
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
    removeStaticCounterFamiliesEp();
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
    removeStaticGaugeFamiliesEp();
}

/* Function called from EP Manager to update EpCounter */
void PrometheusManager::addNUpdateEpCounter (const string& uuid,
                                             const string& ep_name,
            const unordered_map<string, string>&    attr_map)
{
    using namespace opflex::modb;
    using namespace modelgbp::gbpe;
    using namespace modelgbp::observer;

    const lock_guard<mutex> lock(ep_counter_mutex);
    LOG(DEBUG) << "addNupdate ep counter" << ep_name;
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
                Gauge* gauge_ptr = getDynamicGaugeEp(metric, uuid);
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
                if (metric_opt)
                    gauge_ptr->Set(static_cast<double>(metric_opt.get()));
            }
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

// Utility to dump contents of any unordered map
template<typename K, typename V>
static void print_map(unordered_map<K,V> const& m) {
    for (auto const &pair: m) {
        LOG(DEBUG) << "{" << pair.first << ":" << pair.second << "}\n";
    }
}

} /* namespace opflexagent */
