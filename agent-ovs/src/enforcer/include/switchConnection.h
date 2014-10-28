/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef _SWITCHCONNECTION_H_
#define _SWITCHCONNECTION_H_


struct rconn;

namespace opflex {
namespace enforcer {

/**
 * @brief Class to handle communication with the OpenFlow switch
 */

class SwitchConnection {
public:
    SwitchConnection(const std::string& swName);
    ~SwitchConnection();

    /**
     * Connect to the switch and monitor the connection.
     * @param protoVer Version of OpenFlow protocol to use
     */
    bool Connect(ofp_version protoVer);

    /**
     * Disconnect from switch.
     */
    void Disconnect();

    /**
     * Returns true is connected to the switch.
     */
    bool IsConnected();

private:
    std::string switchName;
    vconn *ofConn;
    ofp_version ofProtoVersion;
    bool isConnected;
};

}   // namespace enforcer
}   // namespace opflex


#endif // _SWITCHCONNECTION_H_
