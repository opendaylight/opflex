/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

    std::string mstr(method);
    if (mstr == "echo") {
        return PERFECT_RET_VAL(yajr::rpc::method::echo);
    } else if (mstr == "send_identity") {
       return PERFECT_RET_VAL(yajr::rpc::method::send_identity);
    } else if (mstr == "policy_resolve") {
       return PERFECT_RET_VAL(yajr::rpc::method::policy_resolve);
    } else if (mstr == "policy_unresolve") {
       return PERFECT_RET_VAL(yajr::rpc::method::policy_unresolve);
    } else if (mstr == "policy_update") {
       return PERFECT_RET_VAL(yajr::rpc::method::policy_update);
    } else if (mstr == "endpoint_declare") {
       return PERFECT_RET_VAL(yajr::rpc::method::endpoint_declare);
    } else if (mstr == "endpoint_undeclare") {
       return PERFECT_RET_VAL(yajr::rpc::method::endpoint_undeclare);
    } else if (mstr == "endpoint_resolve") {
       return PERFECT_RET_VAL(yajr::rpc::method::endpoint_resolve);
    } else if (mstr == "endpoint_unresolve") {
       return PERFECT_RET_VAL(yajr::rpc::method::endpoint_unresolve);
    } else if (mstr == "endpoint_update") {
       return PERFECT_RET_VAL(yajr::rpc::method::endpoint_update);
    } else if (mstr == "state_report") {
       return PERFECT_RET_VAL(yajr::rpc::method::state_report);
    } else if (mstr == "custom") {
       return PERFECT_RET_VAL(yajr::rpc::method::custom);
    } else {
       return PERFECT_RET_VAL(yajr::rpc::method::unknown);
    }
