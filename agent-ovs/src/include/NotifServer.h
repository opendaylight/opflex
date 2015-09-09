/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for NotifServer
 *
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OVSAGENT_NOTIF_SERVER_H
#define OVSAGENT_NOTIF_SERVER_H

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_set.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <opflex/modb/MAC.h>

#include "KeyedRateLimiter.h"

namespace ovsagent {

/**
 * A server that listens to a UNIX socket and sends notifications to
 * listeners that connect and register on the socket.
 */
class NotifServer : private boost::noncopyable {
public:
    /**
     * Instantiate a notif server
     *
     * @param io_service the io_service object
     */
    NotifServer(boost::asio::io_service& io_service);

    /**
     * Destroy the server and clean up all state
     */
    ~NotifServer();

    /**
     * Set the path for the notif server socket
     *
     * @param path the path to the socket
     */
    void setSocketName(const std::string& path);

    /**
     * Start the server
     */
    void start();

    /**
     * Stop the server
     */
    void stop();

    /**
     * Dispatch a virtual IP notification indicating an endpoint is
     * claiming ownership for the given MAC address and/or IP pair.
     *
     * @param uuids a list of endpoint UUIDs associated with the
     * virtual IP notificationn
     * @param macAddr the MAC address seen in the advertisement
     * @param ipAddr the IP address seen in the advertisement
     */
    void dispatchVirtualIp(boost::unordered_set<std::string> uuids,
                           const opflex::modb::MAC& macAddr,
                           const std::string& ipAddr);

    /**
     * An internal session object
     */
    class session;

    /**
     * An internal session object pointer
     */
    typedef boost::shared_ptr<session> session_ptr;

private:
    boost::asio::io_service& io_service;
    std::string notifSocketPath;
    volatile bool running;

    std::set<session_ptr> sessions;

    boost::scoped_ptr<boost::asio::local::stream_protocol::acceptor> acceptor;

    KeyedRateLimiter<std::string, 3, 5000> vipLimiter;

    void accept();
    void do_stop();
    void handle_accept(session_ptr new_session,
                       const boost::system::error_code& error);
};

} /* namespace ovsagent */

#endif /* OVSAGENT_NOTIF_SERVER_H */
