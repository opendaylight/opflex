
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

using opflex::ofcore::OFFramework;
using opflex::modb::ModelMetadata;
using opflex::ofcore::PeerStatusListener;

// the two callbacks are unimplemented virtual functions in the base class,
// so we implement them in our derived class

class CPeerStatusListener : public PeerStatusListener {
public:
    ofpeerconnstatus_callback_p peerconnstatus_callback;
    ofconnpoolhealth_callback_p connpoolhealth_callback;

    virtual void peerStatusUpdated(const std::string& peerHostname, 
                           int peerPort, PeerStatus peerStatus);
    virtual void healthUpdated(Health health);
    // store the provided callbacks for use by member functions later
    CPeerStatusListener(ofpeerconnstatus_callback_p cpeerconnstatus_callback,
                      ofconnpoolhealth_callback_p cconnpoolhealth_callback) {
            peerconnstatus_callback = cpeerconnstatus_callback;
            connpoolhealth_callback = cconnpoolhealth_callback;
    }
    virtual ~CPeerStatusListener() { }
};


void CPeerStatusListener::peerStatusUpdated(const std::string& peerHostname, 
                           int peerPort, PeerStatus peerconnStatus) 
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
     // use the saved callback to get back into C user code
     // the c_str string must be less than 64 chars
     if (peerconnstatus_callback != NULL) {
          peerconnstatus_callback(peerHostname.c_str(),
                          peerPort,
                          peerconn_status_code);
     }
}

void CPeerStatusListener::healthUpdated(Health connpoolhealth) 
{
    int connpool_status_code;
    switch (connpoolhealth) {
    case PeerStatusListener::DOWN:
            connpool_status_code = OF_CONNHEALTH_DOWN;
            break;
    case PeerStatusListener::DEGRADED:
            connpool_status_code = OF_CONNHEALTH_DEGRADED;
            break;
    case PeerStatusListener::HEALTHY:
            connpool_status_code = OF_CONNHEALTH_HEALTHY;
            break;
    default:
            connpool_status_code = OF_CONNHEALTH_ERROR;
            break;
    }
     // use the saved callback to get back into C user code
    if (connpoolhealth_callback != NULL) {
        connpoolhealth_callback(connpool_status_code);
    }
}

ofstatus
offramework_create_peerstatus_listener(ofpeerstatuslistener_p *obj,
                         ofpeerconnstatus_callback_p cpeerconnstatus_callback,
                         ofconnpoolhealth_callback_p cconnpoolhealth_callback)
{
    ofstatus status = OF_ESUCCESS;
    CPeerStatusListener *peerStatusObjptr =  NULL;

    if (obj == NULL) {
       status = OF_EINVALID_ARG;
       goto done;
    }
    peerStatusObjptr =
            new (std::nothrow) CPeerStatusListener(cpeerconnstatus_callback,
                                                   cconnpoolhealth_callback);
    if (peerStatusObjptr == NULL) {
       status = OF_EMEMORY;
       goto done;
    }
    *obj = (ofpeerstatuslistener_p)peerStatusObjptr;
done:
    return status;
}

ofstatus
offramework_destroy_peerstatus_listener(ofpeerstatuslistener_p *obj)
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


/**
 * this function must be called before the start() on the framework 
 * there is no unregisterPeerStatusListener() function in the framework
 * class
 */
ofstatus
offramework_register_peerstatus_listener(offramework_p framework,
                              ofpeerstatuslistener_p peerStatusObjptr)
{
    ofstatus status = OF_ESUCCESS;
    OFFramework* f = NULL;
    CPeerStatusListener *ptr = (CPeerStatusListener *)peerStatusObjptr;

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

