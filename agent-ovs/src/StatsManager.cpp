/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for StatsManager class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "logging.h"
#include "StatsManager.h"
#include "Agent.h"

#include "ovs-ofputil.h"

#include <lib/util.h>
extern "C" {
#include <openvswitch/ofp-msgs.h>
}

namespace ovsagent {

using boost::asio::deadline_timer;
using boost::asio::placeholders::error;
using boost::posix_time::milliseconds;
using std::bind;
using boost::system::error_code;

StatsManager::StatsManager(Agent* agent_, PortMapper& portMapper_,
                           long timer_interval_)
    : agent(agent_), portMapper(portMapper_),
      intConnection(NULL), accessConnection(NULL),
      agent_io(agent_->getAgentIOService()),
      timer_interval(timer_interval_), stopping(false) {

}

StatsManager::~StatsManager() {

}

void StatsManager::registerConnection(SwitchConnection* intConnection, SwitchConnection *accessConnection ) {
    this->intConnection = intConnection;
    this->accessConnection = accessConnection;
    if (accessConnection != NULL) {
        this->numConnections = 2;
    }
}

void StatsManager::start() {
    LOG(DEBUG) << "Starting stats manager";
    stopping = false;

    intConnection->RegisterMessageHandler(OFPTYPE_PORT_STATS_REPLY, this);
    accessConnection->RegisterMessageHandler(OFPTYPE_PORT_STATS_REPLY, this);

    timer.reset(new deadline_timer(agent_io, milliseconds(timer_interval)));
    timer->async_wait(bind(&StatsManager::on_timer, this, error));
}

void StatsManager::stop() {
    LOG(DEBUG) << "Stopping stats manager";
    stopping = true;

    if (intConnection) {
        intConnection->UnregisterMessageHandler(OFPTYPE_PORT_STATS_REPLY, this);
    }
    if (accessConnection) {
        accessConnection->UnregisterMessageHandler(OFPTYPE_PORT_STATS_REPLY, this);
    }

    if (timer) {
        timer->cancel();
    }
}

void StatsManager::on_timer(const error_code& ec) {
    if (ec) {
        // shut down the timer when we get a cancellation
        timer.reset();
        return;
    }

    // send port stats request
    struct ofpbuf *intPortStatsReq = ofputil_encode_dump_ports_request(
        (ofp_version)intConnection->GetProtocolVersion(), OFPP_ANY);
    int err = intConnection->SendMessage(intPortStatsReq);
    if (err != 0) {
        LOG(ERROR) << "Failed to send port statistics request: "
                   << ovs_strerror(err);
    }

    // send port stats request
    struct ofpbuf *accessPortStatsReq = ofputil_encode_dump_ports_request(
        (ofp_version)accessConnection->GetProtocolVersion(), OFPP_ANY);
    err = accessConnection->SendMessage(accessPortStatsReq);
    if (err != 0) {
        LOG(ERROR) << "Failed to send port statistics request: "
                   << ovs_strerror(err);
    }

    if (!stopping) {
        timer->expires_at(timer->expires_at() + milliseconds(timer_interval));
        timer->async_wait(bind(&StatsManager::on_timer, this, error));
    }
}


void StatsManager::Handle(SwitchConnection* connection,
                          int msgType, ofpbuf *msg) {
    assert(msgType == OFPTYPE_PORT_STATS_REPLY);

    const struct ofp_header *oh = (ofp_header *)msg->data;
    struct ofputil_port_stats ps;
    struct ofpbuf b;
    static EndpointManager::EpCounters counters;
    static bool intBridgeReply = false;
    static bool accessBridgeReply = false;
    if (connection->getSwitchName().compare("br-int") == 0) {
        intBridgeReply = true;
        if (numConnections > 1 && accessBridgeReply == false) {
            memset(&counters, 0, sizeof(counters));
        }

    } else {
        accessBridgeReply = true;
        if (numConnections > 1 && intBridgeReply == false) {
            memset(&counters, 0, sizeof(counters));
        }
    }

    ofpbuf_use_const(&b, oh, ntohs(oh->length));
    ofpraw_pull_assert(&b);
    while (!ofputil_decode_port_stats(&ps, &b)) {
       if (intBridgeReply) {
           counters.txPackets = ps.stats.tx_packets;
           counters.txBytes = ps.stats.tx_bytes;
           if (numConnections == 1) {
               counters.rxPackets = ps.stats.rx_packets;
               counters.rxBytes = ps.stats.rx_bytes;
           }
           counters.txDrop = counters.txDrop + ps.stats.tx_dropped;
           counters.rxDrop = counters.rxDrop + ps.stats.rx_dropped;
        } else {
           counters.rxPackets = ps.stats.rx_packets;
           counters.rxBytes = ps.stats.rx_bytes;
           counters.txDrop = counters.txDrop + ps.stats.tx_dropped;
           counters.rxDrop = counters.rxDrop + ps.stats.rx_dropped;
        }

       if ((numConnections  == 1 && intBridgeReply) ||
         (numConnections > 1 && accessBridgeReply && intBridgeReply)) {
          EndpointManager& epMgr = agent->getEndpointManager();
          std::unordered_set<std::string> endpoints;
          try {
              const std::string& portName = portMapper.FindPort(ps.port_no);
              epMgr.getEndpointsByIface(portName, endpoints);
          } catch (std::out_of_range e) {
           // port not known yet
          }

          for (const std::string& uuid : endpoints) {
               epMgr.updateEndpointCounters(uuid, counters);
          }
          memset(&counters, 0, sizeof(counters));
          intBridgeReply  = false;
          accessBridgeReply  = false;
        }
    }
}

} /* namespace ovsagent */
