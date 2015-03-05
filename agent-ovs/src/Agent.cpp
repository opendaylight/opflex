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

#include "cmd.h"
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
using boost::asio::io_service;
using boost::bind;
using boost::ref;
using boost::thread;
using std::make_pair;

Agent::Agent(OFFramework& framework_) 
    : framework(framework_), policyManager(framework), 
      endpointManager(framework, policyManager), 
      io_service_thread(NULL), started(false) {

}

Agent::~Agent() {
    stop();
    BOOST_FOREACH(Renderer* r, renderers) {
        delete r;
    }
    renderers.clear();
}

#define DEF_SOCKET LOCALSTATEDIR"/run/opflex-agent-ovs-inspect.sock"

void Agent::setProperties(const boost::property_tree::ptree& properties) {
    static const std::string LOG_LEVEL("log.level");
    // A list of filesystem paths that we should check for endpoint
    // information
    static const std::string ENDPOINT_SOURCE_PATH("endpoint-sources.filesystem");
    static const std::string OPFLEX_PEERS("opflex.peers");
    static const std::string OPFLEX_SSL_MODE("opflex.ssl.mode");
    static const std::string OPFLEX_SSL_CA_STORE("opflex.ssl.ca-store");
    static const std::string HOSTNAME("hostname");
    static const std::string PORT("port");
    static const std::string OPFLEX_INSPECTOR("opflex.inspector.enabled");
    static const std::string OPFLEX_INSPECTOR_SOCK("opflex.inspector.socket-name");

    static const std::string OPFLEX_NAME("opflex.name");
    static const std::string OPFLEX_DOMAIN("opflex.domain");

    static const std::string RENDERERS_STITCHED_MODE("renderers.stitched-mode");

    optional<std::string> logLvl =
        properties.get_optional<std::string>(LOG_LEVEL);
    if (logLvl) {
        setLoggingLevel(logLvl.get());
    }

    optional<const ptree&> endpointSource = 
        properties.get_child_optional(ENDPOINT_SOURCE_PATH);

    if (endpointSource) {
        BOOST_FOREACH(const ptree::value_type &v, endpointSource.get())
            endpointSourcePaths.insert(v.second.data());
    }
    if (endpointSourcePaths.size() == 0)
        LOG(ERROR) << "No endpoint sources found in configuration.";

    optional<std::string> opflexName = 
        properties.get_optional<std::string>(OPFLEX_NAME);
    optional<std::string> opflexDomain = 
        properties.get_optional<std::string>(OPFLEX_DOMAIN);

    if (!opflexName || !opflexDomain) {
        LOG(ERROR) << "Opflex name and domain must be set";
    } else {
        framework.setOpflexIdentity(opflexName.get(),
                                    opflexDomain.get());
        policyManager.setOpflexDomain(opflexDomain.get());
    }

    bool enableInspector = properties.get<bool>(OPFLEX_INSPECTOR, true);
    std::string inspectorSock =
        properties.get<std::string>(OPFLEX_INSPECTOR, DEF_SOCKET);
    if (enableInspector)
        framework.enableInspector(inspectorSock);

    optional<const ptree&> peers = 
        properties.get_child_optional(OPFLEX_PEERS);
    if (peers) {
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
    if (opflexPeers.size() == 0)
        LOG(ERROR) << "No Opflex peers found in configuration";

    sslMode = properties.get<std::string>(OPFLEX_SSL_MODE, "disabled");
    sslCaStore = properties.get<std::string>(OPFLEX_SSL_CA_STORE,
                                             "/etc/ssl/certs/");
    if (sslMode != "disabled") {
        bool verifyPeers = sslMode != "encrypted";
        framework.enableSSL(sslCaStore, verifyPeers);
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
    if (renderers.size() == 0)
        LOG(ERROR) << "No renderers configured; no policy will be applied";
}

void Agent::start() {
    LOG(INFO) << "Starting OVS Agent";
    started = true;

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
    root->addGbpeVMUniverse();
    root->addObserverEpStatUniverse();
    mutator.commit();

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

    io_service_thread = new thread(bind(&io_service::run, ref(agent_io)));

    BOOST_FOREACH(const host_t& h, opflexPeers)
        framework.addPeer(h.first, h.second);
}

void Agent::stop() {
    if (!started) return;
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

    if (io_service_thread) {
        io_service_thread->join();
        delete io_service_thread;
    }

    started = false;
}

} /* namespace ovsagent */
