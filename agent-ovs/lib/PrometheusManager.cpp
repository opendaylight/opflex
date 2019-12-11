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
#include <opflex/modb/Mutator.h>
#include <opflex/modb/Mutator.h>
#include <opflex/modb/PropertyInfo.h>
#include <opflexagent/ExtraConfigManager.h>
#include <opflexagent/logging.h>
#include <opflexagent/PrometheusManager.h>
#include <opflexagent/EndpointManager.h>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <boost/optional.hpp>
#include <regex>

#include <prometheus/gauge.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

namespace opflexagent {

using boost::optional;
using std::string;
using std::make_pair;
using std::runtime_error;
using opflex::modb::URI;
using namespace prometheus;

// construct PrometheusManager
PrometheusManager::PrometheusManager(opflex::ofcore::OFFramework &fwk_) :
                                     framework(fwk_),
                                     gauge_ep_total_{0}  {}

// create all counter families during start
void PrometheusManager::createStaticCounterFamilies (void)
{
    // add a new counter family to the registry (families combine values with the
    // same name, but distinct label dimensions)
    // Note: There is a unique ptr allocated and referencing the below reference
    // during Register().

    /* Counter family to track the total calls made to EpCounter update/remove
     * from other clients */
    auto& counter_ep_create_family = BuildCounter()
                         .Name("EpCreateCount")
                         .Help("Total number of EpCounter creates")
                         .Labels({{"props", "all"}})
                         .Register(*registry_ptr_);
    counter_ep_create_family_ptr_ = &counter_ep_create_family;

    auto& counter_ep_remove_family = BuildCounter()
                         .Name("EpRemoveCount")
                         .Help("Total number of EpCounter deletes")
                         .Labels({{"props", "all"}})
                         .Register(*registry_ptr_);
    counter_ep_remove_family_ptr_ = &counter_ep_remove_family;
}

// create all static counters during start
void PrometheusManager::createStaticCounters ()
{
    auto& counter_ep_create = counter_ep_create_family_ptr_->Add({
                                    {"props", "all"}
                                    });
    counter_ep_create_ptr_ = &counter_ep_create;

    auto& counter_ep_remove = counter_ep_remove_family_ptr_->Add({
                                    {"props", "all"}
                                    });
    counter_ep_remove_ptr_ = &counter_ep_remove;
}

// remove all static counters during stop
void PrometheusManager::removeStaticCounters ()
{
    counter_ep_create_family_ptr_->Remove(counter_ep_create_ptr_);
    counter_ep_create_ptr_ = nullptr;

    counter_ep_remove_family_ptr_->Remove(counter_ep_remove_ptr_);
    counter_ep_remove_ptr_ = nullptr;
}

// create all gauge families during start
void PrometheusManager::createStaticGaugeFamilies (void)
{
    // add a new gauge family to the registry (families combine values with the
    // same name, but distinct label dimensions)
    // Note: There is a unique ptr allocated and referencing the below reference
    // during Register().
    auto& gauge_ep_total_family = BuildGauge()
                         .Name("EpTotalCount")
                         .Help("Total EpCounter active")
                         .Labels({{"props", "all"}})
                         .Register(*registry_ptr_);
    gauge_ep_total_family_ptr_ = &gauge_ep_total_family;
}

// create gauges during start
void PrometheusManager::createStaticGauges ()
{
    auto& gauge_ep_total = gauge_ep_total_family_ptr_->Add({
                                    {"props", "all"}
                                    });
    gauge_ep_total_ptr_ = &gauge_ep_total;
}

// remove gauges during stop
void PrometheusManager::removeStaticGauges ()
{
    gauge_ep_total_family_ptr_->Remove(gauge_ep_total_ptr_);
    gauge_ep_total_ptr_ = nullptr;
    gauge_ep_total_ = 0;
}

// Start of PrometheusManager instance
void PrometheusManager::start()
{
    const std::lock_guard<std::mutex> lock(ep_counter_mutex_);
    LOG(DEBUG) << "starting prometheus manager";
    /**
     * create an http server running on port 8080
     * Note: The third argument is the total worker thread count. Prometheus
     * follows boss-worker thread model. 1 boss thread will get created to
     * intercept HTTP requests. The requests will then be serviced by free
     * worker threads. We are using 1 worker thread to service the requests.
     */
    exposer_ptr_ = std::unique_ptr<Exposer>(new Exposer{"127.0.0.1:8080", "/metrics", 1});
    registry_ptr_ = std::make_shared<Registry>();

    /* Initialize Metric families which can be created during
     * init time */
    createStaticCounterFamilies();
    createStaticGaugeFamilies();

    // Add static metrics
    createStaticCounters();
    createStaticGauges();

    // ask the exposer to scrape the registry on incoming scrapes
    exposer_ptr_->RegisterCollectable(registry_ptr_);
}

// Stop of PrometheusManager instance
void PrometheusManager::stop()
{
    const std::lock_guard<std::mutex> lock(ep_counter_mutex_);
    LOG(DEBUG) << "stopping prometheus manager";

    // Gracefully delete state

    // Remove all of the dynamic families
    removeAllDynGaugeFamilyEpCounter();

    // Remove metrics
    removeStaticCounters();
    removeStaticGauges();

    // Remove metric families
    removeStaticCounterFamilies();
    removeStaticGaugeFamilies();

    exposer_ptr_.reset();
    exposer_ptr_ = nullptr;

    registry_ptr_.reset();
    registry_ptr_ = nullptr;
}

// Increment Ep count
void PrometheusManager::incStaticCounterEpCounter ()
{
    counter_ep_create_ptr_->Increment();
}

// decrement ep count
void PrometheusManager::decStaticCounterEpCounter ()
{
    counter_ep_remove_ptr_->Increment();
}

// track total ep count
void PrometheusManager::updateStaticGaugeEpCounter (bool add)
{
    if (add)
        gauge_ep_total_ptr_->Set(++gauge_ep_total_);
    else
        gauge_ep_total_ptr_->Set(--gauge_ep_total_);
}

// sanitize metric family name for prometheus to accept
std::string PrometheusManager::sanitizeLabelName (std::string label_name)
{
    char replace_from = '-', replace_to = '_';
    size_t found = label_name.find_first_of(replace_from);

    // Prometheus doesnt like anything other than [a-zA-Z_:][a-zA-Z0-9_:]*
    // https://prometheus.io/docs/concepts/data_model/
    // static const std::regex metric_name_regex("[a-zA-Z_:][a-zA-Z0-9_:]*");
    // assert(CheckMetricName(label_name));
    while (found != string::npos) {
        label_name[found] = replace_to;
        found = label_name.find_first_of(replace_from, found+1);
    }

    // TODO: Since this is O(n), we can optimize this. Do this once
    // and cache {"UUID","name"} map.
    return label_name;
}

// Create EpCounter gauge family given an ep_name
Family<Gauge>* PrometheusManager::createDynGaugeFamilyEpCounter (const std::string& ep_name)
{
    LOG(DEBUG) << "creating dyn gauge family " << ep_name;
    Family<Gauge>* gauge_ptr = nullptr;
    auto itr = ep_counter_gauge_family_map_.find(ep_name);
    if (itr == ep_counter_gauge_family_map_.end()) {
        auto& gauge_family = BuildGauge()
                             .Name("EpCounter_"+ ep_name)
                             .Help("Ep Counter Gauge Metric Family")
                             .Labels({{"props", "all"}})
                             .Register(*registry_ptr_);
        gauge_ptr = &gauge_family;
        ep_counter_gauge_family_map_[ep_name] = gauge_ptr;
        incStaticCounterEpCounter();
        updateStaticGaugeEpCounter(true);
    } else {
        LOG(ERROR) << "Dyn Gauge Family EpCounter already found " << ep_name;
        gauge_ptr = itr->second;
    }
    return gauge_ptr;
}

// Get EpCounter gauge family given an ep_name
Family<Gauge>* PrometheusManager::getDynGaugeFamilyEpCounter (const std::string& ep_name)
{
    LOG(DEBUG) << "get dyn gauge family " << ep_name;
    Family<Gauge>* gauge_ptr = nullptr;
    auto itr = ep_counter_gauge_family_map_.find(ep_name);
    if (itr == ep_counter_gauge_family_map_.end()) {
        LOG(DEBUG) << "Dyn Gauge Family EpCounter not found " << ep_name;
    } else {
        gauge_ptr = itr->second;
    }
    return gauge_ptr;
}

// Remove all EpCounter gauge families
void PrometheusManager::removeAllDynGaugeFamilyEpCounter ()
{
    LOG(DEBUG) << "remove all dyn gauge family ";
    auto itr = ep_counter_gauge_family_map_.begin();
    while (itr != ep_counter_gauge_family_map_.end()) {
        LOG(DEBUG) << "Delete EpCounter: " << itr->first
                   << " Family: " << itr->second;
        decStaticCounterEpCounter();
        updateStaticGaugeEpCounter(false);
        itr++;
    }

    ep_counter_gauge_family_map_.clear();
    return;
}

// Remove EpCounter's gauge family given ep_name
void PrometheusManager::removeDynGaugeFamilyEpCounter (const std::string& ep_name)
{
    LOG(DEBUG) << "remove dyn gauge family " << ep_name;
    auto itr = ep_counter_gauge_family_map_.find(ep_name);
    if (itr == ep_counter_gauge_family_map_.end()) {
        LOG(ERROR) << "Delete of unknown EpCounter " << ep_name;
    } else {
        ep_counter_gauge_family_map_.erase(itr);
        decStaticCounterEpCounter();
        updateStaticGaugeEpCounter(false);
    }

    return;
}

// Remove all statically  allocated counter families
void PrometheusManager::removeStaticCounterFamilies ()
{
    // EpCounter specific
    counter_ep_create_family_ptr_ = nullptr;
    counter_ep_remove_family_ptr_ = nullptr;

    return;
}

// Remove all statically allocated gauge families
void PrometheusManager::removeStaticGaugeFamilies()
{
    // EpCounter specific
    gauge_ep_total_family_ptr_ = nullptr;

    return;
}

/* Function called from EP Manager to update EpCounter */
void PrometheusManager::addNUpdateEpCounter (const std::string& ep_name_,
                                             const std::string& uuid)
{
    using namespace opflex::modb;
    using namespace modelgbp::gbpe;
    using namespace modelgbp::observer;

    const std::lock_guard<std::mutex> lock(ep_counter_mutex_);
    auto ep_name = sanitizeLabelName(ep_name_);
    LOG(DEBUG) << "addnupdate ep counter" << ep_name;
    Mutator mutator(framework, "policyelement");
    optional<std::shared_ptr<EpStatUniverse> > su =
                            EpStatUniverse::resolve(framework);
    if (su) {
        boost::optional<std::shared_ptr<EpCounter>> ep_counter =
                            su.get()->resolveGbpeEpCounter(uuid);
        if (ep_counter) {

            // Retrieve or create the family first
            auto gauge_family_ptr = getDynGaugeFamilyEpCounter(ep_name);
            if (!gauge_family_ptr)
                gauge_family_ptr = createDynGaugeFamilyEpCounter(ep_name);

            /* Note: Calling gauge_family->Add() wont create new gauge.
             * If a gauge is already there for a label, it will be retrieved
             * and returned for use */
            auto& rx_packets_gauge = gauge_family_ptr->Add({{"props", "RxPackets"}});
            auto rx_packets_opt = ep_counter.get()->getRxPackets();
            if (rx_packets_opt) {
                auto rx_packets = static_cast<double>(rx_packets_opt.get());
                rx_packets_gauge.Set(rx_packets);
            }

            auto& tx_packets_gauge = gauge_family_ptr->Add({{"props", "TxPackets"}});
            auto tx_packets_opt = ep_counter.get()->getTxPackets();
            if (tx_packets_opt) {
                auto tx_packets = static_cast<double>(tx_packets_opt.get());
                tx_packets_gauge.Set(tx_packets);
            }

            auto& rx_bytes_gauge = gauge_family_ptr->Add({{"props", "RxBytes"}});
            auto rx_bytes_opt = ep_counter.get()->getRxBytes();
            if (rx_bytes_opt) {
                auto rx_bytes = static_cast<double>(rx_bytes_opt.get());
                rx_bytes_gauge.Set(rx_bytes);
            }

            auto& tx_bytes_gauge = gauge_family_ptr->Add({{"props", "TxBytes"}});
            auto tx_bytes_opt = ep_counter.get()->getTxBytes();
            if (tx_bytes_opt) {
                auto tx_bytes = static_cast<double>(tx_bytes_opt.get());
                tx_bytes_gauge.Set(tx_bytes);
            }
        }
    }

    return;
}

// Function called from EP Manager to remove EpCounter
void PrometheusManager::removeEpCounter (const std::string& ep_name_)
{
    const std::lock_guard<std::mutex> lock(ep_counter_mutex_);
    auto ep_name = sanitizeLabelName(ep_name_);
    LOG(DEBUG) << "remove ep counter " << ep_name;
    auto gauge_family_ptr = getDynGaugeFamilyEpCounter(ep_name);
    if (!gauge_family_ptr) {
        LOG(ERROR) << "Dyn Gauge Family ptr not found for ep_name: " << ep_name;
        return;
    }

    /* Note:
     * - Calling gauge_family->Add() wont create new gauge.
     * If a gauge is already there for a label, it will be retrieved
     * and returned for use
     * - there is no retrieve api in prometheus lib, unless we cache these
     */
    auto& rx_packets_gauge = gauge_family_ptr->Add({{"props", "RxPackets"}});
    gauge_family_ptr->Remove(&rx_packets_gauge);
    auto& tx_packets_gauge = gauge_family_ptr->Add({{"props", "TxPackets"}});
    gauge_family_ptr->Remove(&tx_packets_gauge);
    auto& rx_bytes_gauge = gauge_family_ptr->Add({{"props", "RxBytes"}});
    gauge_family_ptr->Remove(&rx_bytes_gauge);
    auto& tx_bytes_gauge = gauge_family_ptr->Add({{"props", "TxBytes"}});
    gauge_family_ptr->Remove(&tx_bytes_gauge);

    // Remove the family
    removeDynGaugeFamilyEpCounter(ep_name);

    return;
}

// Utility to dump contents of any unordered map
template<typename K, typename V>
static void print_map(std::unordered_map<K,V> const& m) {
    for (auto const &pair: m) {
        LOG(DEBUG) << "{" << pair.first << ":" << pair.second << "}\n";
    }
}

} /* namespace opflexagent */
