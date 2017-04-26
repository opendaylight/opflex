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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <boost/assign/list_of.hpp>
#include <modelgbp/dmtree/Root.hpp>

#include "cmd.h"
#include "Agent.h"
#include "FSEndpointSource.h"
#include "FSServiceSource.h"
#include "FSRDConfigSource.h"
#include "logging.h"

#include "Renderer.h"
#ifdef RENDERER_OVS
#include "StitchedModeRenderer.h"
#endif

#include <unordered_map>

namespace ovsagent {

using std::thread;
using std::make_pair;
using opflex::modb::ModelMetadata;
using opflex::modb::Mutator;
using opflex::ofcore::OFFramework;
using boost::property_tree::ptree;
using boost::optional;
using boost::asio::io_service;

Agent::Agent(OFFramework& framework_)
    : framework(framework_), policyManager(framework),
      endpointManager(framework, policyManager), notifServer(agent_io),
      started(false) {

}

Agent::~Agent() {
    stop();
    renderers.clear();
}

#define DEF_INSPECT_SOCKET LOCALSTATEDIR"/run/opflex-agent-ovs-inspect.sock"
#define DEF_NOTIF_SOCKET LOCALSTATEDIR"/run/opflex-agent-ovs-notif.sock"

Renderer* disabled_create(Agent& agent) {
    return NULL;
}

void Agent::setProperties(const boost::property_tree::ptree& properties) {
    static const std::string LOG_LEVEL("log.level");
    static const std::string ENDPOINT_SOURCE_PATH("endpoint-sources.filesystem");
    static const std::string SERVICE_SOURCE_PATH("service-sources.filesystem");
    static const std::string OPFLEX_PEERS("opflex.peers");
    static const std::string OPFLEX_SSL_MODE("opflex.ssl.mode");
    static const std::string OPFLEX_SSL_CA_STORE("opflex.ssl.ca-store");
    static const std::string OPFLEX_SSL_CERT_PATH("opflex.ssl.client-cert.path");
    static const std::string OPFLEX_SSL_CERT_PASS("opflex.ssl.client-cert.password");
    static const std::string HOSTNAME("hostname");
    static const std::string PORT("port");
    static const std::string OPFLEX_INSPECTOR("opflex.inspector.enabled");
    static const std::string OPFLEX_INSPECTOR_SOCK("opflex.inspector.socket-name");
    static const std::string OPFLEX_NOTIF("opflex.notif.enabled");
    static const std::string OPFLEX_NOTIF_SOCK("opflex.notif.socket-name");
    static const std::string OPFLEX_NOTIF_OWNER("opflex.notif.socket-owner");
    static const std::string OPFLEX_NOTIF_GROUP("opflex.notif.socket-group");
    static const std::string OPFLEX_NOTIF_PERMS("opflex.notif.socket-permissions");

    static const std::string OPFLEX_NAME("opflex.name");
    static const std::string OPFLEX_DOMAIN("opflex.domain");

    static const std::string RENDERERS_STITCHED_MODE("renderers.stitched-mode");
    static const std::string RENDERERS_OPENVSWITCH("renderers.openvswitch");

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
    boost::optional<std::string> notOwner =
        properties.get_optional<std::string>(OPFLEX_NOTIF_OWNER);
    boost::optional<std::string> notGrp =
        properties.get_optional<std::string>(OPFLEX_NOTIF_GROUP);
    boost::optional<std::string> notPerms =
        properties.get_optional<std::string>(OPFLEX_NOTIF_PERMS);
    if (enabNotif) enableNotif = enabNotif;
    if (notSocket) notifSock = notSocket;
    if (notOwner) notifOwner = notOwner;
    if (notGrp) notifGroup = notGrp;
    if (notPerms) notifPerms = notPerms;

    optional<const ptree&> endpointSource =
        properties.get_child_optional(ENDPOINT_SOURCE_PATH);

    if (endpointSource) {
        for (const ptree::value_type &v : endpointSource.get())
            endpointSourcePaths.insert(v.second.data());
    }

    optional<const ptree&> serviceSource =
        properties.get_child_optional(SERVICE_SOURCE_PATH);

    if (serviceSource) {
        for (const ptree::value_type &v : serviceSource.get())
            serviceSourcePaths.insert(v.second.data());
    }

    optional<const ptree&> peers =
        properties.get_child_optional(OPFLEX_PEERS);
    if (peers) {
        for (const ptree::value_type &v : peers.get()) {
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
    boost::optional<std::string> confsslClientCert =
        properties.get_optional<std::string>(OPFLEX_SSL_CERT_PATH);
    boost::optional<std::string> confsslClientCertPass =
        properties.get_optional<std::string>(OPFLEX_SSL_CERT_PASS);
    if (confSslMode)
        sslMode = confSslMode;
    if (confsslCaStore)
        sslCaStore = confsslCaStore;
    if (confsslClientCert)
        sslClientCert = confsslClientCert;
    if (confsslClientCertPass)
        sslClientCertPass = confsslClientCertPass;

    typedef Renderer* (*rend_create)(Agent&);
    typedef std::unordered_map<std::string, rend_create> rend_map_t;
    static rend_map_t rend_map =
        boost::assign::map_list_of
#ifdef RENDERER_OVS
        (RENDERERS_STITCHED_MODE, StitchedModeRenderer::create)
        (RENDERERS_OPENVSWITCH, StitchedModeRenderer::create)
#else
        (RENDERERS_STITCHED_MODE, disabled_create)
        (RENDERERS_OPENVSWITCH, disabled_create)
#endif
        ;

    for (rend_map_t::value_type& v : rend_map) {
        optional<const ptree&> rtree =
            properties.get_child_optional(v.first);
        if (rtree) {
            auto it = renderers.find(v.first);
            Renderer* r;
            if (it == renderers.end()) {
                std::unique_ptr<Renderer> rp(v.second(*this));
                r = rp.get();
                if (r == NULL) {
                    LOG(ERROR) << "Renderer type " << v.first
                               << " is not enabled";
                    continue;
                }
                renderers.emplace(v.first, std::move(rp));
            } else {
                r = it->second.get();
            }
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
        if (notifOwner)
            notifServer.setSocketOwner(notifOwner.get());
        if (notifGroup)
            notifServer.setSocketGroup(notifGroup.get());
        if (notifPerms)
            notifServer.setSocketPerms(notifPerms.get());
    }

    if (sslMode && sslMode.get() != "disabled") {
        if (!sslCaStore) sslCaStore = "/etc/ssl/certs/";
        bool verifyPeers = sslMode.get() != "encrypted";

        if (sslClientCert) {
            framework
                .enableSSL(sslCaStore.get(),
                           sslClientCert.get(),
                           sslClientCertPass ? sslClientCertPass.get() : "",
                           verifyPeers);
        } else {
            framework.enableSSL(sslCaStore.get(), verifyPeers);
        }
    }
}

void Agent::start() {
    LOG(INFO) << "Starting OVS Agent";
    started = true;

    // instantiate the opflex framework
    framework.setModel(modelgbp::getMetadata());
    framework.start();

    Mutator mutator(framework, "init");
    std::shared_ptr<modelgbp::dmtree::Root> root =
        modelgbp::dmtree::Root::createRootElement(framework);
    root->addPolicyUniverse();
    root->addRelatorUniverse();
    root->addEprL2Universe();
    root->addEprL3Universe();
    root->addEpdrL2Discovered();
    root->addEpdrL3Discovered();
    root->addGbpeVMUniverse();
    root->addObserverEpStatUniverse();
    root->addObserverPolicyStatUniverse();
    mutator.commit();

    // instantiate other components
    policyManager.start();
    endpointManager.start();
    notifServer.start();

    for (auto& r : renderers) {
        r.second->start();
    }

    io_service_thread.reset(new thread([this]() { agent_io.run(); }));

    for (const std::string& path : endpointSourcePaths) {
        {
            EndpointSource* source =
                new FSEndpointSource(&endpointManager, fsWatcher, path);
            endpointSources.emplace_back(source);
        }
        {
            FSRDConfigSource* source =
                new FSRDConfigSource(&extraConfigManager, fsWatcher, path);
            rdConfigSources.emplace_back(source);
        }
    }
    for (const std::string& path : serviceSourcePaths) {
        ServiceSource* source =
            new FSServiceSource(&serviceManager, fsWatcher, path);
        serviceSources.emplace_back(source);
    }
    fsWatcher.start();

    for (const host_t& h : opflexPeers)
        framework.addPeer(h.first, h.second);
}

void Agent::stop() {
    if (!started) return;
    LOG(INFO) << "Stopping OVS Agent";

    for (auto& r : renderers) {
        r.second->stop();
    }

    fsWatcher.stop();

    notifServer.stop();
    endpointManager.stop();
    policyManager.stop();
    framework.stop();

    if (io_service_thread) {
        io_service_thread->join();
        io_service_thread.reset();
    }

    endpointSources.clear();
    rdConfigSources.clear();
    serviceSources.clear();

    started = false;
}

} /* namespace ovsagent */
