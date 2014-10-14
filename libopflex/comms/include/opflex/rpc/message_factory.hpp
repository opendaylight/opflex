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

#include <rapidjson/document.h>
#include <opflex/logging/internal/logging.hpp>
#include <opflex/comms/comms-internal.hpp>
#include <boost/function.hpp>
#include <rapidjson/writer.h>

namespace opflex {

namespace comms { namespace internal { class CommunicationPeer; }}

namespace rpc { class InboundMessage;
                class InboundRequest;
                class InboundResult;
                class InboundError;

                typedef rapidjson::Value::StringRefType const
                    MethodName;
                typedef rapidjson::Writer<rapidjson::StringBuffer>
                    SendHandler;
                typedef boost::function<bool (opflex::rpc::SendHandler &)>
                    PayloadGenerator;

                template <MethodName * M>
                class OutReq;

class MessageFactory {

  public:

    static opflex::rpc::InboundMessage * getInboundMessage(
            opflex::comms::internal::CommunicationPeer const & peer,
            rapidjson::Document const & doc);

    template <MethodName * M>
    static opflex::rpc::OutReq<M> * newReq(
            opflex::comms::internal::CommunicationPeer const & peer,
            opflex::rpc::PayloadGenerator payloadGenerator);

    static MethodName const * lookupMethod(char const * method);

  private:

    static opflex::rpc::InboundMessage * InboundMessage(
            opflex::comms::internal::CommunicationPeer const & peer,
            rapidjson::Document const & doc);

    static opflex::rpc::InboundRequest * InboundRequest(
            opflex::comms::internal::CommunicationPeer const & peer,
            rapidjson::Value const & params,
            char const * method,
            rapidjson::Value const & id);

    static opflex::rpc::InboundResult * InboundResult(
            opflex::comms::internal::CommunicationPeer const & peer,
            rapidjson::Value const & result,
            rapidjson::Value const & id);

    static opflex::rpc::InboundError * InboundError(
            opflex::comms::internal::CommunicationPeer const & peer,
            rapidjson::Value const & error,
            rapidjson::Value const & id);

};


}}

#endif /* _COMMS__INCLUDE__OPFLEX__RPC__MESSAGE_FACTORY_HPP */
