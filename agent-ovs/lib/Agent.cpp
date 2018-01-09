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
#include <boost/algorithm/string/join.hpp>
#include <modelgbp/dmtree/Root.hpp>

#include <opflexagent/cmd.h>
#include <opflexagent/Agent.h>
#include <opflexagent/FSEndpointSource.h>
#include <opflexagent/ModelEndpointSource.h>
#include <opflexagent/FSServiceSource.h>
#include <opflexagent/FSRDConfigSource.h>
#include <opflexagent/logging.h>

#include <opflexagent/Renderer.h>

#include <unordered_map>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include <dlfcn.h>

namespace opflexagent {

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
    for (auto handle : rendPluginHandles) {
        dlclose(handle);
    }
}

#define DEF_INSPECT_SOCKET LOCALSTATEDIR"/run/opflex-agent-ovs-inspect.sock"
#define DEF_NOTIF_SOCKET LOCALSTATEDIR"/run/opflex-agent-ovs-notif.sock"

Renderer* disabled_create(Agent& agent) {
    return NULL;
}

void Agent::loadPlugin(const std::string& name) {
    if (rendPluginLibs.find(name) != rendPluginLibs.end())
        return;

    void* handle = dlopen(name.c_str(), RTLD_NOW);
    if (handle == NULL) {
        LOG(ERROR) << "Failed to load renderer plugin "
                   << "\"" << name << "\":" << dlerror();
        return;
    }
    auto init =
        (renderer_init_func)(dlsym(handle, "init_renderer_plugin"));
    if (init == NULL) {
        LOG(ERROR) << "Could not load renderer plugin "
                   << "\"" << name << "\" init function symbol:"
                   << dlerror();
        dlclose(handle);
        return;
    }
    const RendererPlugin* plugin = init();
    if (plugin == NULL) {
        LOG(ERROR) << "Renderer plugin "
                   << "\"" << name << "\" init function returned NULL:"
                   << dlerror();
        dlclose(handle);
        return;
    }
    rendPluginLibs.insert(name);
    rendPluginHandles.insert(handle);

    auto names = plugin->getNames();
    for (const auto& name : names) {
        rendPlugins.emplace(name, plugin);
    }

    LOG(INFO) << "Loaded renderer plugin "
              << "\"" << name << "\" providing renderers: "
              << boost::algorithm::join(names, ",");
}

void Agent::setProperties(const boost::property_tree::ptree& properties) {
    static const std::string LOG_LEVEL("log.level");
    static const std::string ENDPOINT_SOURCE_FSPATH("endpoint-sources.filesystem");
    static const std::string ENDPOINT_SOURCE_MODEL_LOCAL("endpoint-sources.model-local");
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

    static const std::string PLUGINS_RENDERER("plugins.renderer");
    static const std::string RENDERERS("renderers");
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

    optional<const ptree&> fsEndpointSource =
        properties.get_child_optional(ENDPOINT_SOURCE_FSPATH);

    if (fsEndpointSource) {
        for (const ptree::value_type &v : fsEndpointSource.get())
            endpointSourceFSPaths.insert(v.second.data());
    }

    optional<const ptree&> modelLocalEndpointSource =
        properties.get_child_optional(ENDPOINT_SOURCE_MODEL_LOCAL);

    if (modelLocalEndpointSource) {
        for (const ptree::value_type &v : modelLocalEndpointSource.get())
            endpointSourceModelLocalNames.insert(v.second.data());
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

    optional<const ptree&> rendererPlugins =
        properties.get_child_optional(PLUGINS_RENDERER);

    if (rendererPlugins) {
        for (const ptree::value_type &v : rendererPlugins.get()) {
            loadPlugin(v.second.data());
        }
    }

    if (properties.get_child_optional(RENDERERS_OPENVSWITCH) ||
        properties.get_child_optional(RENDERERS_STITCHED_MODE)) {
        // Special case for backward compatibility: if config attempts
        // to create an openvswitch renderer, load the plugin
        // automatically.
        loadPlugin("libopflex_agent_renderer_openvswitch.so");
    }

    optional<const ptree&> rendConfig =
        properties.get_child_optional(RENDERERS);
    if (rendConfig) {
        for (rend_map_t::value_type& v : rendPlugins) {
            optional<const ptree&> rtree =
                rendConfig.get().get_child_optional(v.first);
            if (!rtree) continue;

            auto it = renderers.find(v.first);
            Renderer* r;
            if (it == renderers.end()) {
                std::unique_ptr<Renderer> rp(v.second->create(*this));
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

    if (endpointSourceFSPaths.size() == 0 &&
        endpointSourceModelLocalNames.size() == 0)
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
    LOG(INFO) << "Starting OpFlex Agent";
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
    root->addInvUniverse();
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

    io_work.reset(new io_service::work(agent_io));
    io_service_thread.reset(new thread([this]() { agent_io.run(); }));

    for (const std::string& path : endpointSourceFSPaths) {
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
    if (endpointSourceModelLocalNames.size() > 0) {
        EndpointSource* source =
                new ModelEndpointSource(&endpointManager, framework,
                                        endpointSourceModelLocalNames);
        endpointSources.emplace_back(source);
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

    // Just in case the io_service gets blocked by some stray
    // events that don't get cleared, abort the process after a
    // timeout
    std::mutex mutex;
    std::condition_variable terminate;
    bool terminated = false;

    std::thread abort_timer([&mutex, &terminate, &terminated]() {
            std::unique_lock<std::mutex> guard(mutex);
            bool completed =
                terminate.wait_until(guard,
                                     std::chrono::steady_clock::now() +
                                     std::chrono::seconds(10),
                                     [&terminated]() {
                                         return terminated;
                                     });
            if (!completed) {
                LOG(ERROR) << "Failed to cleanly shut down Agent: "
                           << "Aborting";
                std::abort();
            }
        });

    for (auto& r : renderers) {
        r.second->stop();
    }

    fsWatcher.stop();

    notifServer.stop();
    endpointManager.stop();
    policyManager.stop();
    framework.stop();

    if (io_work) {
        io_work.reset();
    }
    if (io_service_thread) {
        io_service_thread->join();
        io_service_thread.reset();
    }

    endpointSources.clear();
    rdConfigSources.clear();
    serviceSources.clear();

    started = false;

    {
        std::lock_guard<std::mutex> guard(mutex);
        terminated = true;
    }
    terminate.notify_all();
    abort_timer.join();

    LOG(INFO) << "Agent stopped";
}

} /* namespace opflexagent */
