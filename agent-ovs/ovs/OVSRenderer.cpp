/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OVSRenderer class
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "OVSRenderer.h"
#include <opflexagent/logging.h>

#include <boost/asio/placeholders.hpp>
#include <boost/algorithm/string/split.hpp>
#include <openvswitch/vlog.h>
#include <fstream>
#include <ctime>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <tuple>

namespace opflexagent {

using std::bind;
using opflex::ofcore::OFFramework;
using boost::property_tree::ptree;
using boost::asio::deadline_timer;
using boost::asio::placeholders::error;

static const std::string ID_NMSPC_CONNTRACK("conntrack");
static const boost::posix_time::milliseconds CLEANUP_INTERVAL(3*60*1000);

#define PACKET_LOGGER_PIDDIR LOCALSTATEDIR"/lib/opflex-agent-ovs/pids"
#define LOOPBACK "127.0.0.1"

OVSRendererPlugin::OVSRendererPlugin() {
    /* No good way to redirect OVS logs to our logs, suppress them for now */
    vlog_set_levels(NULL, VLF_ANY_DESTINATION, VLL_OFF);
}

std::unordered_set<std::string> OVSRendererPlugin::getNames() const {
    return {"stitched-mode", "openvswitch"};
}

Renderer* OVSRendererPlugin::create(Agent& agent) const {
    return new OVSRenderer(agent);
}

OVSRenderer::OVSRenderer(Agent& agent_)
    : Renderer(agent_), ctZoneManager(idGen),
      intSwitchManager(agent_, intFlowExecutor, intFlowReader,
                       intPortMapper),
      tunnelEpManager(&agent_),
      intFlowManager(agent_, intSwitchManager, idGen,
                     ctZoneManager, pktInHandler, tunnelEpManager),
      accessSwitchManager(agent_, accessFlowExecutor,
                          accessFlowReader, accessPortMapper),
      accessFlowManager(agent_, accessSwitchManager, idGen, ctZoneManager),
      pktInHandler(agent_, intFlowManager),
      interfaceStatsManager(&agent_, intSwitchManager.getPortMapper(),
                            accessSwitchManager.getPortMapper()),
      contractStatsManager(&agent_, idGen, intSwitchManager),
      serviceStatsManager(&agent_, idGen, intSwitchManager,
                           intFlowManager),
      secGrpStatsManager(&agent_, idGen, accessSwitchManager),
      tableDropStatsManager(&agent_, idGen, intSwitchManager,
              accessSwitchManager),
      encapType(IntFlowManager::ENCAP_NONE),
      tunnelRemotePort(0), uplinkVlan(0),
      virtualRouter(true), routerAdv(true),
      endpointAdvMode(AdvertManager::EPADV_GRATUITOUS_BROADCAST),
      tunnelEndpointAdvMode(AdvertManager::EPADV_RARP_BROADCAST),
      tunnelEndpointAdvIntvl(300),
      virtualDHCP(true), connTrack(true), ctZoneRangeStart(0),
      ctZoneRangeEnd(0), ifaceStatsEnabled(true), ifaceStatsInterval(0),
      contractStatsEnabled(true), contractStatsInterval(0),
      serviceStatsFlowDisabled(false), serviceStatsEnabled(true), serviceStatsInterval(0),
      secGroupStatsEnabled(true), secGroupStatsInterval(0),
      tableDropStatsEnabled(true), tableDropStatsInterval(0),
      spanRenderer(agent_), netflowRenderer(agent_), started(false),
      dropLogRemotePort(6081), dropLogLocalPort(50000), pktLogger(pktLoggerIO, exporterIO) {

}

OVSRenderer::~OVSRenderer() {

}

void OVSRenderer::start() {
    if (started) return;

    if (intBridgeName == "") {
        LOG(ERROR) << "OVS integration bridge name not set";
        return;
    }

    if (encapType == IntFlowManager::ENCAP_NONE)
        LOG(WARNING)
            << "No encapsulation type specified; only local traffic will work";
    if (flowIdCache == "")
        LOG(WARNING) << "No flow ID cache directory specified";
    if (mcastGroupFile == "")
        LOG(WARNING) << "No multicast group file specified";

    started = true;
    LOG(INFO) << "Starting stitched-mode renderer using"
              << " integration bridge " << intBridgeName
              << " and access bridge "
              << (accessBridgeName == "" ? "[none]" : accessBridgeName);

    if (encapType == IntFlowManager::ENCAP_VXLAN ||
        encapType == IntFlowManager::ENCAP_IVXLAN) {
        tunnelEpManager.setUplinkIface(uplinkIface);
        tunnelEpManager.setUplinkVlan(uplinkVlan);
        tunnelEpManager.setParentRenderer(this);
        tunnelEpManager.start();
    }

    if (!flowIdCache.empty())
        idGen.setPersistLocation(flowIdCache);

    if (connTrack) {
        ctZoneManager.setCtZoneRange(ctZoneRangeStart, ctZoneRangeEnd);
        ctZoneManager.enableNetLink(true);
        ctZoneManager.init(ID_NMSPC_CONNTRACK);

        intFlowManager.enableConnTrack();
        accessFlowManager.enableConnTrack();
    }

    intFlowManager.setEncapType(encapType);
    intFlowManager.setEncapIface(encapIface);
    intFlowManager.setUplinkIface(uplinkNativeIface);
    intFlowManager.setFloodScope(IntFlowManager::ENDPOINT_GROUP);
    if (encapType == IntFlowManager::ENCAP_VXLAN ||
        encapType == IntFlowManager::ENCAP_IVXLAN) {
        assert(tunnelRemotePort != 0);
        intFlowManager.setTunnel(tunnelRemoteIp, tunnelRemotePort);
    }
    intFlowManager.setVirtualRouter(virtualRouter, routerAdv, virtualRouterMac);
    intFlowManager.setVirtualDHCP(virtualDHCP, virtualDHCPMac);
    intFlowManager.setMulticastGroupFile(mcastGroupFile);
    intFlowManager.setEndpointAdv(endpointAdvMode, tunnelEndpointAdvMode,
            tunnelEndpointAdvIntvl);
    if(!dropLogIntIface.empty()) {
        intFlowManager.setDropLog(dropLogIntIface, dropLogRemoteIp,
                dropLogRemotePort);
    }
    if(!dropLogAccessIface.empty()) {
        accessFlowManager.setDropLog(dropLogAccessIface, dropLogRemoteIp,
                        dropLogRemotePort);
    }

    intSwitchManager.registerStateHandler(&intFlowManager);
    intSwitchManager.start(intBridgeName);
    if (accessBridgeName != "") {
        accessSwitchManager.registerStateHandler(&accessFlowManager);
        accessSwitchManager.start(accessBridgeName);
    }
    intFlowManager.start(serviceStatsFlowDisabled);
    intFlowManager.registerModbListeners();

    if (accessBridgeName != "") {
        accessFlowManager.start();
    }

    pktInHandler.registerConnection(intSwitchManager.getConnection(),
                                    (accessBridgeName != "")
                                    ? accessSwitchManager.getConnection()
                                    : NULL);
    pktInHandler.setPortMapper(&intSwitchManager.getPortMapper(),
                               (accessBridgeName != "")
                               ? &accessSwitchManager.getPortMapper()
                               : NULL);
    pktInHandler.setFlowReader(&intSwitchManager.getFlowReader());
    pktInHandler.start();

    if (ifaceStatsEnabled) {
        interfaceStatsManager.setTimerInterval(ifaceStatsInterval);
        interfaceStatsManager.
            registerConnection(intSwitchManager.getConnection(),
                               (accessBridgeName != "")
                               ? accessSwitchManager.getConnection()
                               : NULL);
        interfaceStatsManager.start();
    }
    if (contractStatsEnabled) {
        contractStatsManager.setTimerInterval(contractStatsInterval);
        contractStatsManager.setAgentUUID(getAgent().getUuid());
        contractStatsManager.
            registerConnection(intSwitchManager.getConnection());
        contractStatsManager.start();
    }
    if (serviceStatsEnabled) {
        serviceStatsManager.setTimerInterval(serviceStatsInterval);
        serviceStatsManager.setAgentUUID(getAgent().getUuid());
        serviceStatsManager.
            registerConnection(intSwitchManager.getConnection());
        serviceStatsManager.start();
    }
    if (secGroupStatsEnabled && accessBridgeName != "") {
        secGrpStatsManager.setTimerInterval(secGroupStatsInterval);
        secGrpStatsManager.setAgentUUID(getAgent().getUuid());
        secGrpStatsManager.
            registerConnection(accessSwitchManager.getConnection());
        secGrpStatsManager.start();
    }
    if (tableDropStatsEnabled) {
        tableDropStatsManager.setTimerInterval(tableDropStatsInterval);
        tableDropStatsManager.setAgentUUID(getAgent().getUuid());

        tableDropStatsManager.
            registerConnection(intSwitchManager.getConnection(),
                               (accessBridgeName != "")
                               ? accessSwitchManager.getConnection()
                               : NULL);
        tableDropStatsManager.start();
    }

    intSwitchManager.connect();
    if (accessBridgeName != "") {
        accessSwitchManager.connect();
    }

    cleanupTimer.reset(new deadline_timer(getAgent().getAgentIOService()));
    cleanupTimer->expires_from_now(CLEANUP_INTERVAL);
    cleanupTimer->async_wait(bind(&OVSRenderer::onCleanupTimer,
                                  this, error));

    if(!dropLogIntIface.empty() || !dropLogAccessIface.empty()) {
        // Inform the io_service that we are about to become a daemon. The
        // io_service cleans up any internal resources, such as threads, that may
        // interfere with forking.
        pktLoggerIO.notify_fork(boost::asio::io_service::fork_prepare);
        if(pid_t pid = fork()) {
            if (pid > 0) {
                pktLoggerIO.notify_fork(boost::asio::io_service::fork_parent);
            } else {
                LOG(ERROR) << "PacketLogger: Failed to fork:" << errno;
            }
            return;
        }
        int chdir_ret = chdir("/");
        if(chdir_ret) {
            LOG(ERROR) << "PacketLogger: Failed to chdir to /";
        }
        // Inform the io_service that we have finished becoming a daemon. The
        // io_service uses this opportunity to create any internal file descriptors
        // that need to be private to the new process.
        pktLoggerIO.notify_fork(boost::asio::io_service::fork_child);
        // Close the standard streams. This decouples the daemon from the terminal
        // that started it.
        int fd;
        fd = open("/dev/null",O_RDWR, 0);
        if (fd != -1) {
            dup2 (fd, STDIN_FILENO);
            if (fd > 0)
                close (fd);
        }
        auto logParams = getAgent().getLogParams();
        std::string log_level,log_file;
        bool toSysLog;
        std::tie(log_level, toSysLog, log_file) = logParams;
        initLogging(log_level, toSysLog, log_file);
        boost::system::error_code ec;
        boost::asio::ip::address addr = boost::asio::ip::address::from_string(LOOPBACK, ec);
        if(ec) {
            LOG(ERROR) << "PacketLogger: Failed to convert address " << LOOPBACK;
        }
        pktLogger.setAddress(addr, dropLogLocalPort);
        pktLogger.setNotifSock(getAgent().getPacketEventNotifSock());
        PacketLogHandler::TableDescriptionMap tblDescMap;
        intSwitchManager.getForwardingTableList(tblDescMap);
        pktLogger.setIntBridgeTableDescription(tblDescMap);
        accessSwitchManager.getForwardingTableList(tblDescMap);
        pktLogger.setAccBridgeTableDescription(tblDescMap);
        if(!pktLogger.startListener()) {
            exit(1);
        }

        pid_t child_pid = getpid();
        std::string fileName(std::string(PACKET_LOGGER_PIDDIR) + "/logger.pid");
        fstream s(fileName, s.out);
        if(!s.is_open()) {
            LOG(ERROR) << "Failed to open " << fileName;
        } else {
            s << child_pid;
        }
        s.close();
        // Register signal handlers.
        sigset_t waitset;
        sigemptyset(&waitset);
        sigaddset(&waitset, SIGINT);
        sigaddset(&waitset, SIGTERM);
        sigaddset(&waitset, SIGUSR1);
        sigprocmask(SIG_BLOCK, &waitset, NULL);
        std::thread signal_thread([this, &waitset]() {
            int sig;
            int result = sigwait(&waitset, &sig);
            if (result == 0) {
                LOG(INFO) << "Got " << strsignal(sig) << " signal";
            } else {
                LOG(ERROR) << "Failed to wait for signals: " << errno;
            }
            this->getPacketLogger().stopListener();
            this->getPacketLogger().stopExporter();
        });
        std::thread client_thread([this]() {
           this->getPacketLogger().startExporter();
        });
        this->pktLoggerIO.run();
        client_thread.join();
        signal_thread.join();
        exit(0);
    } else {
        LOG(DEBUG) << "DropLog interfaces not configured.Skipping creation";
    }

    ovsdbConnection.reset(new OvsdbConnection());
    ovsdbConnection->start();

    if (getAgent().isFeatureEnabled(FeatureList::ERSPAN))
        spanRenderer.start(intBridgeName, ovsdbConnection.get());
    netflowRenderer.start(intBridgeName, ovsdbConnection.get());
}

void OVSRenderer::stop() {
    if (!started) return;
    started = false;

    LOG(DEBUG) << "Stopping stitched-mode renderer";

    if (cleanupTimer) {
        cleanupTimer->cancel();
    }

    if (ifaceStatsEnabled)
        interfaceStatsManager.stop();
    if (serviceStatsEnabled)
        serviceStatsManager.stop();
    if (contractStatsEnabled)
        contractStatsManager.stop();
    if (secGroupStatsEnabled)
        secGrpStatsManager.stop();
    if(tableDropStatsEnabled)
        tableDropStatsManager.stop();

    pktInHandler.stop();

    intFlowManager.stop();
    accessFlowManager.stop();

    intSwitchManager.stop();
    accessSwitchManager.stop();
    if (getAgent().isFeatureEnabled(FeatureList::ERSPAN))
        spanRenderer.stop();
    netflowRenderer.stop();

    ovsdbConnection->stop();

    if (encapType == IntFlowManager::ENCAP_VXLAN ||
        encapType == IntFlowManager::ENCAP_IVXLAN) {
        tunnelEpManager.stop();
    }
    {
        std::string fileName(std::string(PACKET_LOGGER_PIDDIR) +"/logger.pid");
        fstream s(fileName, s.in);
        pid_t child_pid;
        if(s >> child_pid) {
            if(kill(child_pid, SIGUSR1)) {
                LOG(ERROR) << "Failed to stop logger" << errno;
            } else {
                LOG(INFO) << "terminating logger process " << child_pid;
            }
        }
    }

}

#define DEF_FLOWID_CACHEDIR \
    LOCALSTATEDIR"/lib/opflex-agent-ovs/ids"
#define DEF_MCAST_GROUPFILE \
    LOCALSTATEDIR"/lib/opflex-agent-ovs/mcast/opflex-groups.json"

void OVSRenderer::setProperties(const ptree& properties) {
    static const std::string OVS_BRIDGE_NAME("ovs-bridge-name");
    static const std::string INT_BRIDGE_NAME("int-bridge-name");
    static const std::string ACCESS_BRIDGE_NAME("access-bridge-name");

    static const std::string ENCAP_VXLAN("encap.vxlan");
    static const std::string ENCAP_IVXLAN("encap.ivxlan");
    static const std::string ENCAP_VLAN("encap.vlan");

    static const std::string UPLINK_NATIVE_IFACE("uplink-native-iface");
    static const std::string UPLINK_IFACE("uplink-iface");
    static const std::string UPLINK_VLAN("uplink-vlan");
    static const std::string ENCAP_IFACE("encap-iface");
    static const std::string REMOTE_IP("remote-ip");
    static const std::string REMOTE_PORT("remote-port");
    static const std::string LOCAL_PORT("local-port");
    static const std::string INT_BR_IFACE("int-br-iface");
    static const std::string ACC_BR_IFACE("access-br-iface");

    static const std::string VIRTUAL_ROUTER("forwarding"
                                            ".virtual-router.enabled");
    static const std::string VIRTUAL_ROUTER_MAC("forwarding"
                                                ".virtual-router.mac");

    static const std::string VIRTUAL_ROUTER_RA("forwarding.virtual-router"
                                               ".ipv6.router-advertisement");

    static const std::string VIRTUAL_DHCP("forwarding.virtual-dhcp.enabled");
    static const std::string VIRTUAL_DHCP_MAC("forwarding.virtual-dhcp.mac");

    static const std::string ENDPOINT_ADV("forwarding."
                                          "endpoint-advertisements.enabled");
    static const std::string ENDPOINT_ADV_MODE("forwarding."
                                               "endpoint-advertisements.mode");
    static const std::string ENDPOINT_TNL_ADV_MODE("forwarding."
                               "endpoint-advertisements.tunnel-endpoint-mode");
    static const std::string ENDPOINT_TNL_ADV_INTVL("forwarding."
                                   "endpoint-advertisements.tunnel-endpoint-interval");

    static const std::string FLOWID_CACHE_DIR("flowid-cache-dir");
    static const std::string MCAST_GROUP_FILE("mcast-group-file");

    static const std::string CONN_TRACK("forwarding.connection-tracking."
                                        "enabled");
    static const std::string CONN_TRACK_RANGE_START("forwarding."
                                                    "connection-tracking."
                                                    "zone-range.start");
    static const std::string CONN_TRACK_RANGE_END("forwarding."
                                                  "connection-tracking."
                                                  "zone-range.end");

    static const std::string STATS_INTERFACE_ENABLED("statistics"
                                                     ".interface.enabled");
    static const std::string STATS_INTERFACE_INTERVAL("statistics"
                                                      ".interface.interval");
    static const std::string STATS_CONTRACT_ENABLED("statistics"
                                                    ".contract.enabled");
    static const std::string STATS_CONTRACT_INTERVAL("statistics"
                                                    ".contract.interval");
    static const std::string STATS_SERVICE_FLOWDISABLED("statistics"
                                                        ".service.flow-disabled");
    static const std::string STATS_SERVICE_ENABLED("statistics"
                                                  ".service.enabled");
    static const std::string STATS_SERVICE_INTERVAL("statistics"
                                                   ".service.interval");
    static const std::string STATS_SECGROUP_ENABLED("statistics"
                                                    ".security-group.enabled");
    static const std::string STATS_SECGROUP_INTERVAL("statistics"
                                                     ".security-group"
                                                     ".interval");
    static const std::string TABLE_DROP_STATS_ENABLED("statistics"
                                                      ".table-drop.enabled");
    static const std::string TABLE_DROP_STATS_INTERVAL("statistics"
                                                       ".table-drop.interval");
    static const std::string DROP_LOG_ENCAP_GENEVE("drop-log.geneve");
    static const std::string REMOTE_NAMESPACE("namespace");

    intBridgeName =
        properties.get<std::string>(OVS_BRIDGE_NAME, "br-int");
    intBridgeName =
        properties.get<std::string>(INT_BRIDGE_NAME, intBridgeName);
    accessBridgeName =
        properties.get<std::string>(ACCESS_BRIDGE_NAME, "");

    boost::optional<const ptree&> ivxlan =
        properties.get_child_optional(ENCAP_IVXLAN);
    boost::optional<const ptree&> vxlan =
        properties.get_child_optional(ENCAP_VXLAN);
    boost::optional<const ptree&> vlan =
        properties.get_child_optional(ENCAP_VLAN);

    boost::optional<const ptree&> dropLogEncapGeneve =
            properties.get_child_optional(DROP_LOG_ENCAP_GENEVE);


    encapType = IntFlowManager::ENCAP_NONE;
    int count = 0;
    if (ivxlan) {
        LOG(ERROR) << "Encapsulation type ivxlan unsupported";
        count += 1;
    }
    if (vlan) {
        encapType = IntFlowManager::ENCAP_VLAN;
        encapIface = vlan.get().get<std::string>(ENCAP_IFACE, "");
        uplinkNativeIface = vlan.get().get<std::string>(UPLINK_NATIVE_IFACE, "");
        count += 1;
    }
    if (vxlan) {
        encapType = IntFlowManager::ENCAP_VXLAN;
        encapIface = vxlan.get().get<std::string>(ENCAP_IFACE, "");
        uplinkIface = vxlan.get().get<std::string>(UPLINK_IFACE, "");
        uplinkNativeIface = vxlan.get().get<std::string>(UPLINK_NATIVE_IFACE, "");
        uplinkVlan = vxlan.get().get<uint16_t>(UPLINK_VLAN, 0);
        tunnelRemoteIp = vxlan.get().get<std::string>(REMOTE_IP, "");
        tunnelRemotePort = vxlan.get().get<uint16_t>(REMOTE_PORT, 4789);
        count += 1;
    }

    if (count > 1) {
        LOG(WARNING) << "Multiple encapsulation types specified for "
                     << "stitched-mode renderer";
    }

    if(dropLogEncapGeneve) {
        dropLogIntIface = dropLogEncapGeneve.get().get<std::string>(INT_BR_IFACE, "");
        dropLogAccessIface = dropLogEncapGeneve.get().get<std::string>(ACC_BR_IFACE, "");
        dropLogRemoteIp = dropLogEncapGeneve.get().get<std::string>(REMOTE_IP, "192.168.1.2");
        dropLogRemotePort = dropLogEncapGeneve.get().get<uint16_t>(REMOTE_PORT, 6081);
        dropLogLocalPort = dropLogEncapGeneve.get().get<uint16_t>(LOCAL_PORT, 50000);
    }

    virtualRouter = properties.get<bool>(VIRTUAL_ROUTER, true);
    virtualRouterMac =
        properties.get<std::string>(VIRTUAL_ROUTER_MAC, "00:22:bd:f8:19:ff");
    routerAdv = properties.get<bool>(VIRTUAL_ROUTER_RA, false);
    virtualDHCP = properties.get<bool>(VIRTUAL_DHCP, true);
    virtualDHCPMac =
        properties.get<std::string>(VIRTUAL_DHCP_MAC, "00:22:bd:f8:19:ff");

    if (properties.get<bool>(ENDPOINT_ADV, true) == false) {
        endpointAdvMode = AdvertManager::EPADV_DISABLED;
    } else {
        std::string epAdvStr =
            properties.get<std::string>(ENDPOINT_ADV_MODE,
                                        "gratuitous-broadcast");
        if (epAdvStr == "gratuitous-unicast") {
            endpointAdvMode = AdvertManager::EPADV_GRATUITOUS_UNICAST;
        } else if (epAdvStr == "router-request") {
            endpointAdvMode = AdvertManager::EPADV_ROUTER_REQUEST;
        } else {
            endpointAdvMode = AdvertManager::EPADV_GRATUITOUS_BROADCAST;
        }
    }

    std::string tnlEpAdvStr =
        properties.get<std::string>(ENDPOINT_TNL_ADV_MODE,
                                    "rarp-broadcast");
    if (tnlEpAdvStr == "gratuitous-broadcast") {
        tunnelEndpointAdvMode = AdvertManager::EPADV_GRATUITOUS_BROADCAST;
    } else if(tnlEpAdvStr == "disabled") {
        tunnelEndpointAdvMode = AdvertManager::EPADV_DISABLED;
    } else {
        tunnelEndpointAdvMode = AdvertManager::EPADV_RARP_BROADCAST;
    }

    tunnelEndpointAdvIntvl =
        properties.get<uint64_t>(ENDPOINT_TNL_ADV_INTVL,
                                    300);

    connTrack = properties.get<bool>(CONN_TRACK, true);
    ctZoneRangeStart = properties.get<uint16_t>(CONN_TRACK_RANGE_START, 1);
    ctZoneRangeEnd = properties.get<uint16_t>(CONN_TRACK_RANGE_END, 65534);

    flowIdCache = properties.get<std::string>(FLOWID_CACHE_DIR,
                                              DEF_FLOWID_CACHEDIR);

    mcastGroupFile = properties.get<std::string>(MCAST_GROUP_FILE,
                                                 DEF_MCAST_GROUPFILE);

    ifaceStatsEnabled = properties.get<bool>(STATS_INTERFACE_ENABLED, true);
    contractStatsEnabled = properties.get<bool>(STATS_CONTRACT_ENABLED, true);
    serviceStatsFlowDisabled = properties.get<bool>(STATS_SERVICE_FLOWDISABLED, false);
    serviceStatsEnabled = properties.get<bool>(STATS_SERVICE_ENABLED, true);
    secGroupStatsEnabled = properties.get<bool>(STATS_SECGROUP_ENABLED, true);
    ifaceStatsInterval = properties.get<long>(STATS_INTERFACE_INTERVAL, 30000);
    tableDropStatsEnabled = properties.get<bool>(TABLE_DROP_STATS_ENABLED, true);

    contractStatsInterval =
        properties.get<long>(STATS_CONTRACT_INTERVAL, 10000);
    serviceStatsInterval =
        properties.get<long>(STATS_SERVICE_INTERVAL, 10000);
    secGroupStatsInterval =
        properties.get<long>(STATS_SECGROUP_INTERVAL, 10000);
    tableDropStatsInterval =
        properties.get<long>(TABLE_DROP_STATS_INTERVAL, 30000);
    if (ifaceStatsInterval <= 0) {
        ifaceStatsEnabled = false;
    }
    if (contractStatsInterval <= 0) {
        contractStatsEnabled = false;
    }
    if (secGroupStatsInterval <= 0) {
        secGroupStatsEnabled = false;
    }
    if(tableDropStatsInterval <= 0) {
        tableDropStatsEnabled = false;
    }
}

static bool connTrackIdGarbageCb(EndpointManager& endpointManager,
                                 opflex::ofcore::OFFramework& framework,
                                 const std::string& nmspc,
                                 const std::string& str) {
    if (str.empty()) return false;
    if (str[0] == '/') {
        // a URI means a routing domain (in IntFlowManager)
        return IdGenerator::uriIdGarbageCb
            <modelgbp::gbp::RoutingDomain>(framework, nmspc, str);
    } else {
        // a UUID means an endpoint UUID (in AccessFlowManager)
        return (bool)endpointManager.getEndpoint(str);
    }
}

void OVSRenderer::onCleanupTimer(const boost::system::error_code& ec) {
    if (ec) return;

    idGen.cleanup();
    intFlowManager.cleanup();
    accessFlowManager.cleanup();

    IdGenerator::garbage_cb_t gcb =
        bind(connTrackIdGarbageCb,
             std::ref(getEndpointManager()),
             std::ref(getFramework()), _1, _2);
    idGen.collectGarbage(ID_NMSPC_CONNTRACK, gcb);

    if (started) {
        cleanupTimer->expires_from_now(CLEANUP_INTERVAL);
        cleanupTimer->async_wait(bind(&OVSRenderer::onCleanupTimer,
                                      this, error));
    }
}

} /* namespace opflexagent */

extern "C" const opflexagent::RendererPlugin* init_renderer_plugin() {
    static const opflexagent::OVSRendererPlugin smrPlugin;

    return &smrPlugin;
}
