/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for TunnelEpManager class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <config.h>
#ifdef HAVE_IFADDRS_H
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif
#include <fstream>

#include <opflex/modb/Mutator.h>
#include <boost/foreach.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "Agent.h"
#include "TunnelEpManager.h"
#include "logging.h"

namespace ovsagent {

using std::string;
using opflex::modb::URI;
using opflex::modb::Mutator;
using boost::optional;
using boost::shared_ptr;
using boost::unique_lock;
using boost::mutex;
using boost::uuids::to_string;
using boost::uuids::random_generator;
using boost::asio::deadline_timer;
using boost::asio::placeholders::error;
using boost::posix_time::milliseconds;
using boost::bind;
using boost::system::error_code;

TunnelEpManager::TunnelEpManager(Agent* agent_, long timer_interval_)
    : agent(agent_), 
      agent_io(agent_->getAgentIOService()), 
      timer_interval(timer_interval_), stopping(false) {

    boost::uuids::uuid tuuid;
    std::ifstream rndfile("/dev/urandom", std::ifstream::binary);
    if (!rndfile.is_open() ||
        !rndfile.read((char *)&tuuid, tuuid.size()).good()) {
        tuuid = random_generator()();       // use boost RNG as fallback
    }
    tunnelEpUUID = to_string(tuuid);
}

TunnelEpManager::~TunnelEpManager() {

}

void TunnelEpManager::start() {
    LOG(DEBUG) << "Starting tunnel endpoint manager";
    stopping = false;

    Mutator mutator(agent->getFramework(), "init");
    optional<shared_ptr<modelgbp::dmtree::Root> > root = 
        modelgbp::dmtree::Root::resolve(agent->getFramework(), URI::ROOT);
    if (root)
        root.get()->addGbpeTunnelEpUniverse();
    mutator.commit();

#ifdef HAVE_IFADDRS_H
    timer.reset(new deadline_timer(agent_io, milliseconds(0)));
    timer->async_wait(bind(&TunnelEpManager::on_timer, this, error));
#else
    LOG(ERROR) << "Cannot enumerate interfaces: unsupported platform";
#endif

}

void TunnelEpManager::stop() {
    LOG(DEBUG) << "Stopping tunnel endpoint manager";

    stopping = true;
    if (timer) {
        timer->cancel();
    }
}

const std::string& TunnelEpManager::getTerminationIp(const std::string& uuid) {
    unique_lock<mutex> guard(termip_mutex);
    if (uuid != tunnelEpUUID || terminationIp == "") 
        throw std::out_of_range("No such tunnel termination endpoint: " + uuid);
    return terminationIp;
}

void TunnelEpManager::on_timer(const error_code& ec) {
    if (ec) {
        // shut down the timer when we get a cancellation
        timer.reset();
        return;
    }

    std::string bestAddress;
    std::string bestIface;

#ifdef HAVE_IFADDRS_H
    // This is linux-specific.  Other plaforms will require their own
    // platform-specific interface enumeration.
    struct ifaddrs *ifaddr, *ifa;
    int family, s, n;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        // If the user specified an interface, only use that interface
        if (uplinkIface != "" && uplinkIface != ifa->ifa_name)
            continue;

        // Need an IPv4 or IPv6 address on a non-loopback up interface
        if (family != AF_INET && family != AF_INET6)
            continue;
        if (ifa->ifa_flags & IFF_LOOPBACK)
            continue;
        if (!(ifa->ifa_flags & IFF_UP))
            continue;

        if (family == AF_INET || family == AF_INET6) {
            s = getnameinfo(ifa->ifa_addr,
                            (family == AF_INET) ? sizeof(struct sockaddr_in) :
                            sizeof(struct sockaddr_in6),
                            host, NI_MAXHOST,
                            NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                LOG(ERROR) << "getnameinfo() failed: " << gai_strerror(s);
                continue;
            }
            bestAddress = host;
            // remove address scope if present
            size_t scope = bestAddress.find_first_of('%');
            if (scope != string::npos) {
                bestAddress.erase(scope);
            }

            bestIface = ifa->ifa_name;
            if (family == AF_INET) {
                // prefer ipv4 address, if present
                break;
            }
        }
    }

    freeifaddrs(ifaddr);
#endif

    if (bestAddress != "" && bestAddress != terminationIp) {
        {
            unique_lock<mutex> guard(termip_mutex);
            terminationIp = bestAddress;
        }

        LOG(INFO) << "Discovered uplink IP address " << bestAddress 
                  << " on " << bestIface;

        using namespace modelgbp::gbpe;
        Mutator mutator(agent->getFramework(), "policyelement");
        optional<shared_ptr<TunnelEpUniverse> > tu = 
            TunnelEpUniverse::resolve(agent->getFramework());
        if (tu) {
            tu.get()->addGbpeTunnelEp(tunnelEpUUID)
                ->setTerminatorIp(bestAddress);
        }

        mutator.commit();

        notifyListeners(tunnelEpUUID);
    }

    if (!stopping) {
        timer->expires_at(timer->expires_at() + milliseconds(timer_interval));
        timer->async_wait(bind(&TunnelEpManager::on_timer, this, error));
    }
}

void TunnelEpManager::registerListener(EndpointListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    tunnelEpListeners.push_back(listener);
}

void TunnelEpManager::unregisterListener(EndpointListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    tunnelEpListeners.remove(listener);
}

void TunnelEpManager::notifyListeners(const std::string& uuid) {
    unique_lock<mutex> guard(listener_mutex);
    BOOST_FOREACH(EndpointListener* listener, tunnelEpListeners) {
        listener->endpointUpdated(uuid);
    }
}

} /* namespace ovsagent */
