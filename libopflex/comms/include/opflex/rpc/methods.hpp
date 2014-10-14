/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _____COMMS__INCLUDE__OPFLEX__RPC__METHODS_HPP
#define _____COMMS__INCLUDE__OPFLEX__RPC__METHODS_HPP

#include <rapidjson/document.h>
#include <opflex/rpc/rpc.hpp>

namespace opflex {

    namespace rpc {

        typedef rapidjson::Value::StringRefType const MethodName;

        namespace method {

            extern MethodName unknown;
            extern MethodName echo;
            extern MethodName send_identity;
            extern MethodName policy_resolve;
            extern MethodName policy_unresolve;
            extern MethodName policy_update;
            extern MethodName endpoint_declare;
            extern MethodName endpoint_undeclare;
            extern MethodName endpoint_resolve;
            extern MethodName endpoint_unresolve;
            extern MethodName endpoint_update;
            extern MethodName state_report;

        }

        template <MethodName * M>
        class OutReq : public opflex::rpc::OutboundRequest {

          public:

            OutReq(
                    opflex::comms::internal::CommunicationPeer const & peer,
                    opflex::rpc::PayloadGenerator const & payloadGenerator

                ) : OutboundRequest(
                        peer,
                        payloadGenerator,
                        M,
                        peer.nextId())
                {}

            virtual char const * requestMethod() const {
                return M->s;
            }

            virtual MethodName * getMethodName() const {
                return M;
            }

        };

        template <MethodName * M>
        class InbReq : public opflex::rpc::InboundRequest {

          public:

            InbReq(
                    opflex::comms::internal::CommunicationPeer const & peer,
                    rapidjson::Value const & params,
                    rapidjson::Value const & id
                )
                : InboundRequest(peer, params, id)
                {
                    LOG(DEBUG) << M->s;
                }

            virtual void process() const;

          protected:
            virtual char const * requestMethod() const { return NULL; }

        };

        template <MethodName * M>
        class InbRes : public opflex::rpc::InboundResult {

          public:

            InbRes(
                    opflex::comms::internal::CommunicationPeer const & peer,
                    rapidjson::Value const & params,
                    rapidjson::Value const & id
                )
                : InboundResult(peer, params, id)
                {
                    LOG(DEBUG) << M->s;
                }

            virtual void process() const;

        };

        template <MethodName * M>
        class InbErr : public opflex::rpc::InboundError {

          public:

            InbErr(
                    opflex::comms::internal::CommunicationPeer const & peer,
                    rapidjson::Value const & params,
                    rapidjson::Value const & id
                )
                : InboundError(peer, params, id)
                {
                    LOG(DEBUG) << M->s;
                }

            virtual void process() const;

        };

    }

}

#endif /* _____COMMS__INCLUDE__OPFLEX__RPC__METHODS_HPP */
