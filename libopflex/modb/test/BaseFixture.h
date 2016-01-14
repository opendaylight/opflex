/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for base fixture
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef MODB_TEST_BASEFIXTURE_H
#define MODB_TEST_BASEFIXTURE_H

#include <vector>
#include <boost/assign/list_of.hpp>

#include "opflex/modb/internal/ObjectStore.h"

#include "MDFixture.h"

namespace opflex {
namespace modb {

using namespace boost::assign;

/**
 * A fixture that adds an object store
 */
class BaseFixture : public MDFixture {
public:
    BaseFixture()
        : MDFixture(), db(threadManager) {
        db.init(md);
        db.start();
        client1 = &db.getStoreClient("owner1");
        client2 = &db.getStoreClient("owner2");
    }

    ~BaseFixture() {
        db.stop();
    }

    util::ThreadManager threadManager;
    ObjectStore db;
    mointernal::StoreClient* client1;
    mointernal::StoreClient* client2;
};

} /* namespace modb */
} /* namespace opflex */

#endif /* MODB_TEST_BASEFIXTURE_H */
