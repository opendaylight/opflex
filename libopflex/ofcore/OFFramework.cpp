/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OFFramework class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <cstdio>

#include <boost/assign.hpp>
#include <boost/foreach.hpp>
#include <boost/scoped_ptr.hpp>

#include "opflex/ofcore/OFFramework.h"
#include "opflex/engine/Processor.h"
#include "opflex/engine/Inspector.h"
#include "opflex/logging/internal/logging.hpp"
#include "opflex/modb/internal/ObjectStore.h"
#include "opflex/modb/mo-internal/StoreClient.h"

#include "ThreadManager.h"

namespace opflex {
namespace ofcore {

using namespace boost::assign;
using namespace opflex::modb;
using engine::internal::MOSerializer;
using boost::scoped_ptr;
using std::string;

class OFFramework::OFFrameworkImpl {
public:
    OFFrameworkImpl()
        : db(threadManager), processor(&db, threadManager), started(false),
          mode(opflex::ofcore::OFConstants::OpflexElementMode::STITCHED_MODE)
          {}
    ~OFFrameworkImpl() {}

    util::ThreadManager threadManager;
    modb::ObjectStore db;
    engine::Processor processor;
    scoped_ptr<engine::Inspector> inspector;
    uv_key_t mutator_key;
    bool started;
    opflex::ofcore::OFConstants::OpflexElementMode mode;
};

OFFramework::OFFramework() : pimpl(new OFFrameworkImpl()) {
    uv_key_create(&pimpl->mutator_key);
}

OFFramework::~OFFramework() {
    stop();
    uv_key_delete(&pimpl->mutator_key);
    delete pimpl;
}

const std::vector<int>& OFFramework::getVersion() {
    static const int v[3] = {SDK_PVERSION, SDK_SVERSION, SDK_IVERSION};
    static const std::vector<int> version(v, v+3);
    return version;
}

const std::string& OFFramework::getVersionStr() {
    static const std::string version(SDK_FULL_VERSION);
    return version;
}

void OFFramework::setModel(const modb::ModelMetadata& model) {
    pimpl->db.init(model);
}

bool OFFramework::setElementMode(
    opflex::ofcore::OFConstants::OpflexElementMode mode_) {
    if(!pimpl->started) {
        pimpl->mode = mode_;
        return true;
    } else {
        return false;
    }
}

void OFFramework::setPrrTimerDuration(const uint64_t duration) {
    pimpl->processor.setPrrTimerDuration(duration);
}

void OFFramework::start() {
    LOG(DEBUG) << "Starting OpFlex Framework";
    pimpl->started = true;
    pimpl->db.start();
    pimpl->processor.start(pimpl->mode);
    if (pimpl->inspector)
        pimpl->inspector->start();
}

MainLoopAdaptor* OFFramework::startSync() {
    MainLoopAdaptor* adaptor = pimpl->threadManager.getAdaptor();
    start();
    return adaptor;
}

void OFFramework::stop() {
    if (pimpl->inspector) {
        LOG(DEBUG) << "Stopping OpFlex Inspector";
        pimpl->inspector->stop();
        pimpl->inspector.reset();
    }
    if (pimpl->started) {
        LOG(DEBUG) << "Stopping OpFlex Framework";

        pimpl->processor.stop();
        pimpl->db.stop();
    }
    pimpl->threadManager.stop();
    pimpl->started = false;
}

void OFFramework::dumpMODB(const std::string& file) {
    MOSerializer& serializer = pimpl->processor.getSerializer();
    serializer.dumpMODB(file);
}

void OFFramework::dumpMODB(FILE* file) {
    MOSerializer& serializer = pimpl->processor.getSerializer();
    serializer.dumpMODB(file);
}

void OFFramework::prettyPrintMODB(std::ostream& output,
                                  bool tree,
                                  bool includeProps,
                                  bool utf8,
                                  size_t truncate) {
    MOSerializer& serializer = pimpl->processor.getSerializer();
    serializer.displayMODB(output, tree, includeProps, utf8, truncate);
}

void OFFramework::setOpflexIdentity(const std::string& name,
                                    const std::string& domain) {
    pimpl->processor.setOpflexIdentity(name, domain);
}

void OFFramework::setOpflexIdentity(const std::string& name,
                                    const std::string& domain,
                                    const std::string& location) {
    pimpl->processor.setOpflexIdentity(name, domain, location);
}

void OFFramework::enableSSL(const std::string& caStorePath,
                            bool verifyPeers) {
    pimpl->processor.enableSSL(caStorePath,
                               verifyPeers);
}

void OFFramework::enableSSL(const std::string& caStorePath,
                            const std::string& keyAndCertFilePath,
                            const std::string& passphrase,
                            bool verifyPeers) {
    pimpl->processor.enableSSL(caStorePath,
                               keyAndCertFilePath,
                               passphrase,
                               verifyPeers);
}

void OFFramework::enableInspector(const std::string& socketName) {
    pimpl->inspector.reset(new engine::Inspector(&pimpl->db));
    pimpl->inspector->setSocketName(socketName);
}

void OFFramework::addPeer(const std::string& hostname,
                          int port) {
    pimpl->processor.addPeer(hostname, port);
}

void OFFramework::registerPeerStatusListener(PeerStatusListener* listener) {
    pimpl->processor.registerPeerStatusListener(listener);
}

void OFFramework::getV4Proxy(boost::asio::ip::address_v4 &v4ProxyAddress ) {
    engine::internal::OpflexPool& pool = pimpl->processor.getPool();
    v4ProxyAddress = pool.getV4Proxy();
}

void OFFramework::getV6Proxy(boost::asio::ip::address_v4 &v6ProxyAddress ) {
    engine::internal::OpflexPool& pool = pimpl->processor.getPool();
    v6ProxyAddress = pool.getV6Proxy();
}

void OFFramework::getMacProxy(boost::asio::ip::address_v4 &macProxyAddress ) {
    engine::internal::OpflexPool& pool = pimpl->processor.getPool();
    macProxyAddress = pool.getMacProxy();
}

void MockOFFramework::setV4Proxy(const boost::asio::ip::address_v4& v4ProxyAddress ) {
    engine::internal::OpflexPool& pool = pimpl->processor.getPool();
    pool.setV4Proxy(v4ProxyAddress);
}

void MockOFFramework::setV6Proxy(const boost::asio::ip::address_v4& v6ProxyAddress ) {
    engine::internal::OpflexPool& pool = pimpl->processor.getPool();
    pool.setV6Proxy(v6ProxyAddress);
}

void MockOFFramework::setMacProxy(const boost::asio::ip::address_v4& macProxyAddress ) {
    engine::internal::OpflexPool& pool = pimpl->processor.getPool();
    pool.setMacProxy(macProxyAddress);
}

void MockOFFramework::start() {
    LOG(DEBUG) << "Starting OpFlex Framework";

    pimpl->db.start();
}

void MockOFFramework::stop() {
    LOG(DEBUG) << "Stopping OpFlex Framework";

    pimpl->db.stop();
    pimpl->threadManager.stop();
}

modb::ObjectStore& OFFramework::getStore() {
    return pimpl->db;
}

boost::optional<URI> OFFramework::getParent(class_id_t child_class,
                                        const URI& child) {
    boost::optional<URI> uri;
    try {
        uri = (pimpl->processor.getSystemClient()->
               getParent(child_class, child)).first;
    } catch(const std::out_of_range&) {
          LOG(DEBUG) << "Unable to find parent for " << child;
    }
    return uri;
}

void OFFramework::registerTLMutator(modb::Mutator& mutator) {
    uv_key_set(&pimpl->mutator_key, (void*)&mutator);
}
modb::Mutator& OFFramework::getTLMutator() {
    modb::Mutator* r = ((modb::Mutator*)uv_key_get(&pimpl->mutator_key));
    if (r == NULL)
        throw std::logic_error("No mutator currently active");
    return *r;
}
void OFFramework::clearTLMutator() {
    uv_key_set(&pimpl->mutator_key, NULL);
}

} /* namespace ofcore */
} /* namespace opflex */
