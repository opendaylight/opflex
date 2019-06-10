/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for SnatSource class.
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/SnatSource.h>
#include <opflexagent/SnatManager.h>

namespace opflexagent {

SnatSource::SnatSource(SnatManager* manager_)
    : manager(manager_) {}

SnatSource::~SnatSource() {}

void SnatSource::updateSnat(const Snat& snat) {
    manager->updateSnat(snat);
}

void SnatSource::removeSnat(const std::string& snatIp,
                            const std::string& uuid) {
    manager->removeSnat(snatIp, uuid);
}

} /* namespace opflexagent */
