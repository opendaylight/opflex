/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for MulticastListener class
 *
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/MulticastListener.h>
#include <opflexagent/logging.h>

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/ip/multicast.hpp>

#include <cstdio>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>

namespace opflexagent {

namespace ba = boost::asio;
using std::bind;
using std::shared_ptr;
using std::unordered_set;
using std::string;

#define LISTEN_PORT 34242

MulticastListener::MulticastListener(ba::io_service& io_service_)
    : io_service(io_service_), socket_v4(io_service_), socket_v6(io_service_),
      running(true) {

    LOG(INFO) << "Starting multicast listener";

    ba::ip::udp::endpoint v4_endpoint(ba::ip::udp::v4(), LISTEN_PORT);
    ba::ip::udp::endpoint v6_endpoint(ba::ip::udp::v6(), LISTEN_PORT);

    socket_v4.open(v4_endpoint.protocol());
    socket_v4.set_option(ba::socket_base::reuse_address(true));
    socket_v4.bind(v4_endpoint);

    socket_v6.open(v6_endpoint.protocol());
    socket_v6.set_option(ba::socket_base::reuse_address(true));
    socket_v6.set_option(ba::ip::v6_only(true));
    socket_v6.bind(v6_endpoint);
}

MulticastListener::~MulticastListener() {
    stop();
}

void MulticastListener::do_stop() {
    socket_v4.close();
    socket_v6.close();
}

void MulticastListener::stop() {
    if (!running) return;
    running = false;

    LOG(INFO) << "Shutting down";

    io_service.dispatch(bind(&MulticastListener::do_stop, this));
}

void MulticastListener::join(std::string mcast_address) {
    boost::system::error_code ec;
    ba::ip::address addr = ba::ip::address::from_string(mcast_address, ec);
    if (ec) {
        LOG(ERROR) << "Cannot join invalid multicast group: "
                     << mcast_address << ": " << ec.message();
        return;
    } else if (!addr.is_multicast()) {
        LOG(ERROR) << "Address is not a multicast address: " << addr;
        return;
    }

    LOG(INFO) << "Joining group " << addr;

    if (addr.is_v4())
        socket_v4.set_option(ba::ip::multicast::join_group(addr), ec);
    else
        socket_v6.set_option(ba::ip::multicast::join_group(addr), ec);

    if (ec)
        LOG(ERROR) << "Could not join group " << addr << ": " << ec.message();
}

void MulticastListener::leave(std::string mcast_address) {
    boost::system::error_code ec;
    ba::ip::address addr = ba::ip::address::from_string(mcast_address, ec);
    if (ec)
        return;

    LOG(INFO) << "Leaving group " << addr;

    if (addr.is_v4())
        socket_v4.set_option(ba::ip::multicast::leave_group(addr), ec);
    else
        socket_v6.set_option(ba::ip::multicast::leave_group(addr), ec);

    if (ec)
        LOG(ERROR) << "Could not leave group " << addr << ": " << ec.message();
}

void MulticastListener::sync(const shared_ptr<unordered_set<string> >& naddrs) {
    unordered_set<std::string>::iterator it = addresses.begin();
    while (it != addresses.end()) {
        if (naddrs->find(*it) == naddrs->end()) {
            leave(*it);
            it = addresses.erase(it);
        } else {
            ++it;
        }
    }
    for (const std::string& addr : *naddrs) {
        if (addresses.find(addr) == addresses.end()) {
            join(addr);
            addresses.insert(addr);
        }
    }
}

} /* namespace opflexagent */
