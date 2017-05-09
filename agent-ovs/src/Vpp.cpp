/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "Vpp.h"
#include "logging.h"

#include <boost/asio/placeholders.hpp>
#include <boost/algorithm/string/split.hpp>

namespace ovsagent {

using std::bind;
using opflex::ofcore::OFFramework;
using boost::property_tree::ptree;
using boost::asio::placeholders::error;

Vpp::Vpp(Agent& agent_)
    : Renderer(agent_), started(false) {
	LOG(INFO) << "Vpp Renderer";
}

Vpp::~Vpp() {

}

void Vpp::start() {
    if (started) return;

    started = true;
    LOG(INFO) << "Starting vpp renderer using";
}

void Vpp::stop() {
    if (!started) return;
    started = false;

    LOG(INFO) << "Stopping vpp renderer";
}

Renderer* Vpp::create(Agent& agent) {
    return new Vpp(agent);
}

void Vpp::setProperties(const ptree& properties) {
    static const std::string VPP_BRIDGE_NAME("vpp-bridge-name");
    std::string bridgeName =
	properties.get<std::string>(VPP_BRIDGE_NAME, "vpp");

    LOG(INFO) << "Bridge Name " << bridgeName;
}

}
