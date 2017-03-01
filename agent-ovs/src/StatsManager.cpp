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

StatsManager::StatsManager(Agent* agent_, PortMapper& intPortMapper_,
                           PortMapper& accessPortMapper_, long timer_interval_)
    : agent(agent_), intPortMapper(intPortMapper_),accessPortMapper(accessPortMapper_),
      intConnection(NULL), accessConnection(NULL),
      agent_io(agent_->getAgentIOService()),
      timer_interval(timer_interval_), stopping(false) {

}

StatsManager::~StatsManager() {
}

void StatsManager::registerConnection(SwitchConnection* intConnection, SwitchConnection *accessConnection ) {
    this->intConnection = intConnection;
    this->accessConnection = accessConnection;
}

void StatsManager::start() {
    stopping = false;

    if ( ! intConnection || ! accessConnection) {
        LOG(DEBUG) << "No connections are registered: cannot start stats manager";
        return;
    }
    LOG(DEBUG) << "Starting stats manager";

    if (intConnection)
        intConnection->RegisterMessageHandler(OFPTYPE_PORT_STATS_REPLY, this);
    if (accessConnection) {
        accessConnection->RegisterMessageHandler(OFPTYPE_PORT_STATS_REPLY, this);
        agent->getEndpointManager().registerListener(this);
    }

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
        agent->getEndpointManager().unregisterListener(this);
    }

    if (timer) {
        timer->cancel();
    }
}

void StatsManager::endpointUpdated(const std::string& uuid) {
    intfCounterMap.erase(uuid);
}

void StatsManager::on_timer(const error_code& ec) {
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
        timer->async_wait(bind(&StatsManager::on_timer, this, error));
    }
}

void StatsManager::updateEndpointCounters(const std::string& uuid, SwitchConnection * connection,
                                           EndpointManager::EpCounters counters) {
   IntfCounters tmpIntfCounters;
   EndpointManager& epMgr = agent->getEndpointManager();

   if (accessConnection) { // intConnection is always expected, accessConnection is optional
        std::unordered_map<std::string, IntfCounters>::iterator it = intfCounterMap.find(uuid);
        if (it == intfCounterMap.end()) {
            if (accessConnection == connection) {
                tmpIntfCounters.accessCounters = counters;
            } else {
                tmpIntfCounters.intCounters = counters;
            }
            intfCounterMap.insert(std::make_pair(uuid, tmpIntfCounters));
            return; // (for better code readability)
        }
        else {
            // counter exists
            tmpIntfCounters = it->second;

            // it can be another counter from same connection
            if ((accessConnection == connection && ! tmpIntfCounters.intCounters) ||
                    (intConnection == connection && ! tmpIntfCounters.accessCounters)) {

                if (accessConnection == connection) {
                    tmpIntfCounters.accessCounters = counters;
                } else {
                    tmpIntfCounters.intCounters = counters;
                }

                intfCounterMap.erase(uuid);
                intfCounterMap.insert(std::make_pair(uuid, tmpIntfCounters));
                return;
            }

            // one counter exists and new counter is from the other connection
            // update epMgr when counters from both connections are available
            if (accessConnection == connection && tmpIntfCounters.intCounters){
                counters.txDrop += tmpIntfCounters.intCounters.get().txDrop;
                counters.rxDrop += tmpIntfCounters.intCounters.get().rxDrop;
            }
            if (intConnection == connection && tmpIntfCounters.accessCounters) {
                counters.txPackets = tmpIntfCounters.accessCounters.get().txPackets;
                counters.rxPackets = tmpIntfCounters.accessCounters.get().rxPackets;
                counters.txBytes = tmpIntfCounters.accessCounters.get().txBytes;
                counters.rxBytes = tmpIntfCounters.accessCounters.get().rxBytes;
                counters.txDrop += tmpIntfCounters.accessCounters.get().txDrop;
                counters.rxDrop += tmpIntfCounters.accessCounters.get().rxDrop;
            }
            epMgr.updateEndpointCounters(uuid, counters);
            intfCounterMap.erase(uuid);
        }
    } else {
         epMgr.updateEndpointCounters(uuid, counters);
    }

}
void StatsManager::Handle(SwitchConnection* connection,
                          int msgType, ofpbuf *msg) {
    assert(msgType == OFPTYPE_PORT_STATS_REPLY);
    if ( ! connection || ! msg)
        return;

    const struct ofp_header *oh = (ofp_header *)msg->data;
    struct ofputil_port_stats ps;
    struct ofpbuf b;

    std::lock_guard<std::mutex> lock(statMtx);
    ofpbuf_use_const(&b, oh, ntohs(oh->length));
    ofpraw_pull_assert(&b);
    while (!ofputil_decode_port_stats(&ps, &b)) {
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
               const std::string& intPortName = intPortMapper.FindPort(ps.port_no);
               epMgr.getEndpointsByIface(intPortName, endpoints);
           } else {
               const std::string& accessPortName = accessPortMapper.FindPort(ps.port_no);
               epMgr.getEndpointsByAccessIface(accessPortName, endpoints);
           }
        } catch (std::out_of_range e) {
           // port not known yet
           LOG(DEBUG) << "Exception in stats manager Handle method";
        }

        for (const std::string& uuid : endpoints) {
             updateEndpointCounters(uuid, connection, counters);
        }
    }
}

} /* namespace ovsagent */
