/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ofmutator.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/modb/Mutator.h"
#include "opflex/c/ofmutator_c.h"

using opflex::modb::Mutator;
using opflex::ofcore::OFFramework;

ofstatus ofmutator_create(offramework_p framework,
                          const char* owner,
                          /* out */ ofmutator_p* mutator) {
    ofstatus status = OF_ESUCCESS;
    Mutator* obj = NULL;

    try {
        if (mutator == NULL || *mutator != NULL) {
            status = OF_EINVALID_ARG;
            goto done;
        }

        obj = new (std::nothrow) Mutator(*(OFFramework*)framework,
                                         owner);
        if (obj == NULL) {
            status = OF_EMEMORY;
            goto done;
        }

        *mutator = (ofmutator_p)obj;
    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }

 done:
    if (OF_IS_FAILURE(status)) {
        if (obj != NULL) delete obj;
        if (mutator != NULL) *mutator = NULL;
    }

    return status;
}

ofstatus ofmutator_destroy(/* out */ ofmutator_p* mutator) {
    ofstatus status = OF_ESUCCESS;
    Mutator* obj = NULL;

    try {
        if (mutator == NULL || *mutator == NULL) {
            status = OF_EINVALID_ARG;
            goto done;
        }

        obj = (Mutator*)*mutator;
        delete obj;

        *mutator = NULL;

    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }

 done:
    return status;
}

ofstatus ofmutator_commit(ofmutator_p mutator) {
    ofstatus status = OF_ESUCCESS;
    Mutator* obj = NULL;

    try {
        if (mutator == NULL) {
            status = OF_EINVALID_ARG;
            goto done;
        }

        obj = (Mutator*)mutator;
        obj->commit();

    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }

 done:
    return status;
}
