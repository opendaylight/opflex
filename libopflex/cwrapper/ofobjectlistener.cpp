/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ofobjectlistener.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/modb/ObjectListener.h"
#include "opflex/c/ofobjectlistener_c.h"

using opflex::modb::ObjectListener;
using opflex::modb::URI;

class CObjectListener : public ObjectListener {
public:
    CObjectListener(void* user_data_, ofnotify_p callback_)
        : user_data(user_data_), callback(callback_) {}

    virtual ~CObjectListener() {}

  virtual void objectUpdated(opflex::modb::class_id_t class_id, const URI& uri) {
        callback(user_data, class_id, (ofuri_p*)&uri);
    }

    void* user_data;
    ofnotify_p callback;
};

ofstatus ofobjectlistener_create(void* user_data,
                                 ofnotify_p callback,
                                 /* out */ ofobjectlistener_p* listener) {
    ofstatus status = OF_ESUCCESS;
    CObjectListener* nl = NULL;

    try {
        if (listener == NULL || *listener != NULL ||
            callback == NULL) {
            status = OF_EINVALID_ARG;
            goto done;
        }

        nl = new (std::nothrow) CObjectListener(user_data, callback);
        if (nl == NULL) {
            status = OF_EMEMORY;
            goto done;
        }

        *listener = (ofobjectlistener_p)nl;
    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }

 done:
    if (OF_IS_FAILURE(status)) {
        if (nl != NULL) delete nl;
        if (listener != NULL) *listener = NULL;
    }

    return status;
}

ofstatus ofobjectlistener_destroy(/* out */ ofobjectlistener_p* listener) {
    ofstatus status = OF_ESUCCESS;
    CObjectListener* l = NULL;

    try {
        if (listener == NULL || *listener == NULL) {
            status = OF_EINVALID_ARG;
            goto done;
        }

        l = (CObjectListener*)*listener;
        delete l;

        *listener = NULL;

    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }

 done:
    return status;
}
