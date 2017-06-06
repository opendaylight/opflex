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

    /**
     * Bridge ID used to keep records
     */
    u32 nextBridge;

    /**
     * Internal state management
     */
    std::map< std::string, u32> intfIndexByName;
    std::map< std::string, u32> bridgeIdByName;
    std::map< std::string, std::string> bridgeNameByIntf;

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

    /**
     * Address conversion utils
     */
    ip46_address boostToVppIp46 (boost::asio::ip::address addr);
    ip4_address boostToVppIp4 (boost::asio::ip::address_v4 addr);
    ip6_address boostToVppIp6 (boost::asio::ip::address_v6 addr);

    /**
     * Print Ip address
     */
    void printIpAddr(ip46_address ip_addr);

    /**
     * VppApi state management functions for maps
     */

    /**
     * get/set/delete sw_if_index by name in map: intfIndexByName
     */
    std::pair<bool, u32> getIntfIndexByName (std::string name);
    bool setIntfIndexByName (std::string name, int sw_if_index);
    bool deleteIntfIndexByName (std::string name);

    /**
     * get/set/delete Vpp bridge id by bridge domain name in map: bridgeIdByName
     */
    std::pair<bool, u32> getBridgeIdByName (std::string name);
    bool setBridgeIdByName (std::string name, int id);
    bool deleteBridgeIdByName (std::string name);

    /**
     * get/set/delete Vpp bridge Name by interface in map: bridgeNameByIntf
     */
    std::pair<bool, std::string> getBridgeNameByIntf (std::string name);
    bool setBridgeNameByIntf (std::string name, std::string intf);
    bool deleteBridgeNameByIntf (std::string name);

    /**
     * Create simple bridge-domain with defaults.
     */
    int createBridge(std::string bridgeName);

    /**
     * Remove specified bridge domain
     *
     * @param bridgeName to be removed
     */
    int deleteBridge(std::string bridgeName);

    /**
     * Create AFPACKET interface from host veth name
     */
    int createAfPacketIntf(std::string name);

    /**
     * Remove specified AFPACKET interface
     *
     * @param name - interface name to be deleted
     */
    int deleteAfPacketIntf(std::string name);

    /**
     * Set interface flags - to bring the interface up or down
     *
     * @param name - interface name
     * @param flags - interface state flags (up/down, promiscuous etc.)
     */
    int setIntfFlags(std::string name, int flags);

    /**
     * set/delete interface ip address
     *
     * @param name - interface name
     * @param is_add - 1 to add, 0 to remove
     * @param del_all - 1 to remove all the ip addresses assigned to
     *                specified interface
     * @param is_ipv6 - 1 if address is IPv6, otherwise 0
     * @param ip_addr - ip address to set on given interface
     * @param mask - subnet mask
     */
    int setIntfIpAddr(std::string name,
                       u8 is_add,
                       u8 del_all,
                       u8 is_ipv6,
                       ip46_address ip_addr,
                       u8 mask);
    /**
     * set interface in bridge-domain
     */
    int setIntfL2Bridge(std::string bridgeName, std::string portName, u8 bviIntf);

    virtual VppConnection* getVppConnection();

    /**
     * Interface flags Up/Down
     */
    enum intfFlags {
        Down = 0,
        Up   = 1
    };

    /**
     * BVI Interface
     */
    enum bviFlags {
        NoBVI = 0,
        BVI   = 1
    };

protected:

    std::unique_ptr<VppConnection> vppConn;

};

} //namespace ovsagent
#endif /* VPPAPI_H */
