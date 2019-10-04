/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ExtraConfigManager class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/ExtraConfigManager.h>
#include <modelgbp/observer/DropLogConfig.hpp>
#include <modelgbp/policy/Universe.hpp>
#include <modelgbp/observer/DropFlowConfigUniverse.hpp>
#include <modelgbp/observer/DropFlowConfig.hpp>
#include <opflex/modb/Mutator.h>
#include <opflexagent/logging.h>

#include <memory>

namespace opflexagent {

using std::string;
using std::unique_lock;
using std::mutex;
using boost::optional;
using std::shared_ptr;
using std::unordered_set;
using std::make_shared;
using opflex::modb::Mutator;

ExtraConfigManager::ExtraConfigManager(opflex::ofcore::OFFramework& _framework):framework(_framework) {

}

void ExtraConfigManager::registerListener(ExtraConfigListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    extraConfigListeners.push_back(listener);
}

void ExtraConfigManager::unregisterListener(ExtraConfigListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    extraConfigListeners.remove(listener);
}

void ExtraConfigManager::notifyListeners(const opflex::modb::URI& domainURI) {
    unique_lock<mutex> guard(listener_mutex);
    for (ExtraConfigListener* listener : extraConfigListeners) {
        listener->rdConfigUpdated(domainURI);
    }
}

shared_ptr<const RDConfig>
ExtraConfigManager::getRDConfig(const opflex::modb::URI& uri) {
    unique_lock<mutex> guard(ec_mutex);
    rdc_map_t::const_iterator it = rdc_map.find(uri);
    if (it != rdc_map.end())
        return it->second.rdConfig;
    return shared_ptr<const RDConfig>();
}

void ExtraConfigManager::updateRDConfig(const RDConfig& rdConfig) {
    unique_lock<mutex> guard(ec_mutex);
    RDConfigState& as = rdc_map[rdConfig.getDomainURI()];

    as.rdConfig = make_shared<const RDConfig>(rdConfig);

    guard.unlock();
    notifyListeners(rdConfig.getDomainURI());
}

void ExtraConfigManager::removeRDConfig(const opflex::modb::URI& uri) {
    unique_lock<mutex> guard(ec_mutex);
    rdc_map_t::iterator it = rdc_map.find(uri);
    if (it != rdc_map.end()) {
        rdc_map.erase(it);
    }

    guard.unlock();
    notifyListeners(uri);
}

void ExtraConfigManager::notifyPacketDropLogConfigListeners(const opflex::modb::URI &dropLogCfgURI) {
    unique_lock<mutex> guard(listener_mutex);
    for (ExtraConfigListener* listener : extraConfigListeners) {
        listener->packetDropLogConfigUpdated(dropLogCfgURI);
    }
}

void ExtraConfigManager::notifyPacketDropFlowConfigListeners(const opflex::modb::URI &dropFlowURI) {
    unique_lock<mutex> guard(listener_mutex);
    for (ExtraConfigListener* listener : extraConfigListeners) {
        listener->packetDropFlowConfigUpdated(dropFlowURI);
    }
}

void ExtraConfigManager::packetDropLogConfigUpdated(PacketDropLogConfig &dropCfg) {
    using modelgbp::observer::DropLogConfig;

    Mutator mutator(framework, "policyelement");
    optional<shared_ptr<modelgbp::policy::Universe>> polUni =
            modelgbp::policy::Universe::resolve(framework);
    if(dropCfg.filePath.empty()){
        DropLogConfig::remove(framework, dropCfg.dropLogCfgURI);
    } else {
        shared_ptr<DropLogConfig> dropLogCfg =
                polUni.get()->addObserverDropLogConfig();
        dropLogCfg->setDropLogMode(dropCfg.dropLogMode);
        dropLogCfg->setDropLogEnable((const uint8_t)dropCfg.dropLogEnable);
    }
    mutator.commit();
    notifyPacketDropLogConfigListeners(dropCfg.dropLogCfgURI);
}

void ExtraConfigManager::packetDropFlowConfigUpdated(PacketDropFlowConfig &dropFlow) {
    using modelgbp::observer::DropFlowConfigUniverse;
    using modelgbp::observer::DropFlowConfig;
    Mutator mutator(framework, "policyelement");
    optional<shared_ptr<DropFlowConfigUniverse>> dropFlowUni =
            DropFlowConfigUniverse::resolve(framework);
    if(dropFlow.filePath.empty()) {
        DropFlowConfig::remove(framework, dropFlow.dropFlowURI);
        mutator.commit();
        notifyPacketDropFlowConfigListeners(dropFlow.dropFlowURI);
        return;
    }
    shared_ptr<DropFlowConfig> dropFlowCfg = dropFlowUni.get()
        ->addObserverDropFlowConfig(dropFlow.spec.uuid);
    if(dropFlow.spec.outerSrc) {
        dropFlowCfg->setOuterSrcAddress(
                dropFlow.spec.outerSrc.get().to_string());
    }
    if(dropFlow.spec.outerDst) {
        dropFlowCfg->setOuterDstAddress(
                dropFlow.spec.outerDst.get().to_string());
    }
    if(dropFlow.spec.innerSrc) {
        dropFlowCfg->setInnerSrcAddress(
                dropFlow.spec.innerSrc.get().to_string());
    }
    if(dropFlow.spec.innerDst) {
        dropFlowCfg->setInnerDstAddress(
                dropFlow.spec.innerDst.get().to_string());
    }
    if(dropFlow.spec.innerSrcMac) {
        dropFlowCfg->setInnerSrcMac(
                dropFlow.spec.innerSrcMac.get());
    }
    if(dropFlow.spec.innerDstMac) {
        dropFlowCfg->setInnerDstMac(
                dropFlow.spec.innerDstMac.get());
    }
    if(dropFlow.spec.ethType) {
        dropFlowCfg->setEthType(dropFlow.spec.ethType.get());
    }
    if(dropFlow.spec.ipProto) {
        dropFlowCfg->setIpProto(dropFlow.spec.ipProto.get());
    }
    if(dropFlow.spec.sPort) {
        dropFlowCfg->setSrcPort(dropFlow.spec.sPort.get());
    }
    if(dropFlow.spec.dPort) {
        dropFlowCfg->setDstPort(dropFlow.spec.dPort.get());
    }
    mutator.commit();
    notifyPacketDropFlowConfigListeners(dropFlow.dropFlowURI);

}

} /* namespace opflexagent */
