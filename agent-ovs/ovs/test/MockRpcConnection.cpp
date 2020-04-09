/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "MockRpcConnection.h"

namespace opflexagent {

void ResponseDict::init() {
    uint64_t j = 1001;
    for (unsigned int i = 0 ; i < no_of_span_msgs; i++, j++) {
        d[i].GetAllocator().Clear();
        d[i].Parse(response[i].c_str());
        dict.emplace(j, i);
    }
    j = 2001;
    for (unsigned int i = no_of_span_msgs ; i < (no_of_span_msgs + no_of_netflow_msgs); i++, j++) {
        d[i].GetAllocator().Clear();
        d[i].Parse(response[i].c_str());
        dict.emplace(j, i);
    }
}

ResponseDict& ResponseDict::Instance() {
    static ResponseDict inst;
    if (!inst.isInitialized) {
        inst.init();
        inst.isInitialized = true;
    }
    return inst;
}

void MockRpcConnection::sendTransaction(const uint64_t& reqId, const list<JsonRpcTransactMessage>& requests, Transaction* trans) {
    ResponseDict& rDict = ResponseDict::Instance();
    auto itr = rDict.dict.find(reqId);
    if (itr != rDict.dict.end()) {
        trans->handleTransaction(reqId, rDict.d[itr->second]);
    } else {
        LOG(DEBUG) << "No response found for req " << reqId;
    }
}

}