/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OpflexHandler.h
 * @brief Interface definition file for OpFlex message handlers
 */
/*
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef GBP_CLIENT_H
#define GBP_CLIENT_H

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#ifdef RAPIDJSON_HAS_STDSTRING
#undef RAPIDJSON_HAS_STDSTRING
#endif
#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>
#include <opflex/test/MockOpflexServer.h>

namespace gbpserver {
    class GBPObject;
};

namespace opflexagent {

class GbpClient {
public:
    GbpClient(const std::string& address,
              opflex::test::MockOpflexServer& server);
    ~GbpClient();

private:
    void Start(const std::string& address);
    void JsonDump(rapidjson::Document& d);
    void JsonDocAdd(rapidjson::Document& d, const gbpserver::GBPObject& gbp);
    std::thread thread_;
    opflex::test::MockOpflexServer& server_;
};

} /* namespace opflexagent */

#endif /* GBP_CLIENT_H */
