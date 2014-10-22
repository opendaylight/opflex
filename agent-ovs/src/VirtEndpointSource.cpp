/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for VirtEndpointSource class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <stdexcept>
#include <libvirt/virterror.h>
#include <glog/logging.h>

#include "VirtEndpointSource.h"

namespace ovsagent {

VirtEndpointSource::VirtEndpointSource(EndpointManager* manager_,
                                       const std::string& hypervisorName_) 
    : EndpointSource(manager_), hypervisorName(hypervisorName_) {
    start();
}

VirtEndpointSource::~VirtEndpointSource() {
    stop();
}

/* dummy error function to suppress virDefaultErrorFunc */
static void customErrorFunc(void *userdata, virErrorPtr err) {
    VirtEndpointSource* source = (VirtEndpointSource*)userdata;
    LOG(ERROR) << "Libvirt error [" 
               << (source ? source->getHypervisorName() : "no connection")
               << "] " << err->message;
}

// XXX - TODO:
// virt event loop
// retry connections
void VirtEndpointSource::start() {
    virSetErrorFunc(NULL, customErrorFunc);
    LOG(INFO) << "Connecting to hypervisor " << hypervisorName;
    virtConn = virConnectOpenReadOnly(hypervisorName.c_str());
    if (virtConn == NULL) {
        throw std::runtime_error("Could not connect to hypervisor " + 
                                 hypervisorName );
    }
    virConnSetErrorFunc(virtConn, this, customErrorFunc);
}

void VirtEndpointSource::stop() {
    if (virtConn != NULL)
        virConnectClose(virtConn);
}

} /* namespace ovsagent */
