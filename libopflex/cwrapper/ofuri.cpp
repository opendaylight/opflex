/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ofuri.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/modb/URI.h"
#include "opflex/c/ofuri_c.h"

using opflex::modb::URI;

ofstatus ofuri_get_str(ofuri_p uri, /* out */ const char** str) {
    ofstatus status = OF_ESUCCESS;
    URI* u = NULL;

    try {
        if (uri == NULL || str == NULL) {
            status = OF_EINVALID_ARG;
            goto done;
        }
        
        u = (URI*)uri;
        *str = u->toString().c_str();

    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }

 done:
    return status;
}

ofstatus ofuri_hash(ofuri_p uri, /* out */ size_t* hash_value) {
    ofstatus status = OF_ESUCCESS;
    URI* u = NULL;

    try {
        if (uri == NULL || hash_value == NULL) {
            status = OF_EINVALID_ARG;
            goto done;
        }
        
        u = (URI*)uri;
        *hash_value = opflex::modb::hash_value(*u);

    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }

 done:
    return status;
}
