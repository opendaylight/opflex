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

#include <boost/foreach.hpp>
#include <boost/unordered_map.hpp>
#include <boost/assign/list_of.hpp>
#include <modelgbp/dmtree/Root.hpp>

#include "Agent.h"
#include "FSEndpointSource.h"
#include "logging.h"
#include "StitchedModeRenderer.h"

namespace ovsagent {

using opflex::modb::ModelMetadata;
using opflex::modb::Mutator;
using opflex::ofcore::OFFramework;
using boost::shared_ptr;
using boost::property_tree::ptree;
using boost::optional;
using std::make_pair;

Agent::Agent(OFFramework& framework_) 
    : framework(framework_), policyManager(framework), 
      endpointManager(framework, policyManager) {

}

Agent::~Agent() {
    BOOST_FOREACH(Renderer* r, renderers) {
        delete r;
    }
    renderers.clear();
}

void Agent::setProperties(const boost::property_tree::ptree& properties) {
    // A list of filesystem paths that we should check for endpoint
    // information
    static const std::string ENDPOINT_SOURCE_PATH("endpoint-sources.filesystem");
    static const std::string RUNTIME_STATE_PATH("runtime-state-path");
    static const std::string OPFLEX_PEERS("opflex.peers");
    static const std::string HOSTNAME("hostname");
    static const std::string PORT("port");

    static const std::string OPFLEX_NAME("opflex.name");
    static const std::string OPFLEX_DOMAIN("opflex.domain");

    static const std::string RENDERERS_STITCHED_MODE("renderers.stitched-mode");

    optional<const ptree&> endpointSource = 
        properties.get_child_optional(ENDPOINT_SOURCE_PATH);

    if (!endpointSource) {
        LOG(ERROR) << "No endpoint source found in configuration.";
    } else {
        BOOST_FOREACH(const ptree::value_type &v, endpointSource.get())
            endpointSourcePaths.insert(v.second.data());
    }

    optional<std::string> opflexName = 
        properties.get_optional<std::string>(OPFLEX_NAME);
    optional<std::string> opflexDomain = 
        properties.get_optional<std::string>(OPFLEX_DOMAIN);

    if (!opflexName || !opflexDomain) {
        LOG(ERROR) << "Opflex name and domain must be set";
    } else {
        framework.setOpflexIdentity(opflexName.get(),
                                    opflexDomain.get());
    }

    optional<std::string> rsp =
        properties.get_optional<std::string>(RUNTIME_STATE_PATH);
    if (!rsp) {
        LOG(ERROR) << "Runtime state path not found in configuration.";
    } else {
        runtimeStatePath = rsp.get();
    }

    optional<const ptree&> peers = 
        properties.get_child_optional(OPFLEX_PEERS);
    if (!peers) {
        LOG(ERROR) << "No Opflex peers found in configuration";
    } else {
        BOOST_FOREACH(const ptree::value_type &v, peers.get()) {
            optional<std::string> h = 
                v.second.get_optional<std::string>(HOSTNAME);
            optional<int> p = 
                v.second.get_optional<int>(PORT);
            if (h && p) {
                opflexPeers.insert(make_pair(h.get(), p.get()));
            }
        }
    }
    typedef Renderer* (*rend_create)(Agent&);
    typedef boost::unordered_map<std::string, rend_create> rend_map_t;
    static rend_map_t rend_map =
        boost::assign::map_list_of(RENDERERS_STITCHED_MODE,
                                   StitchedModeRenderer::create);

    BOOST_FOREACH(rend_map_t::value_type& v, rend_map) {
        optional<const ptree&> rtree = 
            properties.get_child_optional(v.first);
        if (rtree) {
            Renderer* r = v.second(*this);
            renderers.push_back(r);
            r->setProperties(rtree.get());
        }
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
    root->addObserverEpStatUniverse();
    mutator.commit();

    BOOST_FOREACH(const host_t& h, opflexPeers)
        framework.addPeer(h.first, h.second);

    // instantiate other components
    policyManager.start();
    endpointManager.start();

    BOOST_FOREACH(const std::string& path, endpointSourcePaths) {
        EndpointSource* source = new FSEndpointSource(&endpointManager, path);
        endpointSources.insert(source);
    }

    BOOST_FOREACH(Renderer* r, renderers) {
        r->start();
    }

    io_service_thread = new boost::thread(boost::ref(*this));
}

void Agent::stop() {
    LOG(INFO) << "Stopping OVS Agent";

    BOOST_FOREACH(Renderer* r, renderers) {
        r->stop();
    }

    BOOST_FOREACH(EndpointSource* source, endpointSources) {
        delete source;
    }
    endpointSources.clear();

    endpointManager.stop();
    policyManager.stop();
    framework.stop();

    io_service_thread->join();
    delete io_service_thread;
}

void Agent::operator()() {
    agent_io.run();
}

} /* namespace ovsagent */
