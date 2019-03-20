/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for SpanManager class.
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include <opflexagent/SpanManager.h>
#include <opflexagent/logging.h>
#include <modelgbp/span/Universe.hpp>
#include <modelgbp/domain/Config.hpp>

namespace opflexagent {

using namespace modelgbp::span;
using namespace std;
using boost::optional;
using opflex::modb::class_id_t;
using opflex::modb::URI;
using boost::posix_time::milliseconds;

SpanManager::SpanManager(opflex::ofcore::OFFramework& framework_):
             spanUniverseListener(*this), framework(framework_) {

}

void SpanManager::start() {
    LOG(DEBUG) << "starting span manager";
}
void SpanManager::stop() {
}

SpanManager::SpanUniverseListener::SpanUniverseListener(SpanManager& spanManager):
        spanmanager(spanManager) {}

SpanManager::SpanUniverseListener::~SpanUniverseListener() {}

void SpanManager::SpanUniverseListener::objectUpdated(class_id_t classId,
        const URI& uri) {
    LOG(DEBUG) << "Span Universe Listener obj updated";
    boost::optional<shared_ptr<modelgbp::span::Universe> > univ_opt =
        Universe::resolve(spanmanager.framework);
    if (univ_opt) {
        vector<shared_ptr<Session>> sessVec;
        univ_opt.get()->resolveSpanSession(sessVec);
        for (shared_ptr<Session> sess : sessVec) {
            LOG(DEBUG) << "update on session " << sess->getName().get();
            vector<shared_ptr<SrcGrp>> srcGrpVec;
            sess->resolveSpanSrcGrp(srcGrpVec);
            if (!srcGrpVec.empty()) {
                LOG(DEBUG) << "src grp vec size " << srcGrpVec.size();
            } else {
                LOG(DEBUG) << "src grp vec empty";
            }
        }
    }
}

}




