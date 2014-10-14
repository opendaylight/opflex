/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Main implementation for OVS agent
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <iostream>
#include <modelgbp/metadata/metadata.hpp>
#include <modelgbp/dmtree/Root.hpp>
#include <opflex/ofcore/OFFramework.h>

using opflex::modb::ModelMetadata;
using opflex::modb::Mutator;
using opflex::ofcore::OFFramework;

int main(int argc, char** argv) {
    OFFramework::defaultInstance().setModel(modelgbp::getMetadata());
    OFFramework::defaultInstance().start();

    {
        Mutator mutator("testowner");
        boost::shared_ptr<modelgbp::dmtree::Root> root = 
            modelgbp::dmtree::Root::createRootElement();
        boost::shared_ptr<modelgbp::policy::Universe> puniverse = 
            root->addPolicyUniverse();
        boost::shared_ptr<modelgbp::policy::Space> nspace = 
            puniverse->addPolicySpace("testspace");
        mutator.commit();
    }

    boost::optional<boost::shared_ptr<modelgbp::policy::Space> > rspace =
        modelgbp::policy::Space::resolve("testspace");
    std::cout << "Got testspace " << rspace.get()->getName().get() << std::endl;
    OFFramework::defaultInstance().stop();
}
