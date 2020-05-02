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
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <modelgbp/dmtree/Root.hpp>

#include <opflexagent/cmd.h>
#include <opflexagent/Agent.h>
#include <opflexagent/FSEndpointSource.h>
#include <opflexagent/ModelEndpointSource.h>
#include <opflexagent/FSServiceSource.h>
#include <opflexagent/FSRDConfigSource.h>
#include <opflexagent/FSLearningBridgeSource.h>
#include <opflexagent/FSExternalEndpointSource.h>
#include <opflexagent/FSSnatSource.h>
#include <opflexagent/FSPacketDropLogConfigSource.h>
#include <opflexagent/logging.h>

#include <opflexagent/Renderer.h>
#include <opflexagent/SimStats.h>

#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>

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
using boost::uuids::to_string;
using boost::uuids::basic_random_generator;

#ifdef HAVE_PROMETHEUS_SUPPORT
Agent::Agent(OFFramework& framework_, const LogParams& _logParams)
    : framework(framework_),
      prometheusManager(*this, framework),
      policyManager(framework, agent_io),
      endpointManager(*this, framework, policyManager, prometheusManager),
      serviceManager(*this, framework, prometheusManager),
      extraConfigManager(framework),
      notifServer(agent_io),rendererFwdMode(opflex_elem_t::INVALID_MODE),
      started(false), presetFwdMode(opflex_elem_t::INVALID_MODE),
      spanManager(framework, agent_io),
      netflowManager(framework,agent_io),
      prometheusEnabled(true),
      prometheusExposeLocalHostOnly(false),
      prometheusExposeEpSvcNan(false),
      behaviorL34FlowsWithoutSubnet(false),
      logParams(_logParams) {
#else
Agent::Agent(OFFramework& framework_, const LogParams& _logParams)
    : framework(framework_),
      policyManager(framework, agent_io),
      endpointManager(*this, framework, policyManager),
      serviceManager(*this, framework),
      extraConfigManager(framework),
      notifServer(agent_io),rendererFwdMode(opflex_elem_t::INVALID_MODE),
      started(false), presetFwdMode(opflex_elem_t::INVALID_MODE),
      contractInterval(0), securityGroupInterval(0), interfaceInterval(0),
      spanManager(framework, agent_io),
      netflowManager(framework,agent_io),
      behaviorL34FlowsWithoutSubnet(false),
      logParams(_logParams) {
#endif
    std::random_device rng;
    std::mt19937 urng(rng());
    uuid = to_string(basic_random_generator<std::mt19937>(urng)());
}

Agent::~Agent() {
    stop();
    renderers.clear();
    for (auto handle : rendPluginHandles) {
        dlclose(handle);
    }
}

#define DEF_INSPECT_SOCKET LOCALSTATEDIR"/run/opflex-agent-inspect.sock"
#define DEF_NOTIF_SOCKET LOCALSTATEDIR"/run/opflex-agent-notif.sock"

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
#ifdef HAVE_PROMETHEUS_SUPPORT
    static const std::string PROMETHEUS_ENABLED("prometheus.enabled");
    static const std::string PROMETHEUS_LOCALHOST_ONLY("prometheus.localhost-only");
    static const std::string PROMETHEUS_EXPOSE_EPSVC_NAN("prometheus.expose-epsvc-nan");
    static const std::string PROMETHEUS_EP_ATTRIBUTES("prometheus.ep-attributes");
#endif
    static const std::string ENDPOINT_SOURCE_FSPATH("endpoint-sources.filesystem");
    static const std::string ENDPOINT_SOURCE_MODEL_LOCAL("endpoint-sources.model-local");
    static const std::string SERVICE_SOURCE_PATH("service-sources.filesystem");
    static const std::string SNAT_SOURCE_PATH("snat-sources.filesystem");
    static const std::string DROP_LOG_CFG_SOURCE_FSPATH("drop-log-config-sources.filesystem");
    static const std::string PACKET_EVENT_NOTIF_SOCK("packet-event-notif.socket-name");
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
    static const std::string RENDERERS_TRANSPORT_MODE("renderers.transport-mode");
    static const std::string RENDERERS_OPENVSWITCH("renderers.openvswitch");
    static const std::string OPFLEX_STATS("opflex.statistics");
    static const std::string OPFLEX_STATS_MODE("opflex.statistics.mode");
    static const std::string OPFLEX_STATS_INTERFACE_SETTING("opflex.statistics.interface.enabled");
    static const std::string OPFLEX_STATS_INTERFACE_INTERVAL("opflex.statistics.interface.interval");
    static const std::string OPFLEX_STATS_CONTRACT_SETTING("opflex.statistics.contract.enabled");
    static const std::string OPFLEX_STATS_CONTRACT_INTERVAL("opflex.statistics.contract.interval");
    static const std::string OPFLEX_STATS_SECGRP_SETTING("opflex.statistics.security-group.enabled");
    static const std::string OPFLEX_STATS_SECGRP_INTERVAL("opflex.statistics.security-group.interval");
    static const std::string OPFLEX_PRR_INTERVAL("opflex.timers.prr");
    static const std::string OPFLEX_HANDSHAKE("opflex.timers.handshake-timeout");
    static const std::string DISABLED_FEATURES("feature.disabled");
    static const std::string BEHAVIOR_L34FLOWS_WITHOUT_SUBNET("behavior.l34flows-without-subnet");

    // set feature flags to true
    clearFeatureFlags();

    optional<std::string> logLvl =
        properties.get_optional<std::string>(LOG_LEVEL);
    if (logLvl) {
        setLoggingLevel(logLvl.get());
        std::string level_str,log_file;
        bool toSyslog;
        std::tie(level_str, toSyslog, log_file) = logParams;
        level_str = getLogLevelString();
        logParams = std::make_tuple(level_str, toSyslog, log_file);
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
    boost::optional<const ptree&> statChild = properties.get_child_optional(OPFLEX_STATS);
    boost::optional<std::string> statMode_json;
    if (statChild)
        statMode_json = properties.get_optional<std::string>(OPFLEX_STATS_MODE);

    if (enabNotif) enableNotif = enabNotif;
    if (notSocket) notifSock = notSocket;
    if (notOwner) notifOwner = notOwner;
    if (notGrp) notifGroup = notGrp;
    if (notPerms) notifPerms = notPerms;
    if (statMode_json) {
        statMode = getStatModeFromString(statMode_json.get());
    }

    optional<const ptree&> disabledFeatures =
            properties.get_child_optional(DISABLED_FEATURES);
    if (disabledFeatures) {
        for (const ptree::value_type &v : disabledFeatures.get())
            disabledFeaturesSet.insert(v.second.data());
    }

    optional<bool> behaviorAddL34FlowsWithoutSubnet =
        properties.get_optional<bool>(BEHAVIOR_L34FLOWS_WITHOUT_SUBNET);
    if (behaviorAddL34FlowsWithoutSubnet) {
        behaviorL34FlowsWithoutSubnet = behaviorAddL34FlowsWithoutSubnet.get();
    }

#ifdef HAVE_PROMETHEUS_SUPPORT
    boost::optional<bool> prometheusIsEnabled =
                properties.get_optional<bool>(PROMETHEUS_ENABLED);
    if (prometheusIsEnabled) {
        if (prometheusIsEnabled.get() == false)
            prometheusEnabled = false;
    }

    boost::optional<bool> prometheusLocalHostOnly =
                properties.get_optional<bool>(PROMETHEUS_LOCALHOST_ONLY);
    if (prometheusLocalHostOnly) {
        if (prometheusLocalHostOnly.get() == true)
            prometheusExposeLocalHostOnly = true;
    }

    boost::optional<bool> prometheusEpSvcNan =
                properties.get_optional<bool>(PROMETHEUS_EXPOSE_EPSVC_NAN);
    if (prometheusEpSvcNan) {
        if (prometheusEpSvcNan.get() == true)
            prometheusExposeEpSvcNan = true;
    }

    optional<const ptree&> epAttributes =
        properties.get_child_optional(PROMETHEUS_EP_ATTRIBUTES);
    if (epAttributes) {
        for (const ptree::value_type &v : epAttributes.get())
            prometheusEpAttributes.insert(v.second.data());
    }
#endif

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

    optional<const ptree&> snatSource =
        properties.get_child_optional(SNAT_SOURCE_PATH);

    if (snatSource) {
        for (const ptree::value_type &v : snatSource.get())
             snatSourcePaths.insert(v.second.data());
    }

    optional<const ptree&> dropLogCfgSrc =
        properties.get_child_optional(DROP_LOG_CFG_SOURCE_FSPATH);

    if (dropLogCfgSrc) {
        for (const ptree::value_type &v : dropLogCfgSrc.get())
        dropLogCfgSourcePath = v.second.data();
    }

    optional<const ptree&> packetEventNotifSock =
        properties.get_child_optional(PACKET_EVENT_NOTIF_SOCK);

    if (packetEventNotifSock) {
        for (const ptree::value_type &v : packetEventNotifSock.get())
            packetEventNotifSockPath = v.second.data();
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
        properties.get_child_optional(RENDERERS_STITCHED_MODE) ||
        properties.get_child_optional(RENDERERS_TRANSPORT_MODE)) {
        // Special case for backward compatibility: if config attempts
        // to create an openvswitch renderer, load the plugin
        // automatically.
        loadPlugin("libopflex_agent_renderer_openvswitch.so");
    }

    // Following two blocks of code ensure that
    // In the absence of a mode config: default mode, stitched-mode is chosen
    // In the presence of a mode config: the last conf file mode setting
    // overrides the current setting

    bool modeConfigPresent = false;
    if(properties.get_child_optional(RENDERERS_STITCHED_MODE) ||
       properties.get_child_optional(RENDERERS_TRANSPORT_MODE)) {
        modeConfigPresent = true;
    }

    if(this->rendererFwdMode == opflex::ofcore::OFConstants::INVALID_MODE ||
       modeConfigPresent) {
        if(this->presetFwdMode != opflex::ofcore::OFConstants::INVALID_MODE) {
            this->rendererFwdMode = this->presetFwdMode;
        } else if(properties.get_child_optional(RENDERERS_TRANSPORT_MODE)) {
            this->rendererFwdMode = opflex::ofcore::OFConstants::TRANSPORT_MODE;
        } else {
            this->rendererFwdMode = opflex::ofcore::OFConstants::STITCHED_MODE;
        }
    }

    optional<const ptree&> rendConfig =
        properties.get_child_optional(RENDERERS);
    if (rendConfig) {
        for (rend_map_t::value_type& v : rendPlugins) {
            optional<const ptree&> rtree =
                rendConfig.get().get_child_optional(v.first);
            if (!rtree) continue;

            ptree rtree_cp = rtree.get();
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
            if (statMode == StatMode::REAL && statChild) {
                rtree_cp.add_child("statistics", statChild.get() );
            }
            r->setProperties(rtree_cp);
        }
    }

    if (statMode == StatMode::SIM) {
        LOG(INFO) << "Simulation of stats enabled";
        Agent::StatProps statProps{};
        statProps.interval = 30;
        setSimStatProperties(OPFLEX_STATS_INTERFACE_SETTING, OPFLEX_STATS_INTERFACE_INTERVAL,
                             properties, statProps);
        if (statProps.enabled)
            interfaceInterval = statProps.interval*1000;
        statProps.interval = 10;
        setSimStatProperties(OPFLEX_STATS_CONTRACT_SETTING, OPFLEX_STATS_CONTRACT_INTERVAL,
                             properties, statProps);
        if (statProps.enabled)
            contractInterval = statProps.interval*1000;
        statProps.interval = 10;
        setSimStatProperties(OPFLEX_STATS_SECGRP_SETTING, OPFLEX_STATS_SECGRP_INTERVAL,
                             properties, statProps);
        if (statProps.enabled)
            securityGroupInterval = statProps.interval*1000;

        LOG(INFO) << "contract interval set to " << contractInterval << " millisecs";
        LOG(INFO) << "security group interval set to " << securityGroupInterval << " millisecs";
        LOG(INFO) << "interface interval set to " << interfaceInterval << " millisecs";
    }
   
    boost::optional<boost::uint_t<64>::fast> prr_timer_present = 
        properties.get_optional<boost::uint_t<64>::fast>(OPFLEX_PRR_INTERVAL);
    if (prr_timer_present) { 
        prr_timer = prr_timer_present.get();
        if (prr_timer < 15) {
           prr_timer = 15;  /* min is 15 seconds */
        }
        LOG(INFO) << "prr timer set to " << prr_timer << " secs";
    }

    boost::optional<uint32_t> handshakeOpt = properties.get_optional<uint32_t>(OPFLEX_HANDSHAKE);
    if (handshakeOpt) {
        peerHandshakeTimeout = handshakeOpt.get();
        LOG(INFO) << "peer handshake timeout set to " << peerHandshakeTimeout << " ms";
    }

    LOG(INFO) << "Agent mode set to " <<
       ((this->rendererFwdMode == opflex::ofcore::OFConstants::TRANSPORT_MODE)?
        "transport-mode" : "stitched-mode");

    if (disabledFeaturesSet.find("erspan") != disabledFeaturesSet.end()) {
        LOG(DEBUG) << "ERSPAN feature disabled";
        featureFlag[FeatureList::ERSPAN] = false;
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

    if (endpointSourceFSPaths.empty() &&
        endpointSourceModelLocalNames.empty())
        LOG(ERROR) << "No endpoint sources found in configuration.";
    if (serviceSourcePaths.empty())
        LOG(INFO) << "No service sources found in configuration.";
    if (snatSourcePaths.empty())
        LOG(INFO) << "No SNAT sources found in configuration.";
    if (opflexPeers.empty())
        LOG(ERROR) << "No Opflex peers found in configuration";
    if (renderers.empty())
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
     
    framework.setPrrTimerDuration(prr_timer);
    framework.setHandshakeTimeout(peerHandshakeTimeout);
}

void Agent::start() {
    LOG(INFO) << "Starting OpFlex Agent " << uuid;
    started = true;

    // instantiate the opflex framework
    framework.setModel(modelgbp::getMetadata());
    framework.setElementMode(this->rendererFwdMode);
    framework.start();

    Mutator mutator(framework, "init");
    std::shared_ptr<modelgbp::dmtree::Root> root =
        modelgbp::dmtree::Root::createRootElement(framework);
    root->addPolicyUniverse();
    root->addRelatorUniverse();
    root->addSvcServiceUniverse();
    root->addEprL2Universe();
    root->addEprL3Universe();
    root->addInvUniverse();
    root->addEpdrL2Discovered();
    root->addEpdrL3Discovered();
    root->addGbpeVMUniverse();
    root->addObserverEpStatUniverse();
    root->addObserverSvcStatUniverse();
    root->addObserverPolicyStatUniverse();
    root->addObserverDropFlowConfigUniverse();
    root->addSpanUniverse();
    root->addEpdrExternalDiscovered();
    root->addEpdrLocalRouteDiscovered();
    root->addEprPeerRouteUniverse();
    root->addFaultUniverse();
    root->addObserverSysStatUniverse();
    mutator.commit();

    // instantiate other components
#ifdef HAVE_PROMETHEUS_SUPPORT
    if (prometheusEnabled) {
        prometheusManager.start(prometheusExposeLocalHostOnly,
                                prometheusExposeEpSvcNan);
    } else {
        LOG(DEBUG) << "prometheus not enabled";
    }
#endif
    policyManager.start();
    endpointManager.start();
    notifServer.start();
    if (isFeatureEnabled(FeatureList::ERSPAN))
        spanManager.start();
    netflowManager.start();
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
        {
            LearningBridgeSource* source =
                new FSLearningBridgeSource(&learningBridgeManager,
                                           fsWatcher, path);
            learningBridgeSources.emplace_back(source);
        }
        {
            EndpointSource* source =
            new FSExternalEndpointSource(&endpointManager, fsWatcher, path);
            endpointSources.emplace_back(source);
        }
    }
    if (!endpointSourceModelLocalNames.empty()) {
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
    for (const std::string& path : snatSourcePaths) {
        SnatSource* source =
            new FSSnatSource(&snatManager, fsWatcher, path);
        snatSources.emplace_back(source);
    }
    if(!dropLogCfgSourcePath.empty()) {
        opflex::modb::URI uri = (opflex::modb::URIBuilder()
                .addElement("PolicyUniverse").addElement("ObserverDropLogConfig")
                .build());
        dropLogCfgSource.reset(new FSPacketDropLogConfigSource(&extraConfigManager,
                        fsWatcher, dropLogCfgSourcePath, uri));
    }
    fsWatcher.start();

    for (const host_t& h : opflexPeers)
        framework.addPeer(h.first, h.second);


    if (statMode == StatMode::SIM) {
        pSimStats = std::unique_ptr<SimStats>(new SimStats(*this));
        pSimStats->start();
        policyManager.registerListener(&(*pSimStats));
    }

    if (statMode == StatMode::OFF) {
        LOG(INFO) << "Disable stats reporting completely";
        framework.disableObservableReporting();
    } else {
        // disable reporting of some stats for now (MODB only)
        LOG(INFO) << "Disable unsupported stat reporting";
        framework.overrideObservableReporting(modelgbp::observer::OpflexCounter::CLASS_ID, false);
        framework.overrideObservableReporting(modelgbp::gbpe::EpToSvcCounter::CLASS_ID, false);
        framework.overrideObservableReporting(modelgbp::gbpe::SvcToEpCounter::CLASS_ID, false);
        framework.overrideObservableReporting(modelgbp::gbpe::TableDropCounter::CLASS_ID, false);
        framework.overrideObservableReporting(modelgbp::gbpe::SvcCounter::CLASS_ID, false);
        framework.overrideObservableReporting(modelgbp::gbpe::SvcTargetCounter::CLASS_ID, false);
    }
}

void Agent::stop() {
    if (!started) return;
    LOG(INFO) << "Stopping OpFlex Agent";

    // if stats simulation is enbaled, stop it.
    if (statMode == StatMode::SIM) {
        pSimStats->stop();
    }

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
    if (isFeatureEnabled(FeatureList::ERSPAN))
        spanManager.stop();
    netflowManager.stop();
#ifdef HAVE_PROMETHEUS_SUPPORT
    prometheusManager.stop();
    LOG(DEBUG) << "Prometheus Manager stopped";
#endif

    if (io_work) {
        io_work.reset();
    }
    if (io_service_thread) {
        io_service_thread->join();
        io_service_thread.reset();
	LOG(DEBUG) << "IO service thread stopped";
    }

    framework.stop();
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

inline StatMode Agent::getStatModeFromString(const std::string& mode) {
    if (mode == "simulate")
        return StatMode::SIM;
    else if (mode == "off")
        return StatMode::OFF;
    else
        return StatMode::REAL;
}

inline void Agent::setSimStatProperties(const std::string& enabled_prop,
                                 const std::string& interval_prop,
                                 const ptree& properties, Agent::StatProps& props) {
    boost::optional<bool> enabled = properties.get_optional<bool>(enabled_prop);
    if (enabled) {
        if (enabled.get() == true) {
            boost::optional<long> interval =
                   properties.get_optional<long>(interval_prop);
            props.enabled = true;
            if (interval) {
                if (interval.get() < 10)
                   props.interval = 10*1000;
                else
                   props.interval = interval.get();
            }
        } else {
           props.enabled = false;
        }
    } else {
           props.enabled = false;
    }
}

void Agent::setUplinkMac(const std::string &mac) {
    LOG(DEBUG) << "Got TunnelEp MAC " << mac;
    opflex::modb::MAC _mac = opflex::modb::MAC(mac);
    framework.setTunnelMac(_mac);
    if(rendererFwdMode != opflex::ofcore::OFConstants::TRANSPORT_MODE) {
        return;
    }
    for (const host_t& h : opflexPeers)
        framework.addPeer(h.first, h.second);

}

void Agent::clearFeatureFlags() {
    for (int i=0 ; i < FeatureList::MAX; i++) {
        featureFlag[i] = true;
    }
}

} /* namespace opflexagent */
