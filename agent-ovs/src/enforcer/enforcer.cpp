/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include <enforcerImpl.h>

#include <internal/modb.h>
#include <internal/model.h>

using namespace std;

namespace opflex {
namespace enforcer {

static Enforcer *enforcerSingleton;


/*
 *  Enforcer static methods
 */

Enforcer *Enforcer::getInstance() {
    return enforcerSingleton;
}

void Enforcer::Init() {
    enforcerSingleton = new EnforcerImpl();
}

void Enforcer::Start() {
    dynamic_cast<EnforcerImpl *>(getInstance())->Start();
}

void Enforcer::Stop() {
    dynamic_cast<EnforcerImpl *>(getInstance())->Stop();
}


/*
 * Implementation of EnforcerImpl
 */

void EnforcerImpl::Start() {
	flowManager->Start();
    invtManager->Start();
    for (modb::class_id_t id = 0; id < model::CLASS_ID_END; ++id) {
        modb::RegisterListener(id, *this);
    }
}

void EnforcerImpl::Stop() {
    for (modb::class_id_t id = 0; id < model::CLASS_ID_END; ++id) {
        modb::UnregisterListener(id, *this);
    }
    flowManager->Stop();
    invtManager->Stop();
}

void
EnforcerImpl::objectUpdated(modb::class_id_t cid, const modb::URI& uri) {
    invtManager->Update(cid, uri);
    flowManager->Generate();   // TODO should happen indirectly thru Update()
    return;
}

}   // namespace enforcer
}   // namespace opflex


