/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _COMMS__INCLUDE__OPFLEX__RPC__MESSAGE_FACTORY_HPP
#define _COMMS__INCLUDE__OPFLEX__RPC__MESSAGE_FACTORY_HPP

#include <yajr/rpc/send_handler.hpp>
#include <yajr/internal/comms.hpp>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <boost/function.hpp>

namespace yajr {

class Peer;

namespace rpc {

                class InboundMessage;
                class InboundRequest;
                class InboundResult;
                class InboundError;

                typedef rapidjson::Value::StringRefType const
                    MethodName;
                typedef rapidjson::Writer< ::yajr::internal::StringQueue >
                    SendHandler;
                typedef boost::function< bool (::yajr::rpc::SendHandler &) >
                    PayloadGenerator;

                template <MethodName * M>
                class OutReq;

class MessageFactory {

  public:

    static yajr::rpc::InboundMessage * getInboundMessage(
            yajr::Peer const & peer,
            rapidjson::Document const & doc);

    template <MethodName * M>
    static yajr::rpc::OutReq<M> * newReq(
            yajr::rpc::PayloadGenerator const & payloadGenerator,
            yajr::Peer const * peer = NULL);

    static MethodName const * lookupMethod(char const * method);

  private:

    static yajr::rpc::InboundMessage * InboundMessage(
            yajr::Peer const & peer,
            rapidjson::Document const & doc);

    static yajr::rpc::InboundRequest * InboundRequest(
            yajr::Peer const & peer,
            rapidjson::Value const & params,
            char const * method,
            rapidjson::Value const & id);

    static yajr::rpc::InboundResult * InboundResult(
            yajr::Peer const & peer,
            rapidjson::Value const & result,
            rapidjson::Value const & id);

    static yajr::rpc::InboundError * InboundError(
            yajr::Peer const & peer,
            rapidjson::Value const & error,
            rapidjson::Value const & id);

};

} /* yajr::rpc namespace */
} /* yajr namespace */

#endif /* _COMMS__INCLUDE__OPFLEX__RPC__MESSAGE_FACTORY_HPP */
