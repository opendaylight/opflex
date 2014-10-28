/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>
#include <glog/logging.h>
#include <ovs.h>
#include <switchConnection.h>

using namespace std;

namespace opflex {
namespace enforcer {

SwitchConnection::SwitchConnection(const std::string& swName) :
        switchName(swName) {
    ofConn = NULL;
    ofProtoVersion = OFP10_VERSION;
}

SwitchConnection::~SwitchConnection() {
    Disconnect();
}


bool
SwitchConnection::Connect(ofp_version protoVer) {
    if (ofConn != NULL) {    // connection already created
        return isConnected;
    }

    string swPath;
    swPath.append("unix:").append(ovs_rundir()).append("/")
            .append(switchName).append(".mgmt");

    uint32_t versionBitmap = 1u << protoVer;
    int error;
    error = vconn_open(swPath.c_str(), versionBitmap, DSCP_DEFAULT,
                       &ofConn);
    if (error) {
        LOG(ERROR) << "Unable to open socket to " << swPath
                << ": " << ovs_strerror(error);
        return false;
    }

    error = vconn_connect_block(ofConn);
    if (error) {
        LOG(ERROR) << "Failed to connect to " << swPath
                << ": " << ovs_strerror(error);
        return false;
    }

    /* Verify we have the correct protocol version */
    ofp_version connVersion = (enum ofp_version)vconn_get_version(ofConn);
    if (connVersion != protoVer) {
        LOG(WARNING) << "Remote supports version " << connVersion <<
                ", wanted " << ofProtoVersion;
    }
    ofProtoVersion = connVersion;
    isConnected = true;
    LOG(INFO) << "Connected to switch " << swPath
            << " using protocol version " << ofProtoVersion;
    return true;
}

void
SwitchConnection::Disconnect() {
    if (ofConn == NULL) {
        return;
    }

    vconn_close(ofConn);
    ofConn = NULL;
    isConnected = false;
}

bool
SwitchConnection::IsConnected() {
    return ofConn != NULL && isConnected;
}

}   // namespace enforcer
}   // namespace opflex

