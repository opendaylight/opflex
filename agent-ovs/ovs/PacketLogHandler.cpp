/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for PacketLogHandler class
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#include "PacketLogHandler.h"
#include "PacketDecoderLayers.h"
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/ip/v6_only.hpp>

namespace opflexagent {

bool PacketLogHandler::start()
{
    try {
        socketListener.reset(new UdpServer(*this, packet_io, addr, port));
    } catch (boost::system::system_error& e) {
        LOG(ERROR) << "Could not bind to socket: "
                     << e.what();
        return false;
    }
    pktDecoder.configure();
    socketListener->startReceive();
    LOG(INFO) << "PacketLogHandler started!";
    return true;
}

void PacketLogHandler::stop()
{
    LOG(INFO) << "PacketLogHandler stopped";
    if(socketListener) {
        socketListener->stop();
    }
    packet_io.stop();
}

void UdpServer::handleReceive(const boost::system::error_code& error,
      std::size_t bytes_transferred) {
    if (!error || error == boost::asio::error::message_size)
    {
        uint32_t length = (bytes_transferred > 4096) ? 4096: bytes_transferred;
        boost::asio::io_service &packet_io(serverSocket.get_io_service());
        packet_io.post([=](){this->pktLogger.parseLog(recv_buffer.data(),
                length);});
    }
    if(!stopped) {
        startReceive();
    }
}

void PacketLogHandler::parseLog(unsigned char *buf , std::size_t length) {
    std::string parsedLog;
    ParseInfo p(&pktDecoder);
    int ret = pktDecoder.decode(buf, length, p);
    if(ret) {
        LOG(ERROR) << "Error parsing packet " << ret;
        std::stringstream str;
        int maxPrintLen = (length < 100)? length: 100;
        for(int i =0; i < maxPrintLen; i++) {
            if(i%32 == 0){
                str << std::endl;
            }
            str << std::hex << (uint32_t)buf[i] << " ";
        }
        LOG(ERROR) << str.str();
    } else {
        LOG(INFO) << p.parsedString;
    }
}

}
