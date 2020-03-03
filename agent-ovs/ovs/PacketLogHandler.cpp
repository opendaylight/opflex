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
#include <atomic>
#include <thread>
#include <chrono>

namespace opflexagent {

void LocalClient::run() {
    boost::asio::local::stream_protocol::endpoint invalidEndpoint("");
    if(remoteEndpoint==invalidEndpoint) {
        return;
    }
    LOG(INFO) << "PacketEventExporter started!";
    for(;;) {
        if(stopped) {
            boost::system::error_code ec;
            clientSocket.cancel(ec);
            clientSocket.close(ec);
            break;
        }
        if(!connected) {
            try {
                clientSocket.connect(remoteEndpoint);
                connected = true;
            } catch (std::exception &e) {
                LOG(ERROR) << "Failed to connect to packet event exporter socket:"
                        << e.what();
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
        }
        {
            std::unique_lock<std::mutex> lk(pktLogger.qMutex);
            pktLogger.cond.wait_for(lk, std::chrono::seconds(1),
                    [this](){return !this->pktLogger.packetTupleQ.empty();});
            if(!pktLogger.packetTupleQ.empty()) {
                StringBuffer buffer;
                Writer<StringBuffer> writer(buffer);
                unsigned event_count = 0;
                writer.StartArray();
                while((event_count < maxEventsPerBuffer) &&
                        !pktLogger.packetTupleQ.empty()) {
                    PacketTuple p = pktLogger.packetTupleQ.front();
                    p.serialize(writer);
                    pktLogger.packetTupleQ.pop();
                    event_count++;
                }
                writer.EndArray();
                memcpy(send_buffer.data(), buffer.GetString(),
                        (buffer.GetSize()>4096? 4096: buffer.GetSize()));
                pendingData = true;
            }
        }
        if(pendingData) {
            try {
                boost::asio::write(clientSocket,
                        boost::asio::buffer(send_buffer, 4096));
                pendingData = false;
            } catch (boost::system::system_error &bse ) {
                LOG(ERROR) << "Failed to write to socket " << bse.what();
                if(bse.code() !=
                   boost::system::errc::resource_unavailable_try_again) {
                    boost::system::error_code ec;
                    clientSocket.cancel(ec);
                    clientSocket.close(ec);
                    connected = false;
                }
                /*TODO: deserialize and requeue events*/
            }
        }
    }
}

bool PacketLogHandler::startListener()
{
    try {
        socketListener.reset(new UdpServer(*this, server_io, addr, port));
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

bool PacketLogHandler::startExporter()
{
    try {
        exporter.reset(new LocalClient(*this, client_io,
                packetEventNotifSock));
    } catch (boost::system::system_error& e) {
        LOG(ERROR) << "Could not create local client socket: "
                     << e.what();
        return false;
    }
    exporter->run();
    return true;
}

void PacketLogHandler::stopListener()
{
    LOG(INFO) << "PacketLogHandler stopped";
    if(socketListener) {
        socketListener->stop();
    }
    server_io.stop();
}

void PacketLogHandler::stopExporter()
{
    LOG(INFO) << "Exporter stopped";
    if(exporter) {
        exporter->stop();
    }
}

void UdpServer::handleReceive(const boost::system::error_code& error,
      std::size_t bytes_transferred) {
    if (!error || error == boost::asio::error::message_size)
    {
        uint32_t length = (bytes_transferred > 4096) ? 4096: bytes_transferred;
        boost::asio::io_service &server_io(serverSocket.get_io_service());
        server_io.post([=](){this->pktLogger.parseLog(recv_buffer.data(),
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
        {
            std::lock_guard<std::mutex> lk(qMutex);
            if(packetTupleQ.size() < maxOutstandingEvents) {
                if(throttleActive) {
                    LOG(ERROR) << "Queueing packet events";
                    throttleActive = false;
                }
                packetTupleQ.push(p.packetTuple);
                if(packetTupleQ.size()  == maxOutstandingEvents) {
                    LOG(ERROR) << "Max Event queue size ("
                               << maxOutstandingEvents
                               << ") throttling packet events";
                    throttleActive = true;
                }
            }
        }
        cond.notify_one();
    }
}

}
