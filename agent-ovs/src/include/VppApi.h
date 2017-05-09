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



#ifndef VPPAPI_H
#define VPPAPI_H

#include <string.h>
#include <map>
#include <vector>
#include <boost/asio/ip/address.hpp>
#include <boost/bimap.hpp>
#include <VppStruct.h>
#include <VppConnection.h>

namespace ovsagent {

    // TODO alagalah - rename all structs to be unambiguously vpp
    // but waiting on potential C++ bindings to VPP

/**
 * The Vpp API class provides functions to configure VPP.
 *
 * VPP API messages take general form:
 * u16 _vl_msg_id - enum for each API call whether request or reply
 * u32 client_index - a shared memory connection to VPP for API messages
 * u32 context - unique identifier for instance of _vl_msg_id
 *
 *  Example:
 *  - on successful connection to VPP API a client_index of "7" is assigned
 *  - to create a bridge, send the _vl_msg_id of 76 (not guaranteed same, must be queried)
 *    corresponding to bridge_domain_add_del
 *  - VPP will respond with a _vl_msg_id of 77 corresponding to bridge_domain_add_del_reply
 *
 *  If two "bridge_domain_add_del" requests are sent asynchronously, it is feasible that
 *  the _reply messages may return out of order (extremely unlikely - but for purposes of
 *  illustration). In this case a "context" is maintained between each request/reply pair.
 *  This is client managed, meaning if the user sends 42 in a request, it will get 42
 *  in the response. A stylized example:
 *
 *  client -> VPP: bridge_domain_add_del(bridge-name="foo", context=42)
 *  client -> VPP: bridge_domain_add_del(bridge-name="bar", context=73)
 *
 *  VPP -> client: bridge_domain_add_del_reply("Created a bridge (rv=0)", 73)
 *  VPP -> client: bridge_domain_add_del_reply("Created a bridge (rv=0)", 42)
 *
 *  This becomes important especially in multipart responses as well as requesting
 *  streaming async statistics.
 */
class VppApi  {

    /**
     * Client name which gets from opflex config
     */
    std::string name;

    /**
     * VPP's ID for this client connection
     */
    u32 clientIndex;

    VppVersion version;

public:

    VppApi(std::unique_ptr<VppConnection> conn);

    virtual ~VppApi();

    /**
     * Set the name of the switch
     *
     * @param _name the switch name to use
     */
    void setName(std::string n);

    /**
     * Get the name of the switch
     *
     * @return name of the switch to use
     */
    std::string getName();

    /**
     * Establish connection if not already done so
     */
    virtual int connect(std::string name);

    /**
     * Terminate the connection with VPP
     */
    virtual void disconnect();

    bool isConnected();

    /**
     * TODO TEST ONLY
     */
    void enableStats();

    /**
     * Get VPP Version
     */
    VppVersion getVersion();

    /**
     * Print VPP version information to stdout
     */
    void printVersion(VppVersion version);

    /**
     * Check version expects vector of strings of support versions
     */
    bool checkVersion(std::vector<std::string> versions);


    virtual VppConnection* getVppConnection();

protected:

    std::unique_ptr<VppConnection> vppConn;

};

} //namespace ovsagent
#endif /* VPPAPI_H */
