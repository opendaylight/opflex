/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

    switch (fnv_1a_64::hash_runtime(method)) {
        case fnv_1a_64::hash_const("echo"):
            return PERFECT_RET_VAL(yajr::rpc::method::echo);
            break;

        case fnv_1a_64::hash_const("send_identity"):
            return PERFECT_RET_VAL(yajr::rpc::method::send_identity);
            break;

        case fnv_1a_64::hash_const("policy_resolve"):
            return PERFECT_RET_VAL(yajr::rpc::method::policy_resolve);
            break;

        case fnv_1a_64::hash_const("policy_unresolve"):
            return PERFECT_RET_VAL(yajr::rpc::method::policy_unresolve);
            break;

        case fnv_1a_64::hash_const("policy_update"):
            return PERFECT_RET_VAL(yajr::rpc::method::policy_update);
            break;

        case fnv_1a_64::hash_const("endpoint_declare"):
            return PERFECT_RET_VAL(yajr::rpc::method::endpoint_declare);
            break;

        case fnv_1a_64::hash_const("endpoint_undeclare"):
            return PERFECT_RET_VAL(yajr::rpc::method::endpoint_undeclare);
            break;

        case fnv_1a_64::hash_const("endpoint_resolve"):
            return PERFECT_RET_VAL(yajr::rpc::method::endpoint_resolve);
            break;

        case fnv_1a_64::hash_const("endpoint_unresolve"):
            return PERFECT_RET_VAL(yajr::rpc::method::endpoint_unresolve);
            break;

        case fnv_1a_64::hash_const("endpoint_update"):
            return PERFECT_RET_VAL(yajr::rpc::method::endpoint_update);
            break;

        case fnv_1a_64::hash_const("state_report"):
            return PERFECT_RET_VAL(yajr::rpc::method::state_report);
            break;

        case fnv_1a_64::hash_const("transact"):
            return PERFECT_RET_VAL(yajr::rpc::method::transact);
            break;

        case fnv_1a_64::hash_const("custom"):
            return PERFECT_RET_VAL(yajr::rpc::method::custom);
            break;

        default:
            return PERFECT_RET_VAL(yajr::rpc::method::unknown);
    }
