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
#include <boost/algorithm/string.hpp>
#include <regex>

#include <prometheus/gauge.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/detail/utils.h>

namespace opflexagent {

using std::vector;
using std::size_t;
using std::to_string;
using std::lock_guard;
using std::regex;
using std::regex_match;
using std::regex_replace;
using std::make_shared;
using std::make_pair;
using namespace prometheus::detail;
using boost::split;

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

static string svc_family_names[] =
{
  "opflex_svc_rx_bytes",
  "opflex_svc_rx_packets",
  "opflex_svc_tx_bytes",
  "opflex_svc_tx_packets"
};

static string svc_family_help[] =
{
  "service ingress/rx bytes",
  "service ingress/rx packets",
  "service egress/tx bytes",
  "service egress/tx packets"
};

static string svc_target_family_names[] =
{
  "opflex_svc_target_rx_bytes",
  "opflex_svc_target_rx_packets",
  "opflex_svc_target_tx_bytes",
  "opflex_svc_target_tx_packets"
};

static string svc_target_family_help[] =
{
  "cluster service target ingress/rx bytes",
  "cluster service target ingress/rx packets",
  "cluster service target egress/tx bytes",
  "cluster service target egress/tx packets"
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

static string remote_ep_family_names[] =
{
  "opflex_remote_endpoint_count"
};

static string remote_ep_family_help[] =
{
  "number of remote endpoints under the same uplink port"
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
  "opflex_sg_tx_bytes",
  "opflex_sg_tx_packets",
  "opflex_sg_rx_bytes",
  "opflex_sg_rx_packets"
};

static string sgclassifier_family_help[] =
{
  "security-group classifier tx bytes",
  "security-group classifier tx packets",
  "security-group classifier rx bytes",
  "security-group classifier rx packets"
};

static string contract_family_names[] =
{
  "opflex_contract_bytes",
  "opflex_contract_packets"
};

static string contract_family_help[] =
{
  "contract classifier bytes",
  "contract classifier packets",
};

static string table_drop_family_names[] =
{
  "opflex_table_drop_bytes",
  "opflex_table_drop_packets"
};

static string table_drop_family_help[] =
{
  "opflex table drop bytes",
  "opflex table drop packets"
};

#define RETURN_IF_DISABLED  if (disabled) {return;}

// construct PrometheusManager
PrometheusManager::PrometheusManager(Agent &agent_,
                                     opflex::ofcore::OFFramework &fwk_) :
                                     agent(agent_),
                                     framework(fwk_),
                                     gauge_ep_total{0},
                                     gauge_svc_total{0},
                                     rddrop_last_genId{0},
                                     sgclassifier_last_genId{0},
                                     contract_last_genId{0},
                                     disabled{true},
                                     exposeEpSvcNan{false}
{
    //Init state to avoid coverty warnings
    init();
}

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
                         .Name("opflex_endpoint_created_total")
                         .Help("Total number of local endpoint creates")
                         .Labels({})
                         .Register(*registry_ptr);
    counter_ep_create_family_ptr = &counter_ep_create_family;

    auto& counter_ep_remove_family = BuildCounter()
                         .Name("opflex_endpoint_removed_total")
                         .Help("Total number of local endpoint deletes")
                         .Labels({})
                         .Register(*registry_ptr);
    counter_ep_remove_family_ptr = &counter_ep_remove_family;
}

// create all svc counter families during start
void PrometheusManager::createStaticCounterFamiliesSvc (void)
{
    // add a new counter family to the registry (families combine values with the
    // same name, but distinct label dimensions)
    // Note: There is a unique ptr allocated and referencing the below reference
    // during Register().

    /* Counter family to track the total calls made to SvcCounter create/remove
     * from other clients */
    auto& counter_svc_create_family = BuildCounter()
                         .Name("opflex_svc_created_total")
                         .Help("Total number of SVC creates")
                         .Labels({})
                         .Register(*registry_ptr);
    counter_svc_create_family_ptr = &counter_svc_create_family;

    auto& counter_svc_remove_family = BuildCounter()
                         .Name("opflex_svc_removed_total")
                         .Help("Total number of SVC deletes")
                         .Labels({})
                         .Register(*registry_ptr);
    counter_svc_remove_family_ptr = &counter_svc_remove_family;
}

// create all counter families during start
void PrometheusManager::createStaticCounterFamilies (void)
{
    // EpCounter families
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        createStaticCounterFamiliesEp();
    }

    // SvcCounter families
    {
        const lock_guard<mutex> lock(svc_counter_mutex);
        createStaticCounterFamiliesSvc();
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

// create all static svc counters during start
void PrometheusManager::createStaticCountersSvc ()
{
    auto& counter_svc_create = counter_svc_create_family_ptr->Add({});
    counter_svc_create_ptr = &counter_svc_create;

    auto& counter_svc_remove = counter_svc_remove_family_ptr->Add({});
    counter_svc_remove_ptr = &counter_svc_remove;
}

// create all static counters during start
void PrometheusManager::createStaticCounters ()
{
    // EpCounter related metrics
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        createStaticCountersEp();
    }

    // SvcCounter related metrics
    {
        const lock_guard<mutex> lock(svc_counter_mutex);
        createStaticCountersSvc();
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

    // Remove SvcTargetCounter related gauges
    {
        const lock_guard<mutex> lock(svc_target_counter_mutex);
        removeDynamicGaugeSvcTarget();
    }

    // Remove SvcCounter related gauges
    {
        const lock_guard<mutex> lock(svc_counter_mutex);
        removeDynamicGaugeSvc();
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

    // Remove RemoteEp related gauges
    {
        const lock_guard<mutex> lock(remote_ep_mutex);
        removeDynamicGaugeRemoteEp();
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

    // Remove ContractClassifierCounter related gauges
    {
        const lock_guard<mutex> lock(contract_stats_mutex);
        removeDynamicGaugeContractClassifier();
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

// remove all static svc counters during stop
void PrometheusManager::removeStaticCountersSvc ()
{
    counter_svc_create_family_ptr->Remove(counter_svc_create_ptr);
    counter_svc_create_ptr = nullptr;

    counter_svc_remove_family_ptr->Remove(counter_svc_remove_ptr);
    counter_svc_remove_ptr = nullptr;
}

// remove all static counters during stop
void PrometheusManager::removeStaticCounters ()
{

    // Remove EpCounter related counter metrics
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        removeStaticCountersEp();
    }

    // Remove SvcCounter related counter metrics
    {
        const lock_guard<mutex> lock(svc_counter_mutex);
        removeStaticCountersSvc();
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

// create all ContractClassifier specific gauge families during start
void PrometheusManager::createStaticGaugeFamiliesContractClassifier (void)
{
    // add a new gauge family to the registry (families combine values with the
    // same name, but distinct label dimensions)
    // Note: There is a unique ptr allocated and referencing the below reference
    // during Register().

    for (CONTRACT_METRICS metric=CONTRACT_METRICS_MIN;
            metric <= CONTRACT_METRICS_MAX;
                metric = CONTRACT_METRICS(metric+1)) {
        auto& gauge_contract_family = BuildGauge()
                             .Name(contract_family_names[metric])
                             .Help(contract_family_help[metric])
                             .Labels({})
                             .Register(*registry_ptr);
        gauge_contract_family_ptr[metric] = &gauge_contract_family;
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

// create all RemoteEp specific gauge families during start
void PrometheusManager::createStaticGaugeFamiliesRemoteEp (void)
{
    // add a new gauge family to the registry (families combine values with the
    // same name, but distinct label dimensions)
    // Note: There is a unique ptr allocated and referencing the below reference
    // during Register().

    for (REMOTE_EP_METRICS metric=REMOTE_EP_METRICS_MIN;
            metric <= REMOTE_EP_METRICS_MAX;
                metric = REMOTE_EP_METRICS(metric+1)) {
        auto& gauge_remote_ep_family = BuildGauge()
                             .Name(remote_ep_family_names[metric])
                             .Help(remote_ep_family_help[metric])
                             .Labels({})
                             .Register(*registry_ptr);
        gauge_remote_ep_family_ptr[metric] = &gauge_remote_ep_family;

        // metrics per family will be created later
        remote_ep_gauge_map[metric] = nullptr;
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
                         .Name("opflex_endpoint_active_total")
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

// create all SvcTarget specific gauge families during start
void PrometheusManager::createStaticGaugeFamiliesSvcTarget (void)
{
    // add a new gauge family to the registry (families combine values with the
    // same name, but distinct label dimensions)
    // Note: There is a unique ptr allocated and referencing the below reference
    // during Register().

    for (SVC_TARGET_METRICS metric=SVC_TARGET_METRICS_MIN;
            metric <= SVC_TARGET_METRICS_MAX;
                metric = SVC_TARGET_METRICS(metric+1)) {
        auto& gauge_svc_target_family = BuildGauge()
                             .Name(svc_target_family_names[metric])
                             .Help(svc_target_family_help[metric])
                             .Labels({})
                             .Register(*registry_ptr);
        gauge_svc_target_family_ptr[metric] = &gauge_svc_target_family;
    }
}

// create all SVC specific gauge families during start
void PrometheusManager::createStaticGaugeFamiliesSvc (void)
{
    // add a new gauge family to the registry (families combine values with the
    // same name, but distinct label dimensions)
    // Note: There is a unique ptr allocated and referencing the below reference
    // during Register().

    auto& gauge_svc_total_family = BuildGauge()
                         .Name("opflex_svc_active_total")
                         .Help("Total active service count")
                         .Labels({})
                         .Register(*registry_ptr);
    gauge_svc_total_family_ptr = &gauge_svc_total_family;

    for (SVC_METRICS metric=SVC_METRICS_MIN;
            metric <= SVC_METRICS_MAX;
                metric = SVC_METRICS(metric+1)) {
        auto& gauge_svc_family = BuildGauge()
                             .Name(svc_family_names[metric])
                             .Help(svc_family_help[metric])
                             .Labels({})
                             .Register(*registry_ptr);
        gauge_svc_family_ptr[metric] = &gauge_svc_family;
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

void PrometheusManager::createStaticGaugeFamiliesTableDrop(void) {
    // add a new gauge family to the registry (families combine values with the
    // same name, but distinct label dimensions)
    // Note: There is a unique ptr allocated and referencing the below reference
    // during Register().
    const lock_guard<mutex> lock(table_drop_counter_mutex);

    for (TABLE_DROP_METRICS metric=TABLE_DROP_METRICS_MIN;
            metric <= TABLE_DROP_METRICS_MAX;
                metric = TABLE_DROP_METRICS(metric+1)) {
        auto& gauge_table_drop_family = BuildGauge()
                             .Name(table_drop_family_names[metric])
                             .Help(table_drop_family_help[metric])
                             .Labels({})
                             .Register(*registry_ptr);
        gauge_table_drop_family_ptr[metric] = &gauge_table_drop_family;
    }
}

// remove all static counters during stop
void PrometheusManager::removeStaticGaugesTableDrop ()
{
    // Remove Table Drop related counter metrics
    const lock_guard<mutex> lock(table_drop_counter_mutex);

    for (TABLE_DROP_METRICS metric=TABLE_DROP_METRICS_MIN;
                         metric <= TABLE_DROP_METRICS_MAX;
                     metric = TABLE_DROP_METRICS(metric+1)) {
        for (auto itr = table_drop_gauge_map[metric].begin();
            itr != table_drop_gauge_map[metric].end(); itr++) {
            LOG(DEBUG) << "Delete TableDrop " << itr->first
                   << " Gauge: " << itr->second.get().second;
            gauge_table_drop_family_ptr[metric]->Remove(
                    itr->second.get().second);
        }
        table_drop_gauge_map[metric].clear();
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
        const lock_guard<mutex> lock(svc_counter_mutex);
        createStaticGaugeFamiliesSvc();
    }

    {
        const lock_guard<mutex> lock(svc_target_counter_mutex);
        createStaticGaugeFamiliesSvcTarget();
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
        const lock_guard<mutex> lock(remote_ep_mutex);
        createStaticGaugeFamiliesRemoteEp();
    }

    {
        const lock_guard<mutex> lock(rddrop_stats_mutex);
        createStaticGaugeFamiliesRDDrop();
    }

    {
        const lock_guard<mutex> lock(sgclassifier_stats_mutex);
        createStaticGaugeFamiliesSGClassifier();
    }

    createStaticGaugeFamiliesTableDrop();

    {
        const lock_guard<mutex> lock(contract_stats_mutex);
        createStaticGaugeFamiliesContractClassifier();
    }
}

// create EpCounter gauges during start
void PrometheusManager::createStaticGaugesEp ()
{
    auto& gauge_ep_total = gauge_ep_total_family_ptr->Add({});
    gauge_ep_total_ptr = &gauge_ep_total;
}

// create SvcCounter gauges during start
void PrometheusManager::createStaticGaugesSvc ()
{
    auto& gauge_svc_total = gauge_svc_total_family_ptr->Add({});
    gauge_svc_total_ptr = &gauge_svc_total;
}

// create gauges during start
void PrometheusManager::createStaticGauges ()
{
    // EpCounter related gauges
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        createStaticGaugesEp();
    }

    // SvcCounter related gauges
    {
        const lock_guard<mutex> lock(svc_counter_mutex);
        createStaticGaugesSvc();
    }
}

// remove ep gauges during stop
void PrometheusManager::removeStaticGaugesEp ()
{
    gauge_ep_total_family_ptr->Remove(gauge_ep_total_ptr);
    gauge_ep_total_ptr = nullptr;
    gauge_ep_total = 0;
}

// remove svc gauges during stop
void PrometheusManager::removeStaticGaugesSvc ()
{
    gauge_svc_total_family_ptr->Remove(gauge_svc_total_ptr);
    gauge_svc_total_ptr = nullptr;
    gauge_svc_total = 0;
}

// remove gauges during stop
void PrometheusManager::removeStaticGauges ()
{

    // Remove EpCounter related gauge metrics
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        removeStaticGaugesEp();
    }
    // Remove SvcCounter related gauge metrics
    {
        const lock_guard<mutex> lock(svc_counter_mutex);
        removeStaticGaugesSvc();
    }
    // Remove TableDropCounter related gauges
    removeStaticGaugesTableDrop();

}

template <class T>
bool PrometheusManager::MetricDupChecker<T>::is_dup (T *metric)
{
    const lock_guard<mutex> lock(dup_mutex);
    if (metrics.count(metric)) {
        LOG(ERROR) << "Duplicate metric detected: " << metric;
        return true;
    }
    return false;
}

template <class T>
void PrometheusManager::MetricDupChecker<T>::add (T *metric)
{
    const lock_guard<mutex> lock(dup_mutex);
    metrics.insert(metric);
}

template <class T>
void PrometheusManager::MetricDupChecker<T>::remove (T *metric)
{
    const lock_guard<mutex> lock(dup_mutex);
    metrics.erase(metric);
}

template <class T>
void PrometheusManager::MetricDupChecker<T>::clear (void)
{
    const lock_guard<mutex> lock(dup_mutex);
    metrics.clear();
}

// Start of PrometheusManager instance
void PrometheusManager::start (bool exposeLocalHostOnly, bool exposeEpSvcNan_)
{
    disabled = false;
    exposeEpSvcNan = exposeEpSvcNan_;
    LOG(DEBUG) << "starting prometheus manager,"
               << " exposeLHOnly: " << exposeLocalHostOnly
               << " exposeEpSvcNan: " << exposeEpSvcNan;
    /**
     * create an http server running on port 9612
     * Note: The third argument is the total worker thread count. Prometheus
     * follows boss-worker thread model. 1 boss thread will get created to
     * intercept HTTP requests. The requests will then be serviced by free
     * worker threads. We are using 1 worker thread to service the requests.
     * Note: Port #9612 has been reserved for opflex here:
     * https://github.com/prometheus/prometheus/wiki/Default-port-allocations
     */
    if (exposeLocalHostOnly)
        exposer_ptr = unique_ptr<Exposer>(new Exposer{"127.0.0.1:9612", "/metrics", 1});
    else
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

    string allowed;
    for (const auto& allow : agent.getPrometheusEpAttributes())
        allowed += allow+",";
    LOG(DEBUG) << "Agent config's allowed ep attributes: " << allowed;
}

// initialize state of PrometheusManager instance
void PrometheusManager::init ()
{
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        counter_ep_create_ptr = nullptr;
        counter_ep_remove_ptr = nullptr;
        gauge_ep_total_ptr = nullptr;
        counter_ep_create_family_ptr = nullptr;
        counter_ep_remove_family_ptr = nullptr;
        gauge_ep_total_family_ptr = nullptr;
        for (EP_METRICS metric=EP_RX_BYTES;
                metric < EP_METRICS_MAX;
                    metric = EP_METRICS(metric+1)) {
            gauge_ep_family_ptr[metric] = nullptr;
        }
    }

    {
        const lock_guard<mutex> lock(svc_target_counter_mutex);
        for (SVC_TARGET_METRICS metric=SVC_TARGET_METRICS_MIN;
                metric <= SVC_TARGET_METRICS_MAX;
                    metric = SVC_TARGET_METRICS(metric+1)) {
            gauge_svc_target_family_ptr[metric] = nullptr;
        }
    }

    {
        const lock_guard<mutex> lock(svc_counter_mutex);
        counter_svc_create_ptr = nullptr;
        counter_svc_remove_ptr = nullptr;
        gauge_svc_total_ptr = nullptr;
        counter_svc_create_family_ptr = nullptr;
        counter_svc_remove_family_ptr = nullptr;
        gauge_svc_total_family_ptr = nullptr;
        for (SVC_METRICS metric=SVC_METRICS_MIN;
                metric <= SVC_METRICS_MAX;
                    metric = SVC_METRICS(metric+1)) {
            gauge_svc_family_ptr[metric] = nullptr;
        }
    }

    {
        const lock_guard<mutex> lock(podsvc_counter_mutex);
        for (PODSVC_METRICS metric=PODSVC_METRICS_MIN;
                metric <= PODSVC_METRICS_MAX;
                    metric = PODSVC_METRICS(metric+1)) {
            gauge_podsvc_family_ptr[metric] = nullptr;
        }
    }

    {
        const lock_guard<mutex> lock(ofpeer_stats_mutex);
        for (OFPEER_METRICS metric=OFPEER_METRICS_MIN;
                metric <= OFPEER_METRICS_MAX;
                    metric = OFPEER_METRICS(metric+1)) {
            gauge_ofpeer_family_ptr[metric] = nullptr;
        }
    }

    {
        const lock_guard<mutex> lock(remote_ep_mutex);
        for (REMOTE_EP_METRICS metric=REMOTE_EP_METRICS_MIN;
                metric <= REMOTE_EP_METRICS_MAX;
                    metric = REMOTE_EP_METRICS(metric+1)) {
            gauge_remote_ep_family_ptr[metric] = nullptr;
            remote_ep_gauge_map[metric] = nullptr;
        }
    }

    {
        const lock_guard<mutex> lock(rddrop_stats_mutex);
        for (RDDROP_METRICS metric=RDDROP_METRICS_MIN;
                metric <= RDDROP_METRICS_MAX;
                    metric = RDDROP_METRICS(metric+1)) {
            gauge_rddrop_family_ptr[metric] = nullptr;
        }
    }

    {
        const lock_guard<mutex> lock(table_drop_counter_mutex);
        for (TABLE_DROP_METRICS metric = TABLE_DROP_BYTES;
                metric <= TABLE_DROP_METRICS_MAX;
                    metric = TABLE_DROP_METRICS(metric+1)) {
            gauge_table_drop_family_ptr[metric] = nullptr;
        }
    }

    {
        const lock_guard<mutex> lock(sgclassifier_stats_mutex);
        for (SGCLASSIFIER_METRICS metric=SGCLASSIFIER_METRICS_MIN;
                metric <= SGCLASSIFIER_METRICS_MAX;
                    metric = SGCLASSIFIER_METRICS(metric+1)) {
            gauge_sgclassifier_family_ptr[metric] = nullptr;
        }
    }

    {
        const lock_guard<mutex> lock(contract_stats_mutex);
        for (CONTRACT_METRICS metric=CONTRACT_METRICS_MIN;
                metric <= CONTRACT_METRICS_MAX;
                    metric = CONTRACT_METRICS(metric+1)) {
            gauge_contract_family_ptr[metric] = nullptr;
        }
    }
}

// Stop of PrometheusManager instance
void PrometheusManager::stop ()
{
    RETURN_IF_DISABLED
    disabled = true;
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

    gauge_check.clear();
    counter_check.clear();

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

// Increment Svc count
void PrometheusManager::incStaticCounterSvcCreate ()
{
    counter_svc_create_ptr->Increment();
}

// decrement svc count
void PrometheusManager::incStaticCounterSvcRemove ()
{
    counter_svc_remove_ptr->Increment();
}

// track total svc count
void PrometheusManager::updateStaticGaugeSvcTotal (bool add)
{
    if (add)
        gauge_svc_total_ptr->Set(++gauge_svc_total);
    else
        gauge_svc_total_ptr->Set(--gauge_svc_total);
}

// Check if a given metric name is Prometheus compatible
bool PrometheusManager::checkMetricName (const string& metric_name)
{
    // Prometheus doesnt like anything other than:
    // [a-zA-Z_:][a-zA-Z0-9_:]* for metric family name - these are static as on date
    // [a-zA-Z_][a-zA-Z0-9_]* for label name
    // https://prometheus.io/docs/concepts/data_model/
    static const regex metric_name_regex("[a-zA-Z_][a-zA-Z0-9_]*");
    return regex_match(metric_name, metric_name_regex);
}

// sanitize metric family name for prometheus to accept
string PrometheusManager::sanitizeMetricName (const string& metric_name)
{
    // Prometheus doesnt like anything other than:
    // [a-zA-Z_:][a-zA-Z0-9_:]* for metric family name - these are static as on date
    // [a-zA-Z_][a-zA-Z0-9_]* for label name
    // https://prometheus.io/docs/concepts/data_model/

    // Note: the below negation regex doesnt work correctly if there are numbers
    // first char. This is mainly because multiple []'s and '*' doesnt work with
    // regex_replace
    //static const regex metric_name_regex("[^a-zA-Z_][^a-zA-Z0-9_]*");
    static const regex regex1("[^a-zA-Z_]");
    static const regex regex2("[^0-9a-zA-Z_]");
    auto label1 = regex_replace(metric_name.substr(0,1), regex1, "_");
    string label2;
    if (metric_name.size() > 1)
        label2 = regex_replace(metric_name.substr(1), regex2, "_");
    auto label = label1+label2;

    // Label names beginning with __ are reserved for internal use in
    // prometheus.
    auto reserved_for_internal_purposes = label.compare(0, 2, "__") == 0;
    if (reserved_for_internal_purposes)
        label[0] = 'a';

    return label;
}

// Create OFPeerStats gauge given metric type, peer (IP,port) tuple
void PrometheusManager::createDynamicGaugeOFPeer (OFPEER_METRICS metric,
                                                  const string& peer)
{
    // Retrieve the Gauge if its already created
    if (getDynamicGaugeOFPeer(metric, peer))
        return;

    auto& gauge = gauge_ofpeer_family_ptr[metric]->Add({{"peer", peer}});
    if (gauge_check.is_dup(&gauge)) {
        LOG(ERROR) << "duplicate ofpeer dyn gauge family"
                   << " metric: " << metric
                   << " peer: " << peer;
        return;
    }
    LOG(DEBUG) << "created ofpeer dyn gauge family"
               << " metric: " << metric
               << " peer: " << peer;
    gauge_check.add(&gauge);
    ofpeer_gauge_map[metric][peer] = &gauge;
}

// Create ContractClassifierCounter gauge given metric type,
// name of srcEpg, dstEpg & classifier
bool PrometheusManager::createDynamicGaugeContractClassifier (CONTRACT_METRICS metric,
                                                              const string& srcEpg,
                                                              const string& dstEpg,
                                                              const string& classifier)
{
    // Retrieve the Gauge if its already created
    if (getDynamicGaugeContractClassifier(metric,
                                          srcEpg,
                                          dstEpg,
                                          classifier))
        return false;


    auto& gauge = gauge_contract_family_ptr[metric]->Add(
                    {
                        {"src_epg", constructEpgLabel(srcEpg)},
                        {"dst_epg", constructEpgLabel(dstEpg)},
                        {"classifier", constructClassifierLabel(classifier,
                                                                false)}
                    });
    if (gauge_check.is_dup(&gauge)) {
        LOG(ERROR) << "duplicate contract dyn gauge family"
                   << " metric: " << metric
                   << " srcEpg: " << srcEpg
                   << " dstEpg: " << dstEpg
                   << " classifier: " << classifier;
        return false;
    }
    LOG(DEBUG) << "created contract dyn gauge family"
               << " metric: " << metric
               << " srcEpg: " << srcEpg
               << " dstEpg: " << dstEpg
               << " classifier: " << classifier;
    gauge_check.add(&gauge);
    const string& key = srcEpg+dstEpg+classifier;
    contract_gauge_map[metric][key] = &gauge;
    return true;
}

// Create SGClassifierCounter gauge given metric type, classifier
bool PrometheusManager::createDynamicGaugeSGClassifier (SGCLASSIFIER_METRICS metric,
                                                        const string& classifier)
{
    // Retrieve the Gauge if its already created
    if (getDynamicGaugeSGClassifier(metric, classifier))
        return false;

    auto& gauge = gauge_sgclassifier_family_ptr[metric]->Add(
                    {
                        {"classifier", constructClassifierLabel(classifier,
                                                                true)}
                    });
    if (gauge_check.is_dup(&gauge)) {
        LOG(DEBUG) << "duplicate sgclassifier dyn gauge family"
                   << " metric: " << metric
                   << " classifier: " << classifier;
        return false;
    }
    LOG(DEBUG) << "created sgclassifier dyn gauge family"
               << " metric: " << metric
               << " classifier: " << classifier;
    gauge_check.add(&gauge);
    sgclassifier_gauge_map[metric][classifier] = &gauge;
    return true;
}

// Construct label, given EPG URI
string PrometheusManager::constructEpgLabel (const string& epg)
{
    /* Example EPG URI:
     * /PolicyUniverse/PolicySpace/kube/GbpEpGroup/kubernetes%7ckube-system/
     * We want to produce these annotations for every metric:
     * label_map: {{src_epg: "tenant:kube,policy:kube-system"},
     */
    size_t tLow = epg.find("PolicySpace") + 12;
    size_t gEpGStart = epg.rfind("GbpEpGroup");
    string tenant = epg.substr(tLow,gEpGStart-tLow-1);
    size_t nHigh = epg.size()-2;
    size_t nLow = gEpGStart+11;
    string ename = epg.substr(nLow,nHigh-nLow+1);
    string name = "tenant:" + tenant;
    name += ",policy:" + ename;
    return name;
}

// Construct label, given classifier URI
string PrometheusManager::constructClassifierLabel (const string& classifier,
                                                    bool isSecGrp)
{
    /* Example classifier URI:
     * /PolicyUniverse/PolicySpace/kube/GbpeL24Classifier/SGkube_np_static-discovery%7cdiscovery%7carp-ingress/
     * We want to produce these annotations for every metric:
     * label_map: {{classifier: "tenant:kube,policy:kube_np_static-discovery,subj:discovery,rule:arp-ingress,
     *                           [string version of classifier]}}
     */
    size_t tLow = classifier.find("PolicySpace") + 12;
    size_t gL24Start = classifier.rfind("GbpeL24Classifier");
    string tenant = classifier.substr(tLow,gL24Start-tLow-1);
    size_t nHigh = classifier.size()-2;
    size_t nLow = gL24Start+18;
    string cname = classifier.substr(nLow,nHigh-nLow+1);
    string name = "tenant:" + tenant;
    // When the agent works along with aci fabric, the name of SG will be:
    // SGkube_np_static-discovery%7cdiscovery%7carp-ingress
    // In case of GBP server, the SG name will be like below:
    // kube_np_static-discovery
    // Remove the SG if its a prefix and if we have '|' separators.
    vector<string> results;
    // Note: splitting with '%' since "%7c" is treated as 3 chars. The assumption
    // is that GBP server policies dont have classifier name with '%'.
    split(results, cname, [](char c){return c == '%';});
    // 2 '|' will lead to 3 strings: policy, subj, and rule
    if (results.size() == 3) {
        if (isSecGrp)
            name += ",policy:" + results[0].substr(2); // Post "SG"
        else
            name += ",policy:" + results[0];
        name += ",subj:" + results[1].substr(2); // Post 7c
        name += ",rule:" + results[2].substr(2); // Post 7c

        // Note: MO resolves dont work with encoded ascii "%7c"
        // hence modifying the lookup string with actual ascii character '|'
        cname = results[0] + "|"
                    + results[1].substr(2) + "|"
                    + results[2].substr(2);
    } else {
        name += ",policy:" + cname;
    }

    return name+",["+stringizeClassifier(tenant, cname)+"]";
}

// Get a compressed human readable form of classifier
string PrometheusManager::stringizeClassifier (const string& tenant,
                                               const string& classifier)
{
    using namespace modelgbp::gbpe;
    string compressed;
    const auto& counter = L24Classifier::resolve(agent.getFramework(),
                                                 tenant,
                                                 classifier);
    if (counter) {
        auto arp_opc = counter.get()->getArpOpc();
        if (arp_opc)
            compressed += "arp_opc:" + to_string(arp_opc.get()) + ",";

        auto etype = counter.get()->getEtherT();
        if (etype)
            compressed += "etype:" + to_string(etype.get()) + ",";

        auto proto = counter.get()->getProt();
        if (proto)
            compressed += "proto:" + to_string(proto.get()) + ",";

        auto s_from_port = counter.get()->getSFromPort();
        if (s_from_port)
            compressed += "sport:" + to_string(s_from_port.get());

        auto s_to_port = counter.get()->getSToPort();
        if (s_to_port)
            compressed += "-" + to_string(s_to_port.get()) + ",";

        auto d_from_port = counter.get()->getDFromPort();
        if (d_from_port)
            compressed += "dport:" + to_string(d_from_port.get());

        auto d_to_port = counter.get()->getDToPort();
        if (d_to_port)
            compressed += "-" + to_string(d_to_port.get()) + ",";

        auto frag_flags = counter.get()->getFragmentFlags();
        if (frag_flags)
            compressed += "frag_flags:" + to_string(frag_flags.get()) + ",";

        auto icmp_code = counter.get()->getIcmpCode();
        if (icmp_code)
            compressed += "icmp_code" + to_string(icmp_code.get()) + ",";

        auto icmp_type = counter.get()->getIcmpType();
        if (icmp_type)
            compressed += "icmp_type:" + to_string(icmp_type.get()) + ",";

        auto tcp_flags = counter.get()->getTcpFlags();
        if (tcp_flags)
            compressed += "tcp_flags:" + to_string(tcp_flags.get()) + ",";

        auto ct = counter.get()->getConnectionTracking();
        if (ct)
            compressed += "ct:" + to_string(ct.get());
    } else {
        LOG(DEBUG) << "No classifier found for tenant: " << tenant
                   << " classifier: " << classifier;
    }

    return compressed;
}

// Create RemoteEp gauge given metric type
void PrometheusManager::createDynamicGaugeRemoteEp (REMOTE_EP_METRICS metric)
{
    // Retrieve the Gauge if its already created
    if (getDynamicGaugeRemoteEp(metric))
        return;

    LOG(DEBUG) << "creating remote ep dyn gauge family"
               << " metric: " << metric;

    auto& gauge = gauge_remote_ep_family_ptr[metric]->Add({});
    remote_ep_gauge_map[metric] = &gauge;
}

// Create RDDropCounter gauge given metric type, rdURI
void PrometheusManager::createDynamicGaugeRDDrop (RDDROP_METRICS metric,
                                                  const string& rdURI)
{
    // Retrieve the Gauge if its already created
    if (getDynamicGaugeRDDrop(metric, rdURI))
        return;

    /* Example rdURI: /PolicyUniverse/PolicySpace/test/GbpRoutingDomain/rd/
     * We want to just get the tenant name and the vrf. */
    size_t tLow = rdURI.find("PolicySpace") + 12;
    size_t gRDStart = rdURI.rfind("GbpRoutingDomain");
    string tenant = rdURI.substr(tLow,gRDStart-tLow-1);
    size_t rHigh = rdURI.size()-2;
    size_t rLow = gRDStart+17;
    string rd = rdURI.substr(rLow,rHigh-rLow+1);

    auto& gauge = gauge_rddrop_family_ptr[metric]->Add({{"routing_domain",
                                                         tenant+":"+rd}});
    if (gauge_check.is_dup(&gauge)) {
        LOG(DEBUG) << "duplicate rddrop dyn gauge family"
                   << " metric: " << metric
                   << " rdURI: " << rdURI;
        return;
    }
    LOG(DEBUG) << "created rddrop dyn gauge family"
               << " metric: " << metric
               << " rdURI: " << rdURI;
    gauge_check.add(&gauge);
    rddrop_gauge_map[metric][rdURI] = &gauge;
}

// Create SvcTargetCounter gauge given metric type, svc-tgt uuid & ep attr_map
void PrometheusManager::createDynamicGaugeSvcTarget (SVC_TARGET_METRICS metric,
                                                     const string& uuid,
                                                     const string& nhip,
                    const unordered_map<string, string>&    svc_attr_map,
                    const unordered_map<string, string>&    ep_attr_map,
                                                     bool updateLabels)
{
    // During counter update from stats manager, dont create new gauge metric
    if (!updateLabels)
        return;

    auto const &label_map = createLabelMapFromSvcTargetAttr(nhip, svc_attr_map, ep_attr_map);
    auto hash_new = hash_labels(label_map);

    // Retrieve the Gauge if its already created
    auto const &mgauge = getDynamicGaugeSvcTarget(metric, uuid);
    if (mgauge) {
        /**
         * Detect attribute change by comparing hashes of cached label map
         * with new label map
         */
        if (hash_new == hash_labels(mgauge.get().first))
            return;
        else {
            LOG(DEBUG) << "addNupdate svctargetcounter uuid " << uuid
                       << "existing svc target metric, but deleting: hash modified;"
                       << " metric: " << svc_target_family_names[metric]
                       << " gaugeptr: " << mgauge.get().second;
            removeDynamicGaugeSvcTarget(metric, uuid);
        }
    }

    // We shouldnt add a gauge for SvcTarget which doesnt have svc-target name.
    // i.e. no vm-name for EPs
    if (!hash_new) {
        LOG(ERROR) << "label map is empty for svc-target dyn gauge family"
               << " metric: " << metric
               << " uuid: " << uuid;
        return;
    }

    auto& gauge = gauge_svc_target_family_ptr[metric]->Add(label_map);
    if (gauge_check.is_dup(&gauge)) {
        LOG(ERROR) << "duplicate svc-target dyn gauge family"
                   << " metric: " << metric
                   << " uuid: " << uuid
                   << " label hash: " << hash_new;
        return;
    }
    LOG(DEBUG) << "created svc-target dyn gauge family"
               << " metric: " << metric
               << " uuid: " << uuid
               << " label hash: " << hash_new;
    gauge_check.add(&gauge);
    svc_target_gauge_map[metric][uuid] = make_pair(std::move(label_map), &gauge);
}

// Create SvcCounter gauge given metric type, svc uuid & attr_map
void PrometheusManager::createDynamicGaugeSvc (SVC_METRICS metric,
                                               const string& uuid,
                    const unordered_map<string, string>&    svc_attr_map)
{
    // During counter update from stats manager, dont create new gauge metric
    if (svc_attr_map.size() == 0)
        return;

    auto const &label_map = createLabelMapFromSvcAttr(svc_attr_map);
    auto hash_new = hash_labels(label_map);

    // Retrieve the Gauge if its already created
    auto const &mgauge = getDynamicGaugeSvc(metric, uuid);
    if (mgauge) {
        /**
         * Detect attribute change by comparing hashes of cached label map
         * with new label map
         */
        if (hash_new == hash_labels(mgauge.get().first))
            return;
        else {
            LOG(DEBUG) << "addNupdate svccounter uuid " << uuid
                       << "existing svc metric, but deleting: hash modified;"
                       << " metric: " << svc_family_names[metric]
                       << " gaugeptr: " << mgauge.get().second;
            removeDynamicGaugeSvc(metric, uuid);
        }
    }

    // We shouldnt add a gauge for Svc which doesnt have svc name.
    if (!hash_new) {
        LOG(ERROR) << "label map is empty for svc dyn gauge family"
               << " metric: " << metric
               << " uuid: " << uuid;
        return;
    }

    auto& gauge = gauge_svc_family_ptr[metric]->Add(label_map);
    if (gauge_check.is_dup(&gauge)) {
        LOG(ERROR) << "duplicate svc dyn gauge family"
                   << " metric: " << metric
                   << " uuid: " << uuid
                   << " label hash: " << hash_new;
        return;
    }
    LOG(DEBUG) << "created svc dyn gauge family"
               << " metric: " << metric
               << " uuid: " << uuid
               << " label hash: " << hash_new;
    gauge_check.add(&gauge);
    svc_gauge_map[metric][uuid] = make_pair(std::move(label_map), &gauge);
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

    auto& gauge = gauge_podsvc_family_ptr[metric]->Add(label_map);
    if (gauge_check.is_dup(&gauge)) {
        LOG(ERROR) << "duplicate podsvc dyn gauge family"
                   << " metric: " << metric
                   << " uuid: " << uuid
                   << " label hash: " << hash_new;
        return;
    }
    LOG(DEBUG) << "created podsvc dyn gauge family"
               << " metric: " << metric
               << " uuid: " << uuid
               << " label hash: " << hash_new;
    gauge_check.add(&gauge);
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

    auto label_map = createLabelMapFromEpAttr(ep_name,
                                              attr_map,
                                              agent.getPrometheusEpAttributes());
    auto hash = hash_labels(label_map);
    auto& gauge = gauge_ep_family_ptr[metric]->Add(label_map);
    if (gauge_check.is_dup(&gauge)) {
        LOG(ERROR) << "duplicate ep dyn gauge family: " << ep_name
                   << " metric: " << metric
                   << " uuid: " << uuid
                   << " label hash: " << hash
                   << " gaugeptr: " << &gauge;
        // Note: return true so that: if metrics are created before and later
        // result in duplication due to change in attributes, the pre-duplicated
        // metrics can get freed.
        return true;
    }
    LOG(DEBUG) << "created ep dyn gauge family: " << ep_name
               << " metric: " << metric
               << " uuid: " << uuid
               << " label hash: " << hash
               << " gaugeptr: " << &gauge;
    gauge_check.add(&gauge);

    ep_gauge_map[metric][uuid] = make_pair(hash, &gauge);

    return true;
}

// Create a label map that can be used for annotation, given the ep attr map
const map<string,string> PrometheusManager::createLabelMapFromSvcTargetAttr (
                                                           const string& nhip,
                            const unordered_map<string, string>&  svc_attr_map,
                            const unordered_map<string, string>&  ep_attr_map)
{
    map<string,string>   label_map;

    // If there are multiple IPs per EP and if these 2 IPs are nexthops of service,
    // then gauge dup checker will crib for updates from these 2 IP flows.
    // Since this is the unique key for SvcTargetCounter, keeping it as part of
    // annotation. In grafana, we can filter out if the IP is not needed.
    label_map["ip"] = nhip;

    auto svc_name_itr = svc_attr_map.find("name");
    if (svc_name_itr != svc_attr_map.end()) {
        label_map["svc_name"] = svc_name_itr->second;
    }

    auto svc_ns_itr = svc_attr_map.find("namespace");
    if (svc_ns_itr != svc_attr_map.end()) {
        label_map["svc_namespace"] = svc_ns_itr->second;
    }

    auto svc_scope_itr = svc_attr_map.find("scope");
    if (svc_scope_itr != svc_attr_map.end()) {
        label_map["svc_scope"] = svc_scope_itr->second;
    }

    auto ep_name_itr = ep_attr_map.find("vm-name");
    if (ep_name_itr != ep_attr_map.end()) {
        label_map["ep_name"] = ep_name_itr->second;
    }

    auto ep_ns_itr = ep_attr_map.find("namespace");
    if (ep_ns_itr != ep_attr_map.end()) {
        label_map["ep_namespace"] = ep_ns_itr->second;
    }

    return label_map;
}

// Create a label map that can be used for annotation, given the svc attr_map
const map<string,string> PrometheusManager::createLabelMapFromSvcAttr (
                          const unordered_map<string, string>&  svc_attr_map)
{
    map<string,string>   label_map;

    auto svc_name_itr = svc_attr_map.find("name");
    // Ensuring svc's name is present in attributes
    // If not, there is no point in creating this metric
    if (svc_name_itr != svc_attr_map.end()) {
        label_map["name"] = svc_name_itr->second;
    } else {
        return label_map;
    }

    auto svc_ns_itr = svc_attr_map.find("namespace");
    if (svc_ns_itr != svc_attr_map.end()) {
        label_map["namespace"] = svc_ns_itr->second;
    }

    auto svc_scope_itr = svc_attr_map.find("scope");
    if (svc_scope_itr != svc_attr_map.end()) {
        label_map["scope"] = svc_scope_itr->second;
    }

    return label_map;
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

    auto svc_scope_itr = svc_attr_map.find("scope");
    if (svc_scope_itr != svc_attr_map.end()) {
        label_map["svc_scope"] = svc_scope_itr->second;
    }

    return label_map;
}

// Create a label map that can be used for annotation, given the ep attr map
map<string,string> PrometheusManager::createLabelMapFromEpAttr (
                                                           const string& ep_name,
                                 const unordered_map<string, string>&   attr_map,
                                 const unordered_set<string>&        allowed_set)
{
    map<string,string>   label_map;

    auto pod_itr = attr_map.find("vm-name");
    if (pod_itr != attr_map.end())
        label_map["name"] = pod_itr->second;
    else {
        // Note: if vm-name is not part of ep attributes, then just
        // set the ep_name as "name". This is to avoid label clash between
        // ep metrics that dont have vm-name.
        label_map["name"] = ep_name;
    }

    auto ns_itr = attr_map.find("namespace");
    if (ns_itr != attr_map.end())
        label_map["namespace"] = ns_itr->second;

    for (const auto& allowed : allowed_set) {

        if (!allowed.compare("vm-name")
                || !allowed.compare("namespace"))
            continue;

        auto allowed_itr = attr_map.find(allowed);
        if (allowed_itr != attr_map.end()) {
            // empty values can be discarded
            if (allowed_itr->second.empty())
                continue;

            // Label values can be anything in prometheus, but
            // the key has to cater to specific regex
            if (checkMetricName(allowed_itr->first)) {
                label_map[allowed_itr->first] = allowed_itr->second;
            } else {
                const auto& label = sanitizeMetricName(allowed_itr->first);
                LOG(DEBUG) << "ep attr not compatible with prometheus"
                           << " K:" << allowed_itr->first
                           << " V:" << allowed_itr->second
                           << " sanitized name:" << label;
                label_map[label] = allowed_itr->second;
            }
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

// Get ContractClassifierCounter gauge given the metric,
// name of srcEpg, dstEpg & classifier
Gauge * PrometheusManager::getDynamicGaugeContractClassifier (CONTRACT_METRICS metric,
                                                              const string& srcEpg,
                                                              const string& dstEpg,
                                                              const string& classifier)
{
    Gauge *pgauge = nullptr;
    const string& key = srcEpg+dstEpg+classifier;
    auto itr = contract_gauge_map[metric].find(key);
    if (itr == contract_gauge_map[metric].end()) {
        LOG(DEBUG) << "Dyn Gauge ContractClassifier stats not found"
                   << " metric: " << metric
                   << " srcEpg: " << srcEpg
                   << " dstEpg: " << dstEpg
                   << " classifier: " << classifier;
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

// Get RemoteEp gauge given the metric
Gauge * PrometheusManager::getDynamicGaugeRemoteEp (REMOTE_EP_METRICS metric)
{
    return remote_ep_gauge_map[metric];
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

// Get SvcTargetCounter gauge given the metric, uuid of SvcTarget
mgauge_pair_t PrometheusManager::getDynamicGaugeSvcTarget (SVC_TARGET_METRICS metric,
                                                           const string& uuid)
{
    mgauge_pair_t mgauge = boost::none;
    auto itr = svc_target_gauge_map[metric].find(uuid);
    if (itr == svc_target_gauge_map[metric].end()) {
        LOG(TRACE) << "Dyn Gauge SvcTargetCounter not found"
                   << " metric: " << metric
                   << " uuid: " << uuid;
    } else {
        mgauge = itr->second;
    }

    return mgauge;
}

// Get SvcCounter gauge given the metric, uuid of Svc
mgauge_pair_t PrometheusManager::getDynamicGaugeSvc (SVC_METRICS metric,
                                                     const string& uuid)
{
    mgauge_pair_t mgauge = boost::none;
    auto itr = svc_gauge_map[metric].find(uuid);
    if (itr == svc_gauge_map[metric].end()) {
        LOG(TRACE) << "Dyn Gauge SvcCounter not found"
                   << " metric: " << metric
                   << " uuid: " << uuid;
    } else {
        mgauge = itr->second;
    }

    return mgauge;
}

// Get PodSvcCounter gauge given the metric, uuid of Pod+Svc
mgauge_pair_t PrometheusManager::getDynamicGaugePodSvc (PODSVC_METRICS metric,
                                                  const string& uuid)
{
    mgauge_pair_t mgauge = boost::none;
    auto itr = podsvc_gauge_map[metric].find(uuid);
    if (itr == podsvc_gauge_map[metric].end()) {
        LOG(TRACE) << "Dyn Gauge PodSvcCounter not found"
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

// Remove dynamic ContractClassifierCounter gauge given a metic type and
// name of srcEpg, dstEpg & classifier
bool PrometheusManager::removeDynamicGaugeContractClassifier (CONTRACT_METRICS metric,
                                                              const string& srcEpg,
                                                              const string& dstEpg,
                                                              const string& classifier)
{
    Gauge *pgauge = getDynamicGaugeContractClassifier(metric,
                                                      srcEpg,
                                                      dstEpg,
                                                      classifier);
    if (pgauge) {
        LOG(DEBUG) << "remove ContractClassifierCounter"
                   << " srcEpg: " << srcEpg
                   << " dstEpg: " << dstEpg
                   << " classifier: " << classifier
                   << " metric: " << metric;
        const string& key = srcEpg+dstEpg+classifier;
        contract_gauge_map[metric].erase(key);
        gauge_check.remove(pgauge);
        gauge_contract_family_ptr[metric]->Remove(pgauge);
    } else {
        LOG(DEBUG) << "remove dynamic gauge contract stats not found"
                   << " srcEpg:" << srcEpg
                   << " dstEpg:" << dstEpg
                   << " classifier:" << classifier;
        return false;
    }
    return true;
}

// Remove dynamic ContractClassifierCounter gauge given a metric type
void PrometheusManager::removeDynamicGaugeContractClassifier (CONTRACT_METRICS metric)
{
    auto itr = contract_gauge_map[metric].begin();
    while (itr != contract_gauge_map[metric].end()) {
        LOG(DEBUG) << "Delete ContractClassifierCounter"
                   << " key: " << itr->first
                   << " Gauge: " << itr->second;
        gauge_check.remove(itr->second);
        gauge_contract_family_ptr[metric]->Remove(itr->second);
        itr++;
    }

    contract_gauge_map[metric].clear();
}

// Remove dynamic ContractClassifierCounter gauges for all metrics
void PrometheusManager::removeDynamicGaugeContractClassifier ()
{
    for (CONTRACT_METRICS metric=CONTRACT_METRICS_MIN;
            metric <= CONTRACT_METRICS_MAX;
                metric = CONTRACT_METRICS(metric+1)) {
        removeDynamicGaugeContractClassifier(metric);
    }
}

// Remove dynamic SGClassifierCounter gauge given a metic type and classifier
bool PrometheusManager::removeDynamicGaugeSGClassifier (SGCLASSIFIER_METRICS metric,
                                                        const string& classifier)
{
    Gauge *pgauge = getDynamicGaugeSGClassifier(metric, classifier);
    if (pgauge) {
        sgclassifier_gauge_map[metric].erase(classifier);
        gauge_check.remove(pgauge);
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
        gauge_check.remove(itr->second);
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

// Remove dynamic RemoteEp gauge given a metic type
bool PrometheusManager::removeDynamicGaugeRemoteEp (REMOTE_EP_METRICS metric)
{
    Gauge *pgauge = getDynamicGaugeRemoteEp(metric);
    if (pgauge) {
        gauge_remote_ep_family_ptr[metric]->Remove(pgauge);
    } else {
        LOG(DEBUG) << "remove dynamic gauge RemoteEp not found";
        return false;
    }
    return true;
}

// Remove dynamic RemoteEp gauges for all metrics
void PrometheusManager::removeDynamicGaugeRemoteEp ()
{
    for (REMOTE_EP_METRICS metric=REMOTE_EP_METRICS_MIN;
            metric <= REMOTE_EP_METRICS_MAX;
                metric = REMOTE_EP_METRICS(metric+1)) {
        removeDynamicGaugeRemoteEp(metric);
    }
}

// Remove dynamic RDDropCounter gauge given a metic type and rdURI
bool PrometheusManager::removeDynamicGaugeRDDrop (RDDROP_METRICS metric,
                                                  const string& rdURI)
{
    Gauge *pgauge = getDynamicGaugeRDDrop(metric, rdURI);
    if (pgauge) {
        rddrop_gauge_map[metric].erase(rdURI);
        gauge_check.remove(pgauge);
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
        gauge_check.remove(itr->second);
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
        gauge_check.remove(pgauge);
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
        gauge_check.remove(itr->second);
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

// Remove dynamic SvcTargetCounter gauge given a metic type and svc-target uuid
bool PrometheusManager::removeDynamicGaugeSvcTarget (SVC_TARGET_METRICS metric,
                                                     const string& uuid)
{
    auto mgauge = getDynamicGaugeSvcTarget(metric, uuid);
    if (mgauge) {
        auto &mpair = svc_target_gauge_map[metric][uuid];
        mpair.get().first.clear(); // free the label map
        svc_target_gauge_map[metric].erase(uuid);
        gauge_check.remove(mgauge.get().second);
        gauge_svc_target_family_ptr[metric]->Remove(mgauge.get().second);
    } else {
        LOG(DEBUG) << "remove dynamic gauge svc-target not found uuid:" << uuid;
        return false;
    }
    return true;
}

// Remove dynamic SvcTargetCounter gauge given a metric type
void PrometheusManager::removeDynamicGaugeSvcTarget (SVC_TARGET_METRICS metric)
{
    auto itr = svc_target_gauge_map[metric].begin();
    while (itr != svc_target_gauge_map[metric].end()) {
        LOG(DEBUG) << "Delete SvcTarget uuid: " << itr->first
                   << " Gauge: " << itr->second.get().second;
        gauge_check.remove(itr->second.get().second);
        gauge_svc_target_family_ptr[metric]->Remove(itr->second.get().second);
        itr->second.get().first.clear(); // free the label map
        itr++;
    }

    svc_target_gauge_map[metric].clear();
}

// Remove dynamic SvcTargetCounter gauges for all metrics
void PrometheusManager::removeDynamicGaugeSvcTarget ()
{
    for (SVC_TARGET_METRICS metric=SVC_TARGET_METRICS_MIN;
            metric <= SVC_TARGET_METRICS_MAX;
                metric = SVC_TARGET_METRICS(metric+1)) {
        removeDynamicGaugeSvcTarget(metric);
    }
}

// Remove dynamic SvcCounter gauge given a metic type and svc uuid
bool PrometheusManager::removeDynamicGaugeSvc (SVC_METRICS metric,
                                               const string& uuid)
{
    auto mgauge = getDynamicGaugeSvc(metric, uuid);
    if (mgauge) {
        auto &mpair = svc_gauge_map[metric][uuid];
        mpair.get().first.clear(); // free the label map
        svc_gauge_map[metric].erase(uuid);
        gauge_check.remove(mgauge.get().second);
        gauge_svc_family_ptr[metric]->Remove(mgauge.get().second);
    } else {
        LOG(DEBUG) << "remove dynamic gauge svc not found uuid:" << uuid;
        return false;
    }
    return true;
}

// Remove dynamic SvcCounter gauge given a metric type
void PrometheusManager::removeDynamicGaugeSvc (SVC_METRICS metric)
{
    auto itr = svc_gauge_map[metric].begin();
    while (itr != svc_gauge_map[metric].end()) {
        LOG(DEBUG) << "Delete Svc uuid: " << itr->first
                   << " Gauge: " << itr->second.get().second;
        gauge_check.remove(itr->second.get().second);
        gauge_svc_family_ptr[metric]->Remove(itr->second.get().second);
        itr->second.get().first.clear(); // free the label map
        itr++;
    }

    svc_gauge_map[metric].clear();
}

// Remove dynamic SvcCounter gauges for all metrics
void PrometheusManager::removeDynamicGaugeSvc ()
{
    for (SVC_METRICS metric=SVC_METRICS_MIN;
            metric <= SVC_METRICS_MAX;
                metric = SVC_METRICS(metric+1)) {
        removeDynamicGaugeSvc(metric);
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
        gauge_check.remove(mgauge.get().second);
        gauge_podsvc_family_ptr[metric]->Remove(mgauge.get().second);
    } else {
        LOG(TRACE) << "remove dynamic gauge podsvc not found uuid:" << uuid;
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
        gauge_check.remove(itr->second.get().second);
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
        gauge_check.remove(hgauge.get().second);
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
        gauge_check.remove(itr->second.get().second);
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

// Remove all statically  allocated svc counter families
void PrometheusManager::removeStaticCounterFamiliesSvc ()
{
    counter_svc_create_family_ptr = nullptr;
    counter_svc_remove_family_ptr = nullptr;

}

// Remove all statically  allocated counter families
void PrometheusManager::removeStaticCounterFamilies ()
{
    // EpCounter specific
    {
        const lock_guard<mutex> lock(ep_counter_mutex);
        removeStaticCounterFamiliesEp();
    }

    // SvcCounter specific
    {
        const lock_guard<mutex> lock(svc_counter_mutex);
        removeStaticCounterFamiliesSvc();
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

// Remove all statically allocated ContractClassifier gauge families
void PrometheusManager::removeStaticGaugeFamiliesContractClassifier ()
{
    for (CONTRACT_METRICS metric=CONTRACT_METRICS_MIN;
            metric <= CONTRACT_METRICS_MAX;
                metric = CONTRACT_METRICS(metric+1)) {
        gauge_contract_family_ptr[metric] = nullptr;
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

// Remove all statically allocated RemoteEp gauge families
void PrometheusManager::removeStaticGaugeFamiliesRemoteEp ()
{
    for (REMOTE_EP_METRICS metric=REMOTE_EP_METRICS_MIN;
            metric <= REMOTE_EP_METRICS_MAX;
                metric = REMOTE_EP_METRICS(metric+1)) {
        gauge_remote_ep_family_ptr[metric] = nullptr;
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

// Remove all statically allocated svc target gauge families
void PrometheusManager::removeStaticGaugeFamiliesSvcTarget()
{
    for (SVC_TARGET_METRICS metric=SVC_TARGET_METRICS_MIN;
            metric <= SVC_TARGET_METRICS_MAX;
                metric = SVC_TARGET_METRICS(metric+1)) {
        gauge_svc_target_family_ptr[metric] = nullptr;
    }
}

// Remove all statically allocated svc gauge families
void PrometheusManager::removeStaticGaugeFamiliesSvc()
{
    gauge_svc_total_family_ptr = nullptr;
    for (SVC_METRICS metric=SVC_METRICS_MIN;
            metric <= SVC_METRICS_MAX;
                metric = SVC_METRICS(metric+1)) {
        gauge_svc_family_ptr[metric] = nullptr;
    }
}

void PrometheusManager::removeStaticGaugeFamiliesTableDrop()
{
    const lock_guard<mutex> lock(table_drop_counter_mutex);
    for (TABLE_DROP_METRICS metric = TABLE_DROP_BYTES;
            metric <= TABLE_DROP_METRICS_MAX;
                metric = TABLE_DROP_METRICS(metric+1)) {
        gauge_table_drop_family_ptr[metric] = nullptr;
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

    // SvcTargetCounter specific
    {
        const lock_guard<mutex> lock(svc_target_counter_mutex);
        removeStaticGaugeFamiliesSvcTarget();
    }

    // SvcCounter specific
    {
        const lock_guard<mutex> lock(svc_counter_mutex);
        removeStaticGaugeFamiliesSvc();
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

    // RemoteEp specific
    {
        const lock_guard<mutex> lock(remote_ep_mutex);
        removeStaticGaugeFamiliesRemoteEp();
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

    // TableDrop specific
    removeStaticGaugeFamiliesTableDrop();

    // ContractClassifierCounter specific
    {
        const lock_guard<mutex> lock(contract_stats_mutex);
        removeStaticGaugeFamiliesContractClassifier();
    }
}

// Return a rolling hash of attribute map for the ep
size_t PrometheusManager::calcHashEpAttributes (const string& ep_name,
                      const unordered_map<string, string>&   attr_map,
                      const unordered_set<string>&        allowed_set)
{
    auto label_map = createLabelMapFromEpAttr(ep_name,
                                              attr_map,
                                              allowed_set);
    auto hash = hash_labels(label_map);
    LOG(DEBUG) << ep_name << ":calculated label hash = " << hash;
    return hash;
}

/* Function called from IntFlowManager to update PodSvcCounter */
void PrometheusManager::addNUpdatePodSvcCounter (bool isEpToSvc,
                                                 const string& uuid,
                                                 uint64_t bytes,
                                                 uint64_t pkts,
                  const unordered_map<string, string>& ep_attr_map,
                  const unordered_map<string, string>& svc_attr_map)
{
    RETURN_IF_DISABLED

    const lock_guard<mutex> lock(podsvc_counter_mutex);

    if (!exposeEpSvcNan && !pkts)
        return;

    if (isEpToSvc) {
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
                metric_opt = bytes;
                break;
            case PODSVC_EP2SVC_PKTS:
                metric_opt = pkts;
                break;
            default:
                LOG(ERROR) << "Unhandled eptosvc metric: " << metric;
            }
            if (metric_opt && mgauge)
                mgauge.get().second->Set(static_cast<double>(metric_opt.get()));
            if (!mgauge) {
                LOG(ERROR) << "ep2svc stats invalid update for uuid: " << uuid;
                break;
            }
        }
    } else {
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
                metric_opt = bytes;
                break;
            case PODSVC_SVC2EP_PKTS:
                metric_opt = pkts;
                break;
            default:
                LOG(ERROR) << "Unhandled svctoep metric: " << metric;
            }
            if (metric_opt && mgauge)
                mgauge.get().second->Set(static_cast<double>(metric_opt.get()));
            if (!mgauge) {
                LOG(ERROR) << "svc2ep stats invalid update for uuid: " << uuid;
                break;
            }
        }
    }
}

/* Function called from IntFlowManager and ServiceManager to update SvcTargetCounter
 * Note: SvcTargetCounter's key is the next-hop-IP. There could be a chance
 * that multiple next-hop IPs of a service are part of same EP/Pod.
 * In that case, the label annotion for both the svc-targets will be same
 * if we dont annotate the IP address to prometheus. Metric duplication
 * checker will detect this and will neither create nor update gauge metric
 * for the conflicting IP.
 * To avoid confusion and keep things simple, we will annotate with the nhip
 * as well to avoid duplicate metrics. We can avoid showing the IP in grafana
 * if its not of much value. */
void PrometheusManager::addNUpdateSvcTargetCounter (const string& uuid,
                                                    const string& nhip,
                                                    uint64_t rx_bytes,
                                                    uint64_t rx_pkts,
                                                    uint64_t tx_bytes,
                                                    uint64_t tx_pkts,
                         const unordered_map<string, string>& svc_attr_map,
                         const unordered_map<string, string>& ep_attr_map,
                                                    bool updateLabels)
{
    RETURN_IF_DISABLED
    const lock_guard<mutex> lock(svc_target_counter_mutex);

    const string& key = uuid+nhip;
    // Create the gauge counters if they arent present already
    for (SVC_TARGET_METRICS metric=SVC_TARGET_METRICS_MIN;
            metric <= SVC_TARGET_METRICS_MAX;
                metric = SVC_TARGET_METRICS(metric+1)) {
        createDynamicGaugeSvcTarget(metric,
                                    key,
                                    nhip,
                                    svc_attr_map,
                                    ep_attr_map,
                                    updateLabels);
    }

    // Update the metrics
    for (SVC_TARGET_METRICS metric=SVC_TARGET_METRICS_MIN;
            metric <= SVC_TARGET_METRICS_MAX;
                metric = SVC_TARGET_METRICS(metric+1)) {
        const mgauge_pair_t &mgauge = getDynamicGaugeSvcTarget(metric, key);
        uint64_t   metric_val = 0;
        switch (metric) {
        case SVC_TARGET_RX_BYTES:
            metric_val = rx_bytes;
            break;
        case SVC_TARGET_RX_PKTS:
            metric_val = rx_pkts;
            break;
        case SVC_TARGET_TX_BYTES:
            metric_val = tx_bytes;
            break;
        case SVC_TARGET_TX_PKTS:
            metric_val = tx_pkts;
            break;
        default:
            LOG(ERROR) << "Unhandled svc-target metric: " << metric;
        }
        if (mgauge)
            mgauge.get().second->Set(static_cast<double>(metric_val));
        if (!mgauge) {
            LOG(ERROR) << "svc-target stats invalid update for uuid: " << key;
            break;
        }
    }
}

/* Function called from IntFlowManager and ServiceManager to update SvcCounter */
void PrometheusManager::addNUpdateSvcCounter (const string& uuid,
                                              uint64_t rx_bytes,
                                              uint64_t rx_pkts,
                                              uint64_t tx_bytes,
                                              uint64_t tx_pkts,
                  const unordered_map<string, string>& svc_attr_map)
{
    RETURN_IF_DISABLED
    const lock_guard<mutex> lock(svc_counter_mutex);

    // Create the gauge counters if they arent present already
    for (SVC_METRICS metric=SVC_METRICS_MIN;
            metric <= SVC_METRICS_MAX;
                metric = SVC_METRICS(metric+1)) {
        createDynamicGaugeSvc(metric,
                              uuid,
                              svc_attr_map);
    }

    // Update the metrics
    for (SVC_METRICS metric=SVC_METRICS_MIN;
            metric <= SVC_METRICS_MAX;
                metric = SVC_METRICS(metric+1)) {
        const mgauge_pair_t &mgauge = getDynamicGaugeSvc(metric, uuid);
        uint64_t   metric_val = 0;
        switch (metric) {
        case SVC_RX_BYTES:
            metric_val = rx_bytes;
            break;
        case SVC_RX_PKTS:
            metric_val = rx_pkts;
            break;
        case SVC_TX_BYTES:
            metric_val = tx_bytes;
            break;
        case SVC_TX_PKTS:
            metric_val = tx_pkts;
            break;
        default:
            LOG(ERROR) << "Unhandled svc metric: " << metric;
        }
        if (mgauge)
            mgauge.get().second->Set(static_cast<double>(metric_val));
        if (!mgauge) {
            LOG(ERROR) << "svc stats invalid update for uuid: " << uuid;
            break;
        }
    }
}

/* Function called from ContractStatsManager to add/update ContractClassifierCounter */
void PrometheusManager::addNUpdateContractClassifierCounter (const string& srcEpg,
                                                             const string& dstEpg,
                                                             const string& classifier)
{
    RETURN_IF_DISABLED
    using namespace modelgbp::gbpe;
    using namespace modelgbp::observer;

    const lock_guard<mutex> lock(contract_stats_mutex);

    Mutator mutator(framework, "policyelement");
    optional<shared_ptr<PolicyStatUniverse> > su =
                    PolicyStatUniverse::resolve(agent.getFramework());
    if (su) {
        vector<OF_SHARED_PTR<L24ClassifierCounter> > out;
        su.get()->resolveGbpeL24ClassifierCounter(out);
        for (const auto& counter : out) {
            if (!counter)
                continue;
            if (srcEpg.compare(counter.get()->getSrcEpg("")))
                continue;
            if (dstEpg.compare(counter.get()->getDstEpg("")))
                continue;
            if (classifier.compare(counter.get()->getClassifier("")))
                continue;
            if (counter.get()->getGenId(0) != (contract_last_genId+1))
                continue;
            contract_last_genId++;

            for (CONTRACT_METRICS metric=CONTRACT_METRICS_MIN;
                    metric <= CONTRACT_METRICS_MAX;
                        metric = CONTRACT_METRICS(metric+1))
                if (!createDynamicGaugeContractClassifier(metric,
                                                          srcEpg,
                                                          dstEpg,
                                                          classifier))
                    break;

            // Update the metrics
            for (CONTRACT_METRICS metric=CONTRACT_METRICS_MIN;
                    metric <= CONTRACT_METRICS_MAX;
                        metric = CONTRACT_METRICS(metric+1)) {
                Gauge *pgauge = getDynamicGaugeContractClassifier(metric,
                                                                  srcEpg,
                                                                  dstEpg,
                                                                  classifier);
                optional<uint64_t>   metric_opt;
                switch (metric) {
                case CONTRACT_BYTES:
                    metric_opt = counter.get()->getBytes();
                    break;
                case CONTRACT_PACKETS:
                    metric_opt = counter.get()->getPackets();
                    break;
                default:
                    LOG(ERROR) << "Unhandled contract metric: " << metric;
                }
                if (metric_opt && pgauge)
                    pgauge->Set(pgauge->Value() \
                                + static_cast<double>(metric_opt.get()));
                if (!pgauge) {
                    LOG(ERROR) << "Invalid sgclassifier update"
                               << " srcEpg: " << srcEpg
                               << " dstEpg: " << dstEpg
                               << " classifier: " << classifier;
                    break;
                }
            }
        }
        out.clear();
    }
}

/* Function called from SecGrpStatsManager to add/update SGClassifierCounter */
void PrometheusManager::addNUpdateSGClassifierCounter (const string& classifier)
{
    RETURN_IF_DISABLED
    using namespace modelgbp::gbpe;
    using namespace modelgbp::observer;

    const lock_guard<mutex> lock(sgclassifier_stats_mutex);

    Mutator mutator(framework, "policyelement");
    optional<shared_ptr<PolicyStatUniverse> > su =
                    PolicyStatUniverse::resolve(agent.getFramework());
    if (su) {
        vector<OF_SHARED_PTR<SecGrpClassifierCounter> > out;
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
                if (!pgauge) {
                    LOG(ERROR) << "Invalid sgclassifier update classifier: " << classifier;
                    break;
                }
            }
        }
        out.clear();
    }
}

/* Function called from ServiceManager to increment service count */
void PrometheusManager::incSvcCounter (void)
{
    RETURN_IF_DISABLED
    const lock_guard<mutex> lock(svc_counter_mutex);
    incStaticCounterSvcCreate();
    updateStaticGaugeSvcTotal(true);
}

/* Function called from ServiceManager to decrement service count */
void PrometheusManager::decSvcCounter (void)
{
    RETURN_IF_DISABLED
    const lock_guard<mutex> lock(svc_counter_mutex);
    incStaticCounterSvcRemove();
    updateStaticGaugeSvcTotal(false);
}

/* Function called from EndpointManager to create/update RemoteEp count */
void PrometheusManager::addNUpdateRemoteEpCount (size_t count)
{
    RETURN_IF_DISABLED
    const lock_guard<mutex> lock(remote_ep_mutex);

    for (REMOTE_EP_METRICS metric=REMOTE_EP_METRICS_MIN;
            metric <= REMOTE_EP_METRICS_MAX;
                metric = REMOTE_EP_METRICS(metric+1)) {
        // create the metric if its not present
        createDynamicGaugeRemoteEp(metric);
        Gauge *pgauge = getDynamicGaugeRemoteEp(metric);
        if (pgauge)
            pgauge->Set(static_cast<double>(count));
    }
}

/* Function called from ContractStatsManager to update RDDropCounter
 * This will be called from IntFlowManager to create metrics. */
void PrometheusManager::addNUpdateRDDropCounter (const string& rdURI,
                                                 bool isAdd)
{
    RETURN_IF_DISABLED
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
        vector<OF_SHARED_PTR<RoutingDomainDropCounter> > out;
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
                if (!pgauge) {
                    LOG(ERROR) << "Invalid rddrop update rdURI: " << rdURI;
                    break;
                }
            }
        }
        out.clear();
    }
}

/* Function called from PolicyStatsManager to update OFPeerStats */
void PrometheusManager::addNUpdateOFPeerStats (void)
{
    RETURN_IF_DISABLED
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
                if (!pgauge) {
                    LOG(ERROR) << "Invalid ofpeer update peer: " << peerStat.first;
                    break;
                }
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
    RETURN_IF_DISABLED
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
                if (!hgauge) {
                    LOG(ERROR) << "ep stats invalid update for uuid: " << uuid;
                    break;
                }
            }
        }
    }
}

void PrometheusManager::dumpPodSvcState ()
{
    LOG(DEBUG) << "######### PODSVC STATE: #########";
    for (PODSVC_METRICS metric=PODSVC_METRICS_MIN;
            metric <= PODSVC_METRICS_MAX;
                metric = PODSVC_METRICS(metric+1)) {
        for (auto &p : podsvc_gauge_map[metric]) {
            LOG(DEBUG) << "   metric: " << podsvc_family_names[metric]
                       << "   uuid: " << p.first
                       << "   gauge_ptr: " << p.second.get().second;
        }
    }
}

// Function called from ServiceManager to remove SvcTargetCounter
void PrometheusManager::removeSvcTargetCounter (const string& uuid,
                                                const string& nhip)
{
    RETURN_IF_DISABLED
    const lock_guard<mutex> lock(svc_target_counter_mutex);

    const string& key = uuid+nhip;
    LOG(DEBUG) << "remove svc-target counter uuid: " << key;

    for (SVC_TARGET_METRICS metric=SVC_TARGET_METRICS_MIN;
            metric <= SVC_TARGET_METRICS_MAX;
                metric = SVC_TARGET_METRICS(metric+1)) {
        if (!removeDynamicGaugeSvcTarget(metric, key))
            break;
    }
}

// Function called from ServiceManager to remove SvcCounter
void PrometheusManager::removeSvcCounter (const string& uuid)
{
    RETURN_IF_DISABLED
    const lock_guard<mutex> lock(svc_counter_mutex);

    LOG(DEBUG) << "remove svc counter uuid: " << uuid;

    for (SVC_METRICS metric=SVC_METRICS_MIN;
            metric <= SVC_METRICS_MAX;
                metric = SVC_METRICS(metric+1)) {
        if (!removeDynamicGaugeSvc(metric, uuid))
            break;
    }
}

// Function called from IntFlowManager to remove PodSvcCounter
void PrometheusManager::removePodSvcCounter (bool isEpToSvc,
                                             const string& uuid)
{
    RETURN_IF_DISABLED
    const lock_guard<mutex> lock(podsvc_counter_mutex);


    if (isEpToSvc) {
        for (PODSVC_METRICS metric=PODSVC_EP2SVC_MIN;
                metric <= PODSVC_EP2SVC_MAX;
                    metric = PODSVC_METRICS(metric+1)) {
            if (!removeDynamicGaugePodSvc(metric, uuid)) {
                break;
            } else {
                LOG(DEBUG) << "remove podsvc counter"
                           << " eptosvc uuid: " << uuid
                           << " metric: " << metric;
            }
        }
    } else {
        for (PODSVC_METRICS metric=PODSVC_SVC2EP_MIN;
                metric <= PODSVC_SVC2EP_MAX;
                    metric = PODSVC_METRICS(metric+1)) {
            if (!removeDynamicGaugePodSvc(metric, uuid)) {
                break;
            } else {
                LOG(DEBUG) << "remove podsvc counter"
                           << " svctoep uuid: " << uuid
                           << " metric: " << metric;
            }
        }
    }
}

// Function called from EP Manager to remove EpCounter
void PrometheusManager::removeEpCounter (const string& uuid,
                                         const string& ep_name)
{
    RETURN_IF_DISABLED
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

// Function called from ContractStatsManager to remove ContractClassifierCounter
void PrometheusManager::removeContractClassifierCounter (const string& srcEpg,
                                                         const string& dstEpg,
                                                         const string& classifier)
{
    RETURN_IF_DISABLED
    const lock_guard<mutex> lock(contract_stats_mutex);
    for (CONTRACT_METRICS metric=CONTRACT_METRICS_MIN;
            metric <= CONTRACT_METRICS_MAX;
                metric = CONTRACT_METRICS(metric+1)) {
        if (!removeDynamicGaugeContractClassifier(metric,
                                                  srcEpg,
                                                  dstEpg,
                                                  classifier))
            break;
    }
}

// Function called from SecGrpStatsManager to remove SGClassifierCounter
void PrometheusManager::removeSGClassifierCounter (const string& classifier)
{
    RETURN_IF_DISABLED
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
    RETURN_IF_DISABLED
    const lock_guard<mutex> lock(rddrop_stats_mutex);
    LOG(DEBUG) << "remove RDDropCounter rdURI: " << rdURI;

    for (RDDROP_METRICS metric=RDDROP_METRICS_MIN;
            metric <= RDDROP_METRICS_MAX;
                metric = RDDROP_METRICS(metric+1)) {
        if (!removeDynamicGaugeRDDrop(metric, rdURI))
            break;
    }
}

const map<string,string> PrometheusManager::createLabelMapFromTableDropKey(
        const string& bridge_name,
        const string& table_name)
{
   map<string,string>   label_map;
   string table_str = bridge_name + string("_") + table_name;
   label_map["table"] = table_str;
   return label_map;
}

mgauge_pair_t PrometheusManager::getStaticGaugeTableDrop(TABLE_DROP_METRICS metric,
                                          const string& bridge_name,
                                          const string& table_name)
{
    const string table_drop_key = bridge_name + table_name;
    const auto &gauge_itr = table_drop_gauge_map[metric].find(table_drop_key);
    if(gauge_itr == table_drop_gauge_map[metric].end()){
        return boost::none;
    }
    return gauge_itr->second;
}

void PrometheusManager::createStaticGaugeTableDrop (const string& bridge_name,
                                                    const string& table_name)
{
    RETURN_IF_DISABLED
    if ((bridge_name.empty() || table_name.empty()))
        return;

    auto const &label_map = createLabelMapFromTableDropKey(bridge_name,
                                                           table_name);
    {
        const lock_guard<mutex> lock(table_drop_counter_mutex);
        // Retrieve the Gauge if its already created
        auto const &mgauge = getStaticGaugeTableDrop(TABLE_DROP_BYTES,
                                                     bridge_name,
                                                     table_name);
        if(!mgauge) {
            for (TABLE_DROP_METRICS metric=TABLE_DROP_BYTES;
                        metric <= TABLE_DROP_MAX;
                        metric = TABLE_DROP_METRICS(metric+1)) {
                auto& gauge = gauge_table_drop_family_ptr[metric]->Add(label_map);
                if (gauge_check.is_dup(&gauge)) {
                    LOG(ERROR) << "duplicate table drop static gauge"
                               << " bridge_name: " << bridge_name
                               << " table name: " << table_name;
                    return;
                }
                LOG(DEBUG) << "created table drop static gauge"
                           << " bridge_name: " << bridge_name
                           << " table name: " << table_name;
                gauge_check.add(&gauge);
                string table_drop_key = bridge_name + table_name;
                table_drop_gauge_map[metric][table_drop_key] =
                        make_pair(std::move(label_map), &gauge);
            }
        }
    }
}

void PrometheusManager::removeTableDropGauge (const string& bridge_name,
                                              const string& table_name)
{
    RETURN_IF_DISABLED
    string table_drop_key = bridge_name + table_name;

    const lock_guard<mutex> lock(table_drop_counter_mutex);

    for(TABLE_DROP_METRICS metric = TABLE_DROP_BYTES;
            metric <= TABLE_DROP_MAX; metric = TABLE_DROP_METRICS(metric+1)) {
        auto const &mgauge = getStaticGaugeTableDrop(metric,
                                                     bridge_name,
                                                     table_name);
        // Note: mgauge can be boost::none if the create resulted in a
        // duplicate metric.
        if (mgauge) {
            gauge_check.remove(mgauge.get().second);
            gauge_table_drop_family_ptr[metric]->Remove(mgauge.get().second);
            table_drop_gauge_map[metric].erase(table_drop_key);
        }
    }
}

void PrometheusManager::updateTableDropGauge (const string& bridge_name,
                                              const string& table_name,
                                              const uint64_t &bytes,
                                              const uint64_t &packets)
{
    RETURN_IF_DISABLED
    const lock_guard<mutex> lock(table_drop_counter_mutex);
    // Update the metrics
    const mgauge_pair_t &mgauge_bytes = getStaticGaugeTableDrop(
                                                    TABLE_DROP_BYTES,
                                                    bridge_name,
                                                    table_name);
    if (mgauge_bytes) {
        mgauge_bytes.get().second->Set(static_cast<double>(bytes));
    } else {
        LOG(ERROR) << "Invalid bytes update for table drop"
                   << " bridge_name: " << bridge_name
                   << " table_name: " << table_name;
        return;
    }
    const mgauge_pair_t &mgauge_packets = getStaticGaugeTableDrop(
                                                        TABLE_DROP_PKTS,
                                                        bridge_name,
                                                        table_name);
    if (mgauge_packets) {
        mgauge_packets.get().second->Set(static_cast<double>(packets));
    } else {
        LOG(ERROR) << "Invalid pkts update for table drop"
                   << " bridge_name: " << bridge_name
                   << " table_name: " << table_name;
        return;
    }
}

} /* namespace opflexagent */
