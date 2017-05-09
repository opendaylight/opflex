/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for VppApi
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */



#ifndef MOCKVPPAPI_H
#define MOCKVPPAPI_H

#include <string.h>
#include <map>
#include <vector>
#include <boost/asio/ip/address.hpp>
#include <boost/bimap.hpp>
#include <VppStruct.h>
#include <MockVppConnection.h>
#include <VppApi.h>

namespace ovsagent {

class MockVppApi : public VppApi {


    MockVppConnection* mockVppConn;

public:
    MockVppApi();

    ~MockVppApi();

    virtual MockVppConnection* getVppConnection();
};

} //namespace ovsagent
#endif /* MOCKVPPAPI_H */
