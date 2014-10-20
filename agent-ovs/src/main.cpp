/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Main implementation for OVS agent
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <glog/logging.h>

#include "Agent.h"
#include "GLogLogHandler.h"

using ovsagent::Agent;
using ovsagent::GLogLogHandler;
using opflex::ofcore::OFFramework;
using opflex::logging::OFLogHandler;

GLogLogHandler logHandler(OFLogHandler::INFO);

int main(int argc, char** argv) {
    google::InitGoogleLogging(argv[0]);
    OFLogHandler::registerHandler(logHandler);

    LOG(INFO) << "Starting OVS Agent";

    Agent agent(OFFramework::defaultInstance());

    LOG(INFO) << "OVS Agent Shutting Down";
}
