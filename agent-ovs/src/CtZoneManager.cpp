/*
 * Implementation of CtZoneManager class
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "CtZoneManager.h"
#include "IdGenerator.h"
#include "logging.h"

#include <netlink/netfilter/ct.h>
#include <netlink/netfilter/nfnl.h>

#include <boost/bind.hpp>

namespace ovsagent {

CtZoneManager::CtZoneManager(IdGenerator& gen_)
    : minId(1), maxId(65534), useNetLink(false), gen(gen_) { }

CtZoneManager::~CtZoneManager() {

}

class NlCtP {
public:
    NlCtP(struct nl_sock* sock) : ct(nfnl_ct_alloc()) {}
    ~NlCtP() {
        nfnl_ct_put(ct);
    }

    struct nfnl_ct* ct;
};

bool CtZoneManager::ctZoneAllocHook(const std::string&, uint32_t id) {
    uint16_t zoneId = static_cast<uint16_t>(id);
    LOG(DEBUG) << "Clearing connection tracking zone " << zoneId;

    NlCtP ct(nlSock.sock);
    nfnl_ct_set_zone(ct.ct, zoneId);
    int r = nfnl_ct_del(nlSock.sock, ct.ct, 0);
    if (r != 0) {
        LOG(ERROR) << "Failed to clear connection tracking zone " << zoneId
                   << ": " << nl_geterror(r);
        return false;
    }

    return true;
}

void CtZoneManager::setCtZoneRange(uint16_t min, uint16_t max) {
    if (min >= 1) minId = min;
    if (max < ~static_cast<uint16_t>(0)) maxId = max;
}

void CtZoneManager::enableNetLink(bool useNetLink_) {
    useNetLink = useNetLink_;
}

void CtZoneManager::init(const std::string& nmspc_) {
    nmspc = nmspc_;
    gen.initNamespace(nmspc, minId, maxId);
    if (useNetLink) {
        IdGenerator::alloc_hook_t
            hook(boost::bind(&CtZoneManager::ctZoneAllocHook, this, _1, _2));
        gen.setAllocHook(nmspc, hook);

        int r = nfnl_connect(nlSock.sock);
        if (r != 0) {
            LOG(ERROR) << "Failed to connect to netlink netfilter: "
                       << nl_geterror(r);
        }
    }
}

uint16_t CtZoneManager::getId(const std::string& str) {
    return static_cast<uint16_t>(gen.getId(nmspc, str));
}

void CtZoneManager::erase(const std::string& str) {
    gen.erase(nmspc, str);
}

} /* namespace ovsagent */
