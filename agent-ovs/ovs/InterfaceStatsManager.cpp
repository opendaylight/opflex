/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for InterfaceStatsManager class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/logging.h>
#include "InterfaceStatsManager.h"
#include <opflexagent/Agent.h>
#include <limits>

#include "ovs-ofputil.h"

#include <lib/util.h>
extern "C" {
#include <openvswitch/ofp-msgs.h>
}

namespace opflexagent {

using boost::asio::deadline_timer;
using boost::asio::placeholders::error;
using boost::posix_time::milliseconds;
using std::bind;
using boost::system::error_code;

InterfaceStatsManager::InterfaceStatsManager(Agent* agent_,
                                             PortMapper& intPortMapper_,
                                             PortMapper& accessPortMapper_,
                                             long timer_interval_)
    : agent(agent_), intPortMapper(intPortMapper_),
      accessPortMapper(accessPortMapper_),
      intConnection(NULL), accessConnection(NULL),
      agent_io(agent_->getAgentIOService()),
      timer_interval(timer_interval_), stopping(false) {
}

InterfaceStatsManager::~InterfaceStatsManager() {
}

void InterfaceStatsManager::registerConnection(SwitchConnection* intConnection,
                                 SwitchConnection *accessConnection ) {
    this->intConnection = intConnection;
    this->accessConnection = accessConnection;
}

void InterfaceStatsManager::start() {
    stopping = false;

    if (!intConnection) { //  accessConnection is optional
        LOG(DEBUG) <<
            "Unable to start; mandatory connection (int-conn) unavailable";
        return;
    }
    LOG(DEBUG) << "Starting stats manager";

    if (intConnection)
        intConnection->RegisterMessageHandler(OFPTYPE_PORT_STATS_REPLY, this);
    if (accessConnection) {
        accessConnection->RegisterMessageHandler(OFPTYPE_PORT_STATS_REPLY,
                                                 this);
        agent->getEndpointManager().registerListener(this);
    }

    timer.reset(new deadline_timer(agent_io, milliseconds(timer_interval)));
    timer->async_wait(bind(&InterfaceStatsManager::on_timer, this, error));
}

void InterfaceStatsManager::stop() {
    LOG(DEBUG) << "Stopping stats manager";
    stopping = true;

    if (intConnection) {
        intConnection->UnregisterMessageHandler(OFPTYPE_PORT_STATS_REPLY, this);
    }
    if (accessConnection) {
        accessConnection->UnregisterMessageHandler(OFPTYPE_PORT_STATS_REPLY,
                                                             this);
        agent->getEndpointManager().unregisterListener(this);
    }

    if (timer) {
        timer->cancel();
    }
}

void InterfaceStatsManager::endpointUpdated(const std::string& uuid) {
    if (!agent->getEndpointManager().getEndpoint(uuid)) {
        std::lock_guard<std::mutex> lock(statMtx);
        intfCounterMap.erase(uuid);
    }
}

void InterfaceStatsManager::on_timer(const error_code& ec) {
    if (ec) {
        // shut down the timer when we get a cancellation
        timer.reset();
        return;
    }

    // send port stats request
    if (intConnection) {
        struct ofpbuf *intPortStatsReq = ofputil_encode_dump_ports_request(
            (ofp_version)intConnection->GetProtocolVersion(), OFPP_ANY);
        int err = intConnection->SendMessage(intPortStatsReq);
        if (err != 0) {
            LOG(ERROR) << "Failed to send int-port statistics request: "
                   << ovs_strerror(err);
        }
    }

    // send port stats request
    if (accessConnection) {
        struct ofpbuf *accessPortStatsReq = ofputil_encode_dump_ports_request(
            (ofp_version)accessConnection->GetProtocolVersion(), OFPP_ANY);
        int err = accessConnection->SendMessage(accessPortStatsReq);
        if (err != 0) {
            LOG(ERROR) << "Failed to send acc-port statistics request: "
                       << ovs_strerror(err);
        }
    }

    if (!stopping) {
        timer->expires_at(timer->expires_at() + milliseconds(timer_interval));
        timer->async_wait(bind(&InterfaceStatsManager::on_timer, this, error));
    }
}

void InterfaceStatsManager::
updateEndpointCounters(const std::string& uuid,
                       SwitchConnection * connection,
                       EndpointManager::EpCounters& counters) {
    EndpointManager& epMgr = agent->getEndpointManager();
    if (!accessConnection) {
        epMgr.updateEndpointCounters(uuid, counters);
        return;
    }

    IntfCounters& epIntfCounters = intfCounterMap[uuid];
    if (accessConnection == connection) {
        epIntfCounters.accessCounters = counters;
    } else if (intConnection == connection) {
        epIntfCounters.intCounters = counters;
    }

    if (epIntfCounters.accessCounters && epIntfCounters.intCounters) {
        EndpointManager::EpCounters epCount =
                      epIntfCounters.accessCounters.get();
        epCount.txDrop += epIntfCounters.intCounters.get().txDrop;
        epCount.rxDrop += epIntfCounters.intCounters.get().rxDrop;

        epMgr.updateEndpointCounters(uuid, epCount);
        intfCounterMap.erase(uuid);
    }
}

void InterfaceStatsManager::Handle(SwitchConnection* connection,
                                   int msgType, ofpbuf *msg) {
    assert(msgType == OFPTYPE_PORT_STATS_REPLY);
    if (!connection || !msg)
        return;

    const struct ofp_header *oh = (ofp_header *)msg->data;
    struct ofputil_port_stats ps;
    struct ofpbuf b;

    std::lock_guard<std::mutex> lock(statMtx);
    ofpbuf_use_const(&b, oh, ntohs(oh->length));
    ofpraw_pull_assert(&b);
    while (1) {
        memset(&ps, '\0', sizeof ps);
        int error_flg = ofputil_decode_port_stats(&ps, &b);
        if (error_flg)
            break;
        EndpointManager::EpCounters counters;
        memset(&counters, 0, sizeof(counters));

        counters.txPackets = ps.stats.tx_packets;
        counters.rxPackets = ps.stats.rx_packets;
        counters.txBytes = ps.stats.tx_bytes;
        counters.rxBytes = ps.stats.rx_bytes;
        counters.txDrop = ps.stats.tx_dropped;
        counters.rxDrop = ps.stats.rx_dropped;
        EndpointManager& epMgr = agent->getEndpointManager();
        std::unordered_set<std::string> endpoints;
        try {
            if (connection == intConnection) {
                const std::string& intPortName = intPortMapper.
                                       FindPort(ps.port_no);
                epMgr.getEndpointsByIface(intPortName, endpoints);
            } else {
                const std::string& accessPortName = accessPortMapper.
                                       FindPort(ps.port_no);
                epMgr.getEndpointsByAccessIface(accessPortName, endpoints);
            }
        } catch (std::out_of_range e) {
            LOG(DEBUG) << "OpenFlow port on bridge "
                       << ((connection == intConnection) ?
                           intConnection->getSwitchName() :
                           accessConnection->getSwitchName())
                       << " not found: "
                       << ps.port_no;
        }

        for (const std::string& uuid : endpoints) {
            if ((counters.rxDrop == std::numeric_limits<uint64_t>::max()) ||
                (counters.txDrop == std::numeric_limits<uint64_t>::max()))
            // set value to 0 if counter is not supported by OVS
            if (counters.rxDrop == std::numeric_limits<uint64_t>::max())
                counters.rxDrop = 0;
            if (counters.txDrop == std::numeric_limits<uint64_t>::max())
                counters.txDrop = 0;
            if (counters.txPackets == std::numeric_limits<uint64_t>::max())
                counters.txPackets = 0;
            if (counters.rxPackets == std::numeric_limits<uint64_t>::max())
                counters.rxPackets = 0;
            if (counters.rxBytes == std::numeric_limits<uint64_t>::max())
                counters.rxBytes = 0;
            if (counters.txBytes == std::numeric_limits<uint64_t>::max())
                counters.txBytes = 0;
            updateEndpointCounters(uuid, connection, counters);
        }
    }
}

} /* namespace opflexagent */
