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

#include <boost/foreach.hpp>

#include "logging.h"
#include "StatsManager.h"
#include "Agent.h"
#include "ovs.h"

namespace ovsagent {

using namespace opflex::enforcer;
using boost::asio::deadline_timer;
using boost::asio::placeholders::error;
using boost::posix_time::milliseconds;
using boost::bind;
using boost::system::error_code;

StatsManager::StatsManager(Agent* agent_, PortMapper& portMapper_,
                           long timer_interval_)
    : agent(agent_), portMapper(portMapper_),
      agent_io(agent_->getAgentIOService()), 
      timer_interval(timer_interval_), stopping(false) {

}

StatsManager::~StatsManager() {

}

void StatsManager::registerConnection(SwitchConnection* connection) {
    this->connection = connection;
}

void StatsManager::start() {
    LOG(DEBUG) << "Starting stats manager";
    stopping = false;

    connection->RegisterMessageHandler(OFPTYPE_PORT_STATS_REPLY, this);

    timer.reset(new deadline_timer(agent_io, milliseconds(timer_interval)));
    timer->async_wait(bind(&StatsManager::on_timer, this, error));
}

void StatsManager::stop() {
    LOG(DEBUG) << "Stopping stats manager";
    stopping = true;

    connection->UnregisterMessageHandler(OFPTYPE_PORT_STATS_REPLY, this);

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
    struct ofpbuf *portStatsReq = 
        ofputil_encode_dump_ports_request(connection->GetProtocolVersion(),
                                          OFPP_ANY);
    int err = connection->SendMessage(portStatsReq);
    if (err != 0) {
        LOG(ERROR) << "Failed to send port statistics request: "
                   << ovs_strerror(err);
    }

    if (!stopping) {
        timer->expires_at(timer->expires_at() + milliseconds(timer_interval));
        timer->async_wait(bind(&StatsManager::on_timer, this, error));
    }
}


void StatsManager::Handle(SwitchConnection *conn, 
                          ofptype msgType, ofpbuf *msg) {
    assert(msgType == OFPTYPE_PORT_STATS_REPLY);

    const struct ofp_header *oh = (ofp_header *)ofpbuf_data(msg);
    struct ofputil_port_stats ps;
    struct ofpbuf b;

    ofpbuf_use_const(&b, oh, ntohs(oh->length));
    ofpraw_pull_assert(&b);
    while (!ofputil_decode_port_stats(&ps, &b)) {
        EndpointManager::EpCounters txcounters;
        EndpointManager::EpCounters rxcounters;

        txcounters.packets = ps.stats.tx_packets;
        rxcounters.packets = ps.stats.rx_packets;
        txcounters.bytes = ps.stats.tx_bytes;
        rxcounters.bytes = ps.stats.rx_bytes;
        txcounters.drop = ps.stats.tx_dropped;
        rxcounters.drop = ps.stats.rx_dropped;

        EndpointManager& epMgr = agent->getEndpointManager();
        boost::unordered_set<std::string> endpoints;
        try {
            const std::string& portName = portMapper.FindPort(ps.port_no);
            epMgr.getEndpointsByIface(portName, endpoints);
        } catch (std::out_of_range e) {
            // port not known yet
        }

        BOOST_FOREACH(const std::string& uuid, endpoints) {
            epMgr.updateEndpointCounters(uuid, true, txcounters);
            epMgr.updateEndpointCounters(uuid, false, rxcounters);
        }
    }
}

} /* namespace ovsagent */
