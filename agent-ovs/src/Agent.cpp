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
#include "FSServiceSource.h"
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
      endpointManager(framework, policyManager), notifServer(agent_io),
      io_service_thread(NULL), started(false) {

}

Agent::~Agent() {
    stop();
    BOOST_FOREACH(Renderer* r, renderers) {
        delete r;
    }
    renderers.clear();
}

#define DEF_INSPECT_SOCKET LOCALSTATEDIR"/run/opflex-agent-ovs-inspect.sock"
#define DEF_NOTIF_SOCKET LOCALSTATEDIR"/run/opflex-agent-ovs-notif.sock"

void Agent::setProperties(const boost::property_tree::ptree& properties) {
    static const std::string LOG_LEVEL("log.level");
    static const std::string ENDPOINT_SOURCE_PATH("endpoint-sources.filesystem");
    static const std::string SERVICE_SOURCE_PATH("service-sources.filesystem");
    static const std::string OPFLEX_PEERS("opflex.peers");
    static const std::string OPFLEX_SSL_MODE("opflex.ssl.mode");
    static const std::string OPFLEX_SSL_CA_STORE("opflex.ssl.ca-store");
    static const std::string HOSTNAME("hostname");
    static const std::string PORT("port");
    static const std::string OPFLEX_INSPECTOR("opflex.inspector.enabled");
    static const std::string OPFLEX_INSPECTOR_SOCK("opflex.inspector.socket-name");
    static const std::string OPFLEX_NOTIF("opflex.notif.enabled");
    static const std::string OPFLEX_NOTIF_SOCK("opflex.notif.socket-name");

    static const std::string OPFLEX_NAME("opflex.name");
    static const std::string OPFLEX_DOMAIN("opflex.domain");

    static const std::string RENDERERS_STITCHED_MODE("renderers.stitched-mode");

    optional<std::string> logLvl =
        properties.get_optional<std::string>(LOG_LEVEL);
    if (logLvl) {
        setLoggingLevel(logLvl.get());
    }

    boost::optional<std::string> ofName =
        properties.get_optional<std::string>(OPFLEX_NAME);
    if (ofName) opflexName = ofName;
    boost::optional<std::string> ofDomain =
        properties.get_optional<std::string>(OPFLEX_DOMAIN);
    if (ofDomain) opflexDomain = ofDomain;
    if (opflexDomain) LOG(INFO) << opflexDomain.get();

    boost::optional<bool> enabInspector =
        properties.get_optional<bool>(OPFLEX_INSPECTOR);
    boost::optional<std::string> inspSocket =
        properties.get_optional<std::string>(OPFLEX_INSPECTOR_SOCK);
    if (enabInspector) enableInspector = enabInspector;
    if (inspSocket) inspectorSock = inspSocket;

    boost::optional<bool> enabNotif =
        properties.get_optional<bool>(OPFLEX_NOTIF);
    boost::optional<std::string> notSocket =
        properties.get_optional<std::string>(OPFLEX_NOTIF_SOCK);
    if (enabNotif) enableNotif = enabNotif;
    if (notSocket) notifSock = notSocket;

    optional<const ptree&> endpointSource =
        properties.get_child_optional(ENDPOINT_SOURCE_PATH);

    if (endpointSource) {
        BOOST_FOREACH(const ptree::value_type &v, endpointSource.get())
            endpointSourcePaths.insert(v.second.data());
    }

    optional<const ptree&> serviceSource =
        properties.get_child_optional(SERVICE_SOURCE_PATH);

    if (serviceSource) {
        BOOST_FOREACH(const ptree::value_type &v, serviceSource.get())
            serviceSourcePaths.insert(v.second.data());
    }

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

    boost::optional<std::string> confSslMode =
        properties.get_optional<std::string>(OPFLEX_SSL_MODE);
    boost::optional<std::string> confsslCaStore =
        properties.get_optional<std::string>(OPFLEX_SSL_CA_STORE);

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

void Agent::applyProperties() {
    if (!opflexName || !opflexDomain) {
        LOG(ERROR) << "Opflex name and domain must be set";
        throw std::runtime_error("Opflex name and domain must be set");
    } else {
        framework.setOpflexIdentity(opflexName.get(),
                                    opflexDomain.get());
        policyManager.setOpflexDomain(opflexDomain.get());
    }

    if (endpointSourcePaths.size() == 0)
        LOG(ERROR) << "No endpoint sources found in configuration.";
    if (serviceSourcePaths.size() == 0)
        LOG(INFO) << "No service sources found in configuration.";
    if (opflexPeers.size() == 0)
        LOG(ERROR) << "No Opflex peers found in configuration";
    if (renderers.size() == 0)
        LOG(ERROR) << "No renderers configured; no policy will be applied";

    if (!enableInspector || enableInspector.get()) {
        if (!inspectorSock) inspectorSock = DEF_INSPECT_SOCKET;
        framework.enableInspector(inspectorSock.get());
    }
    if (!enableNotif || enableNotif.get()) {
        if (!notifSock) notifSock = DEF_NOTIF_SOCKET;
        notifServer.setSocketName(notifSock.get());
    }

    if (sslMode && sslMode.get() != "disabled") {
        if (!sslCaStore) sslCaStore = "/etc/ssl/certs/";
        bool verifyPeers = sslMode.get() != "encrypted";
        framework.enableSSL(sslCaStore.get(), verifyPeers);
    }
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
    notifServer.start();

    BOOST_FOREACH(Renderer* r, renderers) {
        r->start();
    }

    io_service_thread = new thread(bind(&io_service::run, ref(agent_io)));

    BOOST_FOREACH(const std::string& path, endpointSourcePaths) {
        EndpointSource* source =
            new FSEndpointSource(&endpointManager, fsWatcher, path);
        endpointSources.insert(source);
    }
    BOOST_FOREACH(const std::string& path, serviceSourcePaths) {
        ServiceSource* source =
            new FSServiceSource(&serviceManager, fsWatcher, path);
        serviceSources.insert(source);
    }
    fsWatcher.start();

    BOOST_FOREACH(const host_t& h, opflexPeers)
        framework.addPeer(h.first, h.second);
}

void Agent::stop() {
    if (!started) return;
    LOG(INFO) << "Stopping OVS Agent";

    BOOST_FOREACH(Renderer* r, renderers) {
        r->stop();
    }

    fsWatcher.stop();
    BOOST_FOREACH(EndpointSource* source, endpointSources) {
        delete source;
    }
    endpointSources.clear();

    BOOST_FOREACH(ServiceSource* source, serviceSources) {
        delete source;
    }
    serviceSources.clear();

    notifServer.stop();
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
