/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for Agent class
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <glog/logging.h>
#include <boost/foreach.hpp>
#include <modelgbp/dmtree/Root.hpp>

#include "Agent.h"
#include "FSEndpointSource.h"

namespace ovsagent {

using opflex::modb::ModelMetadata;
using opflex::modb::Mutator;
using opflex::ofcore::OFFramework;
using boost::shared_ptr;
using boost::property_tree::ptree;
using boost::optional;

Agent::Agent(OFFramework& framework_) 
    : framework(framework_), policyManager(framework), 
      endpointManager(framework, policyManager) {

}

Agent::~Agent() {
}

void Agent::setProperties(const boost::property_tree::ptree& properties) {
    // A list of filesystem paths that we should check for endpoint
    // information
    static const std::string ENDPOINT_SOURCE_PATH("endpoint-sources.filesystem");

    optional<const ptree&> endpointSource = 
        properties.get_child_optional(ENDPOINT_SOURCE_PATH);
    BOOST_FOREACH(const ptree::value_type &v, endpointSource.get())
        endpointSourcePaths.insert(v.second.data());

    if (!endpointSource) {
        LOG(WARNING) << "No endpoint source found in configuration.";
    }
}

void Agent::start() {
    LOG(INFO) << "Starting OVS Agent";
    // instantiate the opflex framework
    framework.setModel(modelgbp::getMetadata());
    framework.start();

    Mutator mutator(framework, "init");
    shared_ptr<modelgbp::dmtree::Root> root = 
        modelgbp::dmtree::Root::createRootElement(framework);
    root->addPolicyUniverse();
    root->addRelatorUniverse();
    root->addEprL2Universe();
    root->addEprL3Universe();
    root->addEpdrL2Discovered();
    root->addEpdrL3Discovered();
    mutator.commit();

    // instantiate other components
    policyManager.start();
    endpointManager.start();

    BOOST_FOREACH(const std::string& path, endpointSourcePaths) {
        EndpointSource* source = new FSEndpointSource(&endpointManager, path);
        endpointSources.insert(source);
    }
}

void Agent::stop() {
    LOG(INFO) << "Stopping OVS Agent";

    BOOST_FOREACH(EndpointSource* source, endpointSources) {
        delete source;
    }
    endpointSources.clear();

    endpointManager.stop();
    policyManager.stop();
    framework.stop();
}

} /* namespace ovsagent */
