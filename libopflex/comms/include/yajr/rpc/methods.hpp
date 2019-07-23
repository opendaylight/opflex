/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _____COMMS__INCLUDE__YAJR__RPC__METHODS_HPP
#define _____COMMS__INCLUDE__YAJR__RPC__METHODS_HPP

#include <yajr/internal/comms.hpp> // for nextId()
#include <yajr/rpc/rpc.hpp>
#include <yajr/yajr.hpp>

#include <opflex/logging/internal/logging.hpp>

#include <rapidjson/document.h>

namespace yajr {

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
            extern MethodName transact;
            extern MethodName custom;

        } /* yajr::rpc::method namespace */

        template <MethodName * M>
        class OutReq : public yajr::rpc::OutboundRequest {

          public:

            OutReq(
                    yajr::rpc::PayloadGenerator const & payloadGenerator,
                    yajr::Peer const * peer
                ) : OutboundRequest(
                        payloadGenerator,
                        M,
                        /* do a little dance so that it technically would never
                         * throw, but at most crash with SIG11 (yet, this should
                         * never happen)
                         */
                        dynamic_cast< yajr::comms::internal::CommunicationPeer
                            const * >(peer)->nextId(),
                        peer)
                {}

            virtual char const * requestMethod() const {
                return M->s;
            }

            virtual MethodName * getMethodName() const {
                return M;
            }

        };

        template <MethodName * M>
        class InbReq : public yajr::rpc::InboundRequest {

          public:

            InbReq(
                    yajr::Peer const & peer,
                    rapidjson::Value const & params,
                    rapidjson::Value const & id
                )
                : InboundRequest(peer, params, id)
                {
                    VLOG(5)
                        << M->s
                    ;
                }

            virtual void process() const;

          protected:
            virtual char const * requestMethod() const { return NULL; }

        };

        template <MethodName * M>
        class InbRes : public yajr::rpc::InboundResult {

          public:

            InbRes(
                    yajr::Peer const & peer,
                    rapidjson::Value const & params,
                    rapidjson::Value const & id
                )
                : InboundResult(peer, params, id)
                {
                    VLOG(5)
                        << M->s
                    ;
                }

            virtual void process() const;

        };

        template <MethodName * M>
        class InbErr : public yajr::rpc::InboundError {

          public:

            InbErr(
                    yajr::Peer const & peer,
                    rapidjson::Value const & params,
                    rapidjson::Value const & id
                )
                : InboundError(peer, params, id)
                {
                    VLOG(3)
                        << M->s
                    ;
                }

            virtual void process() const;

        };

    } /* yajr::rpc namespace */

} /* yajr namespace */

#endif /* _____COMMS__INCLUDE__YAJR__RPC__METHODS_HPP */

