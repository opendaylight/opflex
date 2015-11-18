/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for MulticastListener
 *
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OVSAGENT_MULTICAST_LISTENER_H
#define OVSAGENT_MULTICAST_LISTENER_H

#include <boost/shared_ptr.hpp>
#include <boost/unordered_set.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>

namespace ovsagent {

/**
 * A server that simply subscribes to a set of multicast addresses and
 * holds them open.
 */
class MulticastListener : private boost::noncopyable {
public:
    /**
     * Instantiate the listener
     *
     * @param io_service the io_service object
     */
    MulticastListener(boost::asio::io_service& io_service);

    /**
     * Destroy the server and clean up all state
     */
    ~MulticastListener();

    /**
     * Stop the server
     */
    void stop();

    /**
     * Make the subscriptions match the given set
     */
    void sync(boost::shared_ptr<boost::unordered_set<std::string> >& addrs);

private:
    boost::asio::io_service& io_service;
    boost::asio::ip::udp::socket socket_v4;
    boost::asio::ip::udp::socket socket_v6;
    boost::unordered_set<std::string> addresses;
    volatile bool running;

    void join(std::string mcast_address);
    void leave(std::string mcast_address);

    void do_stop();
};

} /* namespace ovsagent */

#endif /* OVSAGENT_MULTICAST_LISTENER_H */
