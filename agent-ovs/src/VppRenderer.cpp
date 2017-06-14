/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "VppRenderer.h"
#include "logging.h"

#include <boost/asio/placeholders.hpp>
#include <boost/algorithm/string/split.hpp>

namespace ovsagent {

    using std::bind;
    using opflex::ofcore::OFFramework;
    using boost::property_tree::ptree;
    using boost::asio::placeholders::error;

    VppRenderer::VppRenderer(Agent& agent_):
        Renderer(agent_),
        vppManager(agent_, idGen),
        tunnelRemotePort(0), uplinkVlan(0),
        started(false) {
        LOG(INFO) << "Vpp Renderer";
    }

    VppRenderer::~VppRenderer() {

    }

    void VppRenderer::start() {
        if (started) return;
        started = true;
        vppManager.start(bridgeName);
        vppManager.registerModbListeners();
        LOG(DEBUG) << "Starting vpp renderer using bridge" << bridgeName;

        if (encapType == VppManager::ENCAP_NONE)
            LOG(WARNING)
            << "No encapsulation type specified; only local traffic will work";

        if (encapType == VppManager::ENCAP_VXLAN ||
            encapType == VppManager::ENCAP_IVXLAN) {
            /*
             * TunnelEpManager:
             *                  Need to check, if we can use
             * it here or need something similar for Vpp.
             *
             * Set the uplink and uplinkVlan and start it
             */
        }

        vppManager.setEncapType(encapType);
        vppManager.setEncapIface(encapIface);
        vppManager.setFloodScope(VppManager::ENDPOINT_GROUP);
        if (encapType == VppManager::ENCAP_VXLAN ||
            encapType == VppManager::ENCAP_IVXLAN) {
            assert(tunnelRemotePort != 0);
            vppManager.setTunnel(tunnelRemoteIp, tunnelRemotePort);
        }


    }

    void VppRenderer::stop() {
        if (!started) return;
        started = false;
        LOG(DEBUG) << "Stopping vpp renderer";

        vppManager.stop();

        if (encapType == VppManager::ENCAP_VXLAN ||
            encapType == VppManager::ENCAP_IVXLAN) {
            /*
             * Need to Stop TunnelEpManager
             */
        }
    }

    Renderer* VppRenderer::create(Agent& agent) {
        return new VppRenderer(agent);
    }

    void VppRenderer::setProperties(const ptree& properties) {
        static const std::string VPP_BRIDGE_NAME("opflex-vpp");

        static const std::string ENCAP_VXLAN("encap.vxlan");
        static const std::string ENCAP_IVXLAN("encap.ivxlan");
        static const std::string ENCAP_VLAN("encap.vlan");

        static const std::string UPLINK_IFACE("uplink-iface");
        static const std::string UPLINK_VLAN("uplink-vlan");
        static const std::string ENCAP_IFACE("encap-iface");
        static const std::string REMOTE_IP("remote-ip");
        static const std::string REMOTE_PORT("remote-port");

        bridgeName = properties.get<std::string>(VPP_BRIDGE_NAME, "vpp");

        LOG(DEBUG) << "Bridge Name " << bridgeName;

        boost::optional<const ptree&> ivxlan =
            properties.get_child_optional(ENCAP_IVXLAN);
        boost::optional<const ptree&> vxlan =
            properties.get_child_optional(ENCAP_VXLAN);
        boost::optional<const ptree&> vlan =
            properties.get_child_optional(ENCAP_VLAN);

        encapType = VppManager::ENCAP_NONE;
        int count = 0;
        if (ivxlan) {
            LOG(ERROR) << "Encapsulation type ivxlan unsupported";
            count += 1;
        }
        if (vlan) {
            encapType = VppManager::ENCAP_VLAN;
            encapIface = vlan.get().get<std::string>(ENCAP_IFACE, "");
            count += 1;
        }
        if (vxlan) {
            encapType = VppManager::ENCAP_VXLAN;
            encapIface = vxlan.get().get<std::string>(ENCAP_IFACE, "");
            uplinkIface = vxlan.get().get<std::string>(UPLINK_IFACE, "");
            uplinkVlan = vxlan.get().get<uint16_t>(UPLINK_VLAN, 0);
            tunnelRemoteIp = vxlan.get().get<std::string>(REMOTE_IP, "");
            tunnelRemotePort = vxlan.get().get<uint16_t>(REMOTE_PORT, 4789);
            count += 1;
        }
        if (count > 1) {
            LOG(WARNING) << "Multiple encapsulation types specified for "
                     << "vpp renderer";
        }
    }

}
