/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for offramework wrapper.
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

#include <new>
#include <string>

#include "opflex/ofcore/OFFramework.h"
#include "opflex/c/offramework_c.h"

using opflex::ofcore::OFFramework;
using opflex::ofcore::PeerStatusListener;
using opflex::modb::ModelMetadata;

ofstatus offramework_create(/* out */ offramework_p* framework) {
    ofstatus status = OF_ESUCCESS;
    OFFramework* nf = NULL;

    try {
        if (framework == NULL || *framework != NULL) {
            status = OF_EINVALID_ARG;
            goto done;
        }

        nf = new (std::nothrow) OFFramework;
        if (nf == NULL) {
            status = OF_EMEMORY;
            goto done;
        }

        *framework = (offramework_p)nf;
    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }

 done:
    if (OF_IS_FAILURE(status)) {
        if (nf != NULL) delete nf;
        if (framework != NULL) *framework = NULL;
    }

    return status;
}

ofstatus offramework_destroy(/* out */ offramework_p* framework) {
    ofstatus status = OF_ESUCCESS;
    OFFramework* f = NULL;

    try {
        if (framework == NULL || *framework == NULL) {
            status = OF_EINVALID_ARG;
            goto done;
        }

        f = (OFFramework*)*framework;
        delete f;

        *framework = NULL;

    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }

 done:
    return status;
}

ofstatus offramework_set_model(offramework_p framework,
                               ofmetadata_p metadata) {
    ofstatus status = OF_ESUCCESS;
    OFFramework* f = NULL;

    try {
        if (framework == NULL || metadata == NULL) {
            status = OF_EINVALID_ARG;
            goto done;
        }
        
        f = (OFFramework*)framework;
        f->setModel(*(ModelMetadata*)metadata);

    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }

 done:
    return status;
}

ofstatus offramework_set_opflex_identity(offramework_p framework,
                                         const char* name,
                                         const char* domain) {
    ofstatus status = OF_ESUCCESS;
    OFFramework* f = NULL;

    try {
        if (framework == NULL || name == NULL || domain == NULL) {
            status = OF_EINVALID_ARG;
            goto done;
        }

        f = (OFFramework*)framework;
        std::string n(name), d(domain);
        f->setOpflexIdentity(n, d);

    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }

 done:
    return status;
}

ofstatus offramework_start(offramework_p framework) {
    ofstatus status = OF_ESUCCESS;
    OFFramework* f = NULL;

    try {
        if (framework == NULL) {
            status = OF_EINVALID_ARG;
            goto done;
        }

        f = (OFFramework*)framework;
        f->start();

    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }

 done:
    return status;
}

ofstatus offramework_stop(offramework_p framework) {
    ofstatus status = OF_ESUCCESS;
    OFFramework* f = NULL;

    try {
        if (framework == NULL) {
            status = OF_EINVALID_ARG;
            goto done;
        }

        f = (OFFramework*)framework;
        f->stop();

    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }

 done:
    return status;
}

ofstatus offramework_add_peer(offramework_p framework,
                              const char* hostname, int port) {
    ofstatus status = OF_ESUCCESS;
    OFFramework* f = NULL;

    try {
        if (framework == NULL || hostname == NULL) {
            status = OF_EINVALID_ARG;
            goto done;
        }

        f = (OFFramework*)framework;
        std::string h(hostname);
        f->addPeer(h, port);

    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }

 done:
    return status;
}

ofstatus
offramework_register_peerstatuslistener(offramework_p framework,
                                        ofpeerstatuslistener_p peerStatusObjptr)
{
    ofstatus status = OF_ESUCCESS;
    OFFramework* f = NULL;
    PeerStatusListener *ptr = (PeerStatusListener*)peerStatusObjptr;

    if (framework == NULL) {
        status = OF_EINVALID_ARG;
        goto done;
    }
    if (peerStatusObjptr == NULL) {
        status = OF_EINVALID_ARG;
        goto done;
    }
    try {
        f = (OFFramework*)framework;
        f->registerPeerStatusListener(ptr);
    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }
 done:
    return status;
}
