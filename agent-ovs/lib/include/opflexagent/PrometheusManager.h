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
    void addNUpdateEpCounter(const std::string& ep_name,
                             const std::string& uuid);

    /**
     * Remove EpCounter metric given the ep name
     */
    void removeEpCounter(const std::string& epName);

    // TODO: Other Counter related APIs

private:
    opflex::ofcore::OFFramework& framework;

    // The http server handle
    std::unique_ptr<Exposer>     exposer_ptr_;

    /**
     * Registry which keeps track of all the metric
     * families to scrape.
     */
    std::shared_ptr<Registry>    registry_ptr_;

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
    std::string sanitizeLabelName(std::string label_name);

    /* EpCounter related apis and state */

    // Lock to safe guard EpCounter related state
    std::mutex ep_counter_mutex_;

    // Static Metric families

    // Counter family to track all EpCounter creates
    Family<Counter>    *counter_ep_create_family_ptr_;

    // Counter family to track all EpCounter removes
    Family<Counter>    *counter_ep_remove_family_ptr_;

    // Gauge family to track the  total # of EpCounters
    Family<Gauge>      *gauge_ep_total_family_ptr_;

    // Counter to track ep creates
    Counter            *counter_ep_create_ptr_;

    // Counter to track ep removes
    Counter            *counter_ep_remove_ptr_;

    // Gauge to track total Eps
    Gauge              *gauge_ep_total_ptr_;

    // Actual ep total count
    double              gauge_ep_total_;

    // func to increment EpCounter
    void incStaticCounterEpCounter(void);

    // func to decrement EpCounter
    void decStaticCounterEpCounter(void);

    // func to set total EpCounter
    void updateStaticGaugeEpCounter(bool add);

    // Dynamic Metric families

    // func to create gauge family of EpCounter for ep_name
    Family<Gauge>* createDynGaugeFamilyEpCounter(const std::string& ep_name);

    // func to get gauge family of EpCounter given ep_name
    Family<Gauge>* getDynGaugeFamilyEpCounter(const std::string& ep_name);

    // func to remove gauge family of EpCounter given ep_name
    void removeDynGaugeFamilyEpCounter(const std::string& epName);

    // func to remove all gauge families of every EpCounter
    void removeAllDynGaugeFamilyEpCounter(void);

    // cache the gauge metric for every ep_name
    std::unordered_map<std::string, Family<Gauge> *>
                                 ep_counter_gauge_family_map_;

    /* TODO: Other Counter related apis and state */
};

} /* namespace opflexagent */

#endif // __OPFLEXAGENT_PROMETHEUS_MANAGER_H__
