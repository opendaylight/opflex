
/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for peerstatuslistener wrapper.
 *
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <new>
#include <string>

#include "opflex/ofcore/OFFramework.h"
#include "opflex/c/offramework_c.h"
#include "opflex/ofcore/PeerStatusListener.h"
#include "opflex/c/ofpeerstatuslistener_c.h"

using std::string;
using opflex::ofcore::OFFramework;
using opflex::modb::ModelMetadata;
using opflex::ofcore::PeerStatusListener;

class CPeerStatusListener : public PeerStatusListener {
public:
    CPeerStatusListener(void* user_data_,
                        ofpeerstatus_peer_p peer_callback_,
                        ofpeerstatus_health_p health_callback_)
        : user_data(user_data_),
          peer_callback(peer_callback_),
          health_callback(health_callback_) {}

    virtual ~CPeerStatusListener() { }

    virtual void peerStatusUpdated(const string& peerHostname, 
                                   int peerPort, PeerStatus peerStatus);
    virtual void healthUpdated(Health health);

    void* user_data;
    ofpeerstatus_peer_p peer_callback;
    ofpeerstatus_health_p health_callback;
};

void CPeerStatusListener::peerStatusUpdated(const string& peerHostname, 
                                            int peerPort, 
                                            PeerStatus peerconnStatus) 
{
    int peerconn_status_code;
    switch (peerconnStatus) {
    case PeerStatusListener::DISCONNECTED:
        peerconn_status_code = OF_PEERSTATUS_DISCONNECTED;
        break;
    case PeerStatusListener::CONNECTING:
        peerconn_status_code = OF_PEERSTATUS_CONNECTING;
        break;
    case PeerStatusListener::CONNECTED:
        peerconn_status_code = OF_PEERSTATUS_CONNECTED;
        break;
    case PeerStatusListener::READY:
        peerconn_status_code = OF_PEERSTATUS_READY;
        break;
    case PeerStatusListener::CLOSING:
        peerconn_status_code = OF_PEERSTATUS_CLOSING;
        break;
    default:
        peerconn_status_code = OF_PEERSTATUS_ERROR;
        break;
    }

    if (peer_callback != NULL) {
        peer_callback(user_data,
                      peerHostname.c_str(),
                      peerPort,
                      peerconn_status_code);
    }
}

void CPeerStatusListener::healthUpdated(Health connpoolhealth) 
{
    int connpool_status_code;
    switch (connpoolhealth) {
    case PeerStatusListener::DOWN:
        connpool_status_code = OF_POOLHEALTH_DOWN;
        break;
    case PeerStatusListener::DEGRADED:
        connpool_status_code = OF_POOLHEALTH_DEGRADED;
        break;
    case PeerStatusListener::HEALTHY:
        connpool_status_code = OF_POOLHEALTH_HEALTHY;
        break;
    default:
        connpool_status_code = OF_POOLHEALTH_ERROR;
        break;
    }

    if (health_callback != NULL) {
        health_callback(user_data, connpool_status_code);
    }
}

ofstatus ofpeerstatuslistener_create(void* user_data,
                                     ofpeerstatus_peer_p peer_callback,
                                     ofpeerstatus_health_p health_callback,
                                     /* out */ ofpeerstatuslistener_p *obj) {
    ofstatus status = OF_ESUCCESS;
    CPeerStatusListener *peerStatusObjptr =  NULL;

    if (obj == NULL) {
        status = OF_EINVALID_ARG;
        goto done;
    }
    peerStatusObjptr =
        new (std::nothrow) CPeerStatusListener(user_data,
                                               peer_callback,
                                               health_callback);
    if (peerStatusObjptr == NULL) {
        status = OF_EMEMORY;
        goto done;
    }
    *obj = (ofpeerstatuslistener_p)peerStatusObjptr;
 done:
    return status;
}

ofstatus
ofpeerstatuslistener_destroy(ofpeerstatuslistener_p *obj)
{
    ofstatus status = OF_ESUCCESS;
    CPeerStatusListener *ptr = NULL;

    try {
        if ((obj == NULL) || (*obj == NULL)) {
            status = OF_EINVALID_ARG;
            goto done;
        }
        ptr = (CPeerStatusListener*)*obj;
        delete ptr;

        *obj = NULL;
    } catch (...) {
        status = OF_EFAILED;
        goto done;
    }
 done:
    return status;
}
