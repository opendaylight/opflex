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
#include <cerrno>
#include <random>

#include <opflex/modb/Mutator.h>
#include <opflex/modb/MAC.h>
#include <modelgbp/gbpe/EncapTypeEnumT.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <opflexagent/Agent.h>
#include <opflexagent/TunnelEpManager.h>
#include <opflexagent/Renderer.h>
#include <opflexagent/logging.h>

namespace opflexagent {

using std::string;
using std::shared_ptr;
using std::unique_lock;
using std::mutex;
using opflex::modb::URI;
using opflex::modb::Mutator;
using boost::optional;
using boost::uuids::to_string;
using boost::uuids::basic_random_generator;
using boost::asio::deadline_timer;
using boost::asio::placeholders::error;
using boost::posix_time::milliseconds;
using boost::system::error_code;

TunnelEpManager::TunnelEpManager(Agent* agent_, long timer_interval_)
    : agent(agent_),
      agent_io(agent_->getAgentIOService()),
      timer_interval(timer_interval_), stopping(false), uplinkVlan(0),
      terminationIpIsV4(false) {
    std::random_device rng;
    std::mt19937 urng(rng());
    tunnelEpUUID = to_string(basic_random_generator<std::mt19937>(urng)());
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

const std::string& TunnelEpManager::getTerminationMac(const std::string& uuid) {
    if (uuid != tunnelEpUUID || terminationMac == "")
        throw std::out_of_range("No such tunnel termination endpoint: " + uuid);
    return terminationMac;
}

#ifdef HAVE_IFADDRS_H
static string getInterfaceMac(const string& iface) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) {
        int err = errno;
        LOG(ERROR) << "Socket creation failed when getting MAC address of "
            << iface << ", error: " << strerror(err);
        return "";
    }

    ifreq ifReq;
    strncpy(ifReq.ifr_name, iface.c_str(), sizeof(ifReq.ifr_name));
    if (ioctl(sock, SIOCGIFHWADDR, &ifReq) != -1) {
        close(sock);
        return
            opflex::modb::MAC((uint8_t*)(ifReq.ifr_hwaddr.sa_data)).toString();
    } else {
        int err = errno;
        close(sock);
        LOG(ERROR) << "ioctl to get MAC address failed for " << iface
            << ", error: " << strerror(err);
        return "";
    }
}

static string getInterfaceAddressV4(const string& iface) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) {
        int err = errno;
        LOG(ERROR) << "Socket creation failed when getting IPv4 address of "
             << iface << ", error: " << strerror(err);
        return "";
    }

    ifreq ifReq;
    strncpy(ifReq.ifr_name, iface.c_str(), sizeof(ifReq.ifr_name));
    if (ioctl(sock, SIOCGIFADDR, &ifReq) != -1) {
        close(sock);
        char host[NI_MAXHOST];
        int s = getnameinfo(&ifReq.ifr_addr, sizeof(struct sockaddr_in),
                            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        if (s != 0) {
            LOG(ERROR) << "getnameinfo() failed: " << gai_strerror(s);
            return "";
        }
        return host;
    } else {
        int err = errno;
        close(sock);
        LOG(ERROR) << "ioctl to get IPv4 address failed for " << iface
            << ", error: " << strerror(err);
        return "";
    }
}
#endif

void TunnelEpManager::on_timer(const error_code& ec) {
    if (ec) {
        // shut down the timer when we get a cancellation
        timer.reset();
        return;
    }

    string bestAddress;
    string bestIface;
    string bestMac;
    bool bestAddrIsV4 = false;
    if(renderer && renderer->isUplinkAddressImplemented()) {
        boost::asio::ip::address bestAddr = renderer->getUplinkAddress();
        if(!bestAddr.is_unspecified()) {
            bestAddress = bestAddr.to_string();
        }
        string l2_address = renderer->getUplinkMac();
        if(l2_address.length()==17) {
            bestMac = renderer->getUplinkMac();
        }
    } else {
#ifdef HAVE_IFADDRS_H
    // This is linux-specific.  Other plaforms will require their own
    // platform-specific interface enumeration.

    /*
     * If uplink interface was specified and it resolved to an IPv4 address
     * in the previous iteration, re-fetch the address for that interface
     * using ioctl(). Otherwise use getifaddrs() to iterate over all
     * interfaces. This check tries to avoid getifaddrs() in the common case
     * because getifaddrs() can expensive if there are many endpoints
     * (and hence interfaces). The ioctl() doesn't work for IPv6 addresses,
     * so one can't avoid getifaddrs() with IPv6 only.
     */
    if (!uplinkIface.empty() && terminationIpIsV4) {
        bestAddress = getInterfaceAddressV4(uplinkIface);
        if (!bestAddress.empty()) {
            bestIface = uplinkIface;
            bestAddrIsV4 = true;
        }
    }

    if (bestAddress.empty()) {
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
                bestAddrIsV4 = (family == AF_INET);
                if (family == AF_INET) {
                    // prefer ipv4 address, if present
                    break;
                }
            }
        }
        freeifaddrs(ifaddr);
    }

    if (!bestAddress.empty()) {
        bestMac = getInterfaceMac(bestIface);
    }
    terminationIpIsV4 = bestAddrIsV4;

#endif
    }

    if ((!bestAddress.empty() && bestAddress != terminationIp) ||
        (!bestMac.empty() && bestMac != terminationMac)) {
        {
            unique_lock<mutex> guard(termip_mutex);
            terminationIp = bestAddress;
            terminationMac = bestMac;
            agent->setUplinkMac(bestMac);
        }

        LOG(INFO) << "Discovered uplink IP: " << bestAddress
                  << ", MAC: " << bestMac
                  << " on interface: " << bestIface;

        using namespace modelgbp::gbpe;
        Mutator mutator(agent->getFramework(), "policyelement");
        optional<shared_ptr<TunnelEpUniverse> > tu =
            TunnelEpUniverse::resolve(agent->getFramework());
        if (tu) {
            shared_ptr<TunnelEp> tunnelEp =
                tu.get()->addGbpeTunnelEp(tunnelEpUUID);
            tunnelEp->setTerminatorIp(bestAddress)
                .setMac(opflex::modb::MAC(bestMac));
            if (uplinkVlan != 0) {
                tunnelEp->setEncapType(EncapTypeEnumT::CONST_VLAN)
                    .setEncapId(uplinkVlan);
            }
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
    for (EndpointListener* listener : tunnelEpListeners) {
        listener->endpointUpdated(uuid);
    }
}

} /* namespace opflexagent */
