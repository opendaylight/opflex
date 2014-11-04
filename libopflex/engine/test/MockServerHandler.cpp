/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for MockServerHandler
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#include "MockServerHandler.h"

namespace opflex {
namespace engine {
namespace internal {

using rapidjson::Value;

void MockServerHandler::connected() {
    // XXX - TODO
}

void MockServerHandler::disconnected() {
    // XXX - TODO
}

void MockServerHandler::ready() {
    // XXX - TODO
}

void MockServerHandler::handleSendIdentityReq(const Value& payload) {
    // XXX - TODO
}

void MockServerHandler::handlePolicyResolveReq(const Value& payload) {
    // XXX - TODO
}

void MockServerHandler::handlePolicyUnresolveReq(const rapidjson::Value& payload) {
    // XXX - TODO
}

void MockServerHandler::handleEPDeclareReq(const rapidjson::Value& payload) {
    // XXX - TODO
}

void MockServerHandler::handleEPUndeclareReq(const rapidjson::Value& payload) {
    // XXX - TODO
}

void MockServerHandler::handleEPResolveReq(const rapidjson::Value& payload) {
    // XXX - TODO
}

void MockServerHandler::handleEPUnresolveReq(const rapidjson::Value& payload) {
    // XXX - TODO
}

void MockServerHandler::handleEPUpdateRes(const rapidjson::Value& payload) {
    // nothing to do
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
