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

#include "config.h"

#include <boost/assign.hpp>

#include "opflex/ofcore/OFFramework.h"
#include "opflex/engine/Processor.h"
#include "opflex/modb/internal/ObjectStore.h"
#include "opflex/logging/internal/logging.hpp"
#include "opflex/comms/comms.hpp"

namespace opflex {
namespace ofcore {

using namespace boost::assign;

class OFFramework::OFFrameworkImpl {
public:
    OFFrameworkImpl() : processor(&db) { } 
    ~OFFrameworkImpl() {}

    modb::ObjectStore db;
    engine::Processor processor;
    uv_key_t mutator_key;
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

void OFFramework::start() {
    LOG(DEBUG) << "Starting OpFlex Framework";

    pimpl->db.start();
    pimpl->processor.start();
}

void OFFramework::stop() {
    LOG(DEBUG) << "Stopping OpFlex Framework";

    pimpl->processor.stop();
    pimpl->db.stop();
}

void OFFramework::setOpflexIdentity(const std::string& name,
                                    const std::string& domain) {
    pimpl->processor.setOpflexIdentity(name, domain);
}

void OFFramework::addPeer(const std::string& hostname,
                          int port) {
    pimpl->processor.addPeer(hostname, port);
}

void MockOFFramework::start() {
    LOG(DEBUG) << "Starting OpFlex Framework";

    pimpl->db.start();
}

void MockOFFramework::stop() {
    LOG(DEBUG) << "Stopping OpFlex Framework";

    pimpl->db.stop();
}

modb::ObjectStore& OFFramework::getStore() {
    return pimpl->db;
}

OFFramework& OFFramework::defaultInstance() {
    static OFFramework staticInstance;
    return staticInstance;
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
