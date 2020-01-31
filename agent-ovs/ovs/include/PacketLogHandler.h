/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for PacketLogHandler
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#include <thread>
#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <opflexagent/logging.h>
#include <opflexagent/logging.h>
#include "PacketDecoderLayers.h"
#include <mutex>
#include <condition_variable>
#include <queue>

#pragma once
#ifndef OPFLEXAGENT_PACKETLOGHANDLER_H_
#define OPFLEXAGENT_PACKETLOGHANDLER_H_

namespace opflexagent {

class PacketLogHandler;

/**
 * Class to listen on the given UDP port
 */
class UdpServer
{
public:
    /**
     * Constructor for UDP listener
     * @param logHandler reference to the parent LogHandler
     * @param io_service reference to IO service to handle UDP
     * @param addr IP address to listen on
     * @param port listener UDP port
     */
    UdpServer(PacketLogHandler &logHandler,
            boost::asio::io_service& io_service,
               boost::asio::ip::address &addr, uint16_t port)
    : pktLogger(logHandler), serverSocket(io_service,
            boost::asio::ip::udp::endpoint(addr, port)),
            stopped(false) {
        serverSocket.set_option(boost::asio::socket_base::reuse_address(true));
    }
    /**
     * Start UDP listener
     */
    void startReceive() {
        serverSocket.async_receive_from(
            boost::asio::buffer(recv_buffer, 4096), remoteEndpoint,
            boost::bind(&UdpServer::handleReceive, this,
              boost::asio::placeholders::error,
              boost::asio::placeholders::bytes_transferred));
    }
    /**
     * Stop UDP listener
     */
    void stop() {
        boost::system::error_code ec;
        stopped = true;
        serverSocket.cancel(ec);
        serverSocket.close(ec);
    }
private:
    /**
     * Handle received UDP packets
     */
    void handleReceive(const boost::system::error_code& error,
      std::size_t bytes_transferred);
    PacketLogHandler &pktLogger;
    boost::asio::ip::udp::socket serverSocket;
    boost::asio::ip::udp::endpoint remoteEndpoint;
    boost::array<unsigned char, 4096> recv_buffer;
    bool stopped;
};

/**
 * Class to connect to a given local socket
 */
class LocalClient
{
public:
    /**
     * Constructor for Local client
     * @param logHandler reference to the parent LogHandler
     * @param client_io io_service instance for client socket
     * @param socketFileName Path of the local socket-file
     */
    LocalClient(PacketLogHandler &logHandler,
            boost::asio::io_service &client_io,
            const std::string &socketFileName)
        : pktLogger(logHandler), clientSocket(client_io),
          remoteEndpoint(socketFileName), stopped(false),
          connected(false), pendingData(false) {
    }
    /**
     * Connect to a given local socket and export events
     */
    void run();
    /**
     * Stop Local socket
     */
    void stop() {
        stopped = true;
    }

private:
    PacketLogHandler &pktLogger;
    boost::asio::local::stream_protocol::socket clientSocket;
    boost::asio::local::stream_protocol::endpoint remoteEndpoint;
    boost::array<unsigned char, 4096> send_buffer;
    bool stopped;
    bool connected;
    bool pendingData;
    static const unsigned maxEventsPerBuffer=10;
};

/**
 * Class to hold the UDP listener and the packet decoder
 */
class PacketLogHandler {
public:
    /**
     * Constructor for PacketLogHandler
     * @param _io reference to IO service to handle server
     * @param _clientio reference to IO service to handle client
     */
    PacketLogHandler(boost::asio::io_service &_io,
            boost::asio::io_service &_clientio):server_io(_io),
            client_io(_clientio), stopped(false), throttleActive(false){}
    /**
     * set IPv4 listening address for the socket
     * @param _addr IPv4 address
     * @param _port UDP port number
     */
    void setAddress(boost::asio::ip::address &_addr, uint16_t _port=6081)
    { port = _port; addr = _addr; }
    /**
     * set socketfile path for the unix client socket
     * @param sockfilePath file system path for notification socket
     */
    void setNotifSock(const std::string &sockfilePath)
    { packetEventNotifSock = sockfilePath; }
    /**
     * Start packet logging
     */
    bool startListener();
    /**
     * Start exporter
     */
    bool startExporter();
    /**
     * Stop packet logging
     */
    void stopListener();
    /**
     * Stop exporter
     */
    void stopExporter();
    /**
     * Call packet decoder as an async callback
     * @param buf packet buffer
     * @param length total length of packet
     */
    void parseLog(unsigned char *buf , std::size_t length);

private:
    boost::asio::io_service &server_io;
    boost::asio::io_service &client_io;
    std::unique_ptr<UdpServer> socketListener;
    std::unique_ptr<LocalClient> exporter;
    PacketDecoder pktDecoder;
    boost::asio::ip::address addr;
    std::string packetEventNotifSock;
    uint16_t port;
    bool stopped;
    std::mutex qMutex;
    std::condition_variable cond;
    std::queue<PacketTuple> packetTupleQ;
    bool throttleActive;
    static const unsigned maxOutstandingEvents=30;
    friend UdpServer;
    friend LocalClient;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_PACKETLOGHANDLER_H */
