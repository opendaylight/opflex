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
#include <opflexagent/logging.h>

#ifdef HAVE_LIBNFCT
#include <libnetfilter_conntrack/libnetfilter_conntrack.h>
#endif /* HAVE_LIBCFCT */

#include <limits>

namespace opflexagent {

CtZoneManager::CtZoneManager(IdGenerator& gen_)
    : minId(1), maxId(65534), useNetLink(false), gen(gen_) { }

CtZoneManager::~CtZoneManager() {

}

#ifdef HAVE_LIBNFCT
class NfCtP {
public:
    NfCtP() {
        handle = nfct_open(CONNTRACK, 0);
        if (!handle) {
            LOG(ERROR) << "Failed to open netlink handle";
        }
    }
    ~NfCtP() {
        nfct_close(handle);
    }

    struct nfct_handle* handle;
};

struct delete_ctx {
    NfCtP cth;
    NfCtP ith;
    uint16_t zoneId;
};

static int delete_cb(enum nf_conntrack_msg_type type,
                     struct nf_conntrack *ct,
                     void *data)
{
    struct delete_ctx* ctx = (struct delete_ctx*)data;

    if (ctx->zoneId != nfct_get_attr_u16(ct, ATTR_ZONE))
        return NFCT_CB_CONTINUE;

    int res = nfct_query(ctx->ith.handle, NFCT_Q_DESTROY, ct);
    if (res < 0) {
        LOG(ERROR) << "Failed to delete conntrack entry "
                   << nfct_get_attr_u32(ct, ATTR_ID)
                   << ": " << res;
    }

    return NFCT_CB_CONTINUE;
}
#endif /* HAVE_LIBNFCT */

bool CtZoneManager::ctZoneAllocHook(const std::string&, uint32_t id) {
#ifdef HAVE_LIBNFCT
    LOG(DEBUG) << "Clearing connection tracking zone " << id;

    delete_ctx ctx;
    if (!ctx.cth.handle || !ctx.ith.handle)
        return false;
    ctx.zoneId = static_cast<uint16_t>(id);

    nfct_callback_register(ctx.cth.handle, NFCT_T_ALL, delete_cb, &ctx);

    typedef decltype(&nfct_filter_dump_destroy) destroy_t;
    std::unique_ptr<struct nfct_filter_dump, destroy_t>
        filter_dump(nfct_filter_dump_create(), nfct_filter_dump_destroy);
    if (!filter_dump) {
        LOG(ERROR) << "Could not create nfct_filter_dump";
        return false;
    }

    nfct_filter_dump_set_attr_u8(filter_dump.get(),
                                 NFCT_FILTER_DUMP_L3NUM,
                                 AF_INET);
    int res = nfct_query(ctx.cth.handle, NFCT_Q_DUMP_FILTER, filter_dump.get());
    if (res < 0) {
        LOG(ERROR) << "Failed to query connection tracking for zone "
                   << id << ": " << res;
        return false;
    }

#endif /* HAVE_LIBNFCT */

    return true;
}

void CtZoneManager::setCtZoneRange(uint16_t min, uint16_t max) {
    if (min >= 1) minId = min;
    if (max < std::numeric_limits<uint16_t>::max()) maxId = max;
}

void CtZoneManager::enableNetLink(bool useNetLink_) {
    useNetLink = useNetLink_;
}

void CtZoneManager::init(const std::string& nmspc_) {
    using std::placeholders::_1;
    using std::placeholders::_2;

    nmspc = nmspc_;
    gen.initNamespace(nmspc, minId, maxId);
    if (useNetLink) {
#ifdef HAVE_LIBNFCT
        IdGenerator::alloc_hook_t
            hook(std::bind(&CtZoneManager::ctZoneAllocHook, this, _1, _2));
        gen.setAllocHook(nmspc, hook);
#else /* HAVE_LIBNFCT */
        LOG(WARNING) << "Netlink library not available and connection "
            "tracking is enabled.";
#endif /* HAVE_LIBNFCT */
    }
}

uint16_t CtZoneManager::getId(const std::string& str) {
    return static_cast<uint16_t>(gen.getId(nmspc, str));
}

void CtZoneManager::erase(const std::string& str) {
    gen.erase(nmspc, str);
}

} /* namespace opflexagent */
