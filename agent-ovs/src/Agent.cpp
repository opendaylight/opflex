/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for Agent class
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "Agent.h"
#include <modelgbp/dmtree/Root.hpp>

namespace ovsagent {

using opflex::modb::ModelMetadata;
using opflex::modb::Mutator;
using opflex::ofcore::OFFramework;
using boost::shared_ptr;

Agent::Agent(OFFramework& framework_) 
    : framework(framework_) {
    start();
}

Agent::~Agent() {
    stop();
}

void Agent::start() {
    framework.setModel(modelgbp::getMetadata());
    framework.start();

    Mutator mutator(framework, "testowner");
    shared_ptr<modelgbp::dmtree::Root> root = 
        modelgbp::dmtree::Root::createRootElement(framework);
    root->addPolicyUniverse();
    root->addRelatorUniverse();
    mutator.commit();

}

void Agent::stop() {
    framework.stop();
}

} /* namespace ovsagent */
