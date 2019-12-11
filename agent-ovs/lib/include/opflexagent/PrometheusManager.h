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

#include <config.h>
#include <stdexcept>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <opflex/modb/URIBuilder.h>
#include <opflex/ofcore/OFFramework.h>
#include <opflexagent/ExtraConfigManager.h>
#include <opflexagent/logging.h>
#include <chrono>
#include <map>
#include <unordered_map>
#include <memory>
#include <string>
#include <thread>
#include <mutex>

#include <prometheus/gauge.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#pragma once
#ifndef __OPFLEXAGENT_PROMETHEUS_MANAGER_H__
#define __OPFLEXAGENT_PROMETHEUS_MANAGER_H__

namespace opflexagent {

using boost::optional;
using std::string;
using std::make_pair;
using std::runtime_error;
using opflex::modb::URI;
using namespace prometheus;

class PrometheusManager {
public:
    /**
     * Instantiate a new prometheus manager
     */
    PrometheusManager(opflex::ofcore::OFFramework &fwk_);
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
     * Create EpCounter metric family if its not present.
     * Update EpCounter metric family if its already present
     *
     * @param ep_name   the name of the ep
     * @param uuid      uuid of ep
     */
    void addNUpdateEpCounter(const std::string& uuid,
                             const std::string& ep_name,
        const std::unordered_map<std::string, std::string>&    attr_map);
    /**
     * Remove EpCounter metric given the ep name
     */
    void removeEpCounter(const std::string& uuid,
                         const std::string& ep_name);

    // TODO: Other Counter related APIs

private:
    // opflex framwork handle
    opflex::ofcore::OFFramework& framework;
    // The http server handle
    std::unique_ptr<Exposer>     exposer_ptr;
    /**
     * Registry which keeps track of all the metric
     * families to scrape.
     */
    std::shared_ptr<Registry>    registry_ptr;
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
    std::string sanitizeMetricName(std::string metric_name);
    // Api to check if the input metric_name is prometheus compatible.
    bool checkMetricName(const std::string& metric_name);

    /* Start of EpCounter related apis and state */
    // Lock to safe guard EpCounter related state
    std::mutex ep_counter_mutex;

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
                              const std::string& uuid,
                              const std::string& ep_name,
        const std::unordered_map<std::string, std::string>&    attr_map);
    // func to get gauge for EpCounter given metric type, uuid
    Gauge* getDynamicGaugeEp(EP_METRICS metric, const std::string& uuid);
    // func to remove gauge for EpCounter given metric type, uuid
    bool removeDynamicGaugeEp(EP_METRICS metric, const std::string& uuid);
    // func to remove all gauge of every EpCounter for a metric type
    void removeDynamicGaugeEp(EP_METRICS metric);
    // func to remove all gauges of every EpCounter
    void removeDynamicGaugeEp(void);

    // cache the gauge metric for every ep uuid
    std::unordered_map<std::string, Gauge *>    ep_gauge_map[EP_METRICS_MAX];
    /* End of EpCounter related apis and state */

    /* TODO: Other Counter related apis and state */
};

} /* namespace opflexagent */

#endif // __OPFLEXAGENT_PROMETHEUS_MANAGER_H__
