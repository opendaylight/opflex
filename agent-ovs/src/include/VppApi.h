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
#include <future>
#include <VppConst.h>

using namespace std;

namespace ovsagent {

    using u8 = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using i8 = int8_t;
    using i16 = int16_t;
    using i32 = int32_t;


    // TODO alagalah - rename all structs to be unambiguously vpp
    // but waiting on potential C++ bindings to VPP

    /**
     * Vpp Version struct
     * Contains information about VPP version, build date and build directory
     */
    struct VppVersion
    {
        string program;
        string version;
        string buildDate;
        string buildDirectory;
    };

    typedef union
    {
        u8 as_u8[4];
        u32 as_u32;
    } ip4_address;

    struct ip6_address
    {
        u16 as_u16[8];
    };

    struct ip46_address
    {
        u8 is_ipv6;
        ip4_address ip4;
        ip6_address ip6;
    };

    struct ip46_route
    {
        u32 next_hop_sw_if_index;
        u32 table_id;
        u32 classify_table_index;
        u32 next_hop_table_id;
        u8 create_vrf_if_needed;
        u8 is_add;
        u8 is_drop;
        u8 is_unreach;
        u8 is_prohibit;
        u8 is_ipv6; //TODO <- not needed really since ip46_address has this.
        u8 is_local;
        u8 is_classify;
        u8 is_multipath;
        u8 is_resolve_host;
        u8 is_resolve_attached;
        // Is last/not-last message in group of multiple add/del messages.
        u8 not_last;
        u8 next_hop_weight;
        u8 dst_address_length;
        ip46_address dst_address;
        u8 next_hop_address_set;
        ip46_address next_hop_address;
        u8 next_hop_n_out_labels;
        u32 next_hop_via_label;
        vector<u32> next_hop_out_label_stack;
    };

    struct VppAclRule
    {
        u8 is_permit;
        u8 is_ipv6; //TODO redundant since IPv6 flag is in ip46 struct
        ip46_address src_ip_addr;
        u8 src_ip_prefix_len;
        ip46_address dst_ip_addr;
        u8 dst_ip_prefix_len;
        u8 proto;
        //  After an ip[46]_header_t, either:
        // (L4) u16 srcport : u16 dstport
        // (ICMP) u8 type : u8 code: u16 checksum
        u16 srcport_or_icmptype_first;
        u16 srcport_or_icmptype_last;
        u16 dstport_or_icmpcode_first;
        u16 dstport_or_icmpcode_last;
        u8 tcp_flags_mask;
        u8 tcp_flags_value;

    };

    using VppAclRules_t = std::vector<VppAclRule>;

    struct VppAcl
    {
        u32 acl_index;
        string tag;
        u32 count;
        VppAclRules_t r;
    };


    using VppAcls_t = std::vector<VppAcl>;


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
    string name;

    /**
     * VPP's ID for this client connection
     */
    u32 clientIndex;

    /**
     * Connection state tracking bool variable
     */
    // CONTROL_PING can be used for API connection "keepalive"
    bool connected = false;

    //TODO: This should be a re-usable collection like a pool.
    u32 nextBridge = 1;
    u32 contextId = 42;

    /**
     * get current per message context
     */
    u32 getContextId();

    /**
     * increment and return per message ID
     */
    u32 getNextContextId();

    /**
     * message context (instance of a message ID) converted into host order
     */
    u32 getContextIdParm();

    /**
     * incremented message context converted into host order
     */
    u32 getNextContextIdParm();

    /**
     * Internal state management
     */
    map< string, u32> bridgeIdByName;
    map< string, string> bridgeNameByIntf;
    map< u32, VppAcl> aclsByIndex;


    VppVersion version;

    typedef boost::bimap< u16, string> messageIdsBiMap; //LEFT by ID, RIGHT by NAME
    typedef messageIdsBiMap::value_type messageIdsPair;
    typedef messageIdsBiMap::left_map::const_iterator messageIdsIdIter;
    typedef messageIdsBiMap::right_map::const_iterator messageIdsNameIter;

    messageIdsBiMap messageIds;
    map< string, string> messageCRCByName; //Name, CRC

    /**
     * Returns VPP message ID
     */
    u16 getMessageIdByName(string);

    /**
     * Returns VPP message name from its ID
     */
    string getMessageNameById(u16);

    /**
     * Returns message_crc for VPP getMessageId()
     */
    string getMessageWithCRCByName(string);

    /**
     * Static function for VPP API callback handling
     */
    static void messageReceiveHandler(char* data, int len);

public:

    /**
     * Default Constructor
     */
    VppApi();

    /**
     * Constructor with name of client
     */
    VppApi(string);

    /**
     * Set the name of the switch
     *
     * @param _name the switch name to use
     */
    void setName(string _name);

    /**
     * Get the name of the switch
     *
     * @return name of the switch to use
     */
    string getName();

    /**
     * TODO:
     *
     * @returns 0 success or -'ve fail
     */
    int populateMessageIds();

    /**
     * Typedef map to store interface name and numeric index
     * It can be extended to include fib, stats and/or other associated information
     * to interfaces.
     */
    typedef map< string, u32> portMap_t;

    /**
     * Data structure to store name and sw_if_index number of interfaces
     */
    portMap_t intfIndexByName;

    /**
     * Real connect function
     *
     * @param TODO
     * @return
     */
    int _connect (bool useCB);

    /**
     * Real send function
     *
     * @param msg
     * @param size
     * @param replyId TODO
     * @return
     */
    int send(char* msg, int size, u16 replyId);

    /**
     * Enable Stream function
     *
     * @param TODO
     * @param
     */
    void enableStream(u32 context, u16 streamId);

    /**
     * Message termination check for multipart message reply
     *
     * For multipart messages, send the message ending with '_dump' from
     * Opflex agent. Also send the following barrierSend. VPP will start
     * sending multipart messages in response to dump message and at the
     * end it will respond to barrierSend message.
     */
    int barrierSend(u32 context);
    int barrierSend();
    std::pair<bool, char*> barrierReceived();

    /**
     * uses messageMap of futures
     */
    std::vector<char*> messagesReceived(u32 context);

    /**
     * uses streamMap of futures
     */
    std::vector<char*> readStream(u32 context, u16 streamId);

    /**
     * @deprecated
     */
    std::vector<char*> messagesReceived();

    /**
     * Establish connection if not already done so
     */
    int connect();

    /**
     * Establish connection using callback
     */
    int connectWithHandler();

    /**
     * Terminate the connection with VPP
     */
    void disconnect();

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
    bool checkVersion(std::vector<string> versions);

    /**
     * Print Ip address
     */
    void printIpAddr(ip46_address ip_addr);

    /**
     * initialize the route struct
     */
    void initializeRoute(ip46_route *route);

    /**
     * Create simple bridge-domain with defaults.
     */
    int createBridge(string bridgeName);

    /**
     * Remove specified bridge domain
     *
     * @param bridgeName to be removed
     */
    int deleteBridge(string bridgeName);

    /**
     * Create AFPACKET interface from host veth name
     */
    int createAfPacketIntf(string name);

    /**
     * Remove specified AFPACKET interface
     *
     * @param name - interface name to be deleted
     */
    int deleteAfPacketIntf(string name);

    /**
     * Create loopback interface
     */
    int createLoopback (string name);

    /**
     * Remove loopback interface
     *
     * @param name - loopback interface name to be deleted
     */
    int deleteLoopback (string name);

    /**
     * Create/delete VxLAN
     *
     * @param vxlanIntfName - TODO
     * @param isAdd - 0 to remove, 1 to add
     * @param srcAddr - Tunnel Source address
     * @param dstAddr - Tunnel Termination address
     */
    int createVxlan(string vxlanIntfName,
                     u8 isAdd,
                     u8 isIpv6,
                     ip46_address srcAddr,
                     ip46_address dstAddr,
                     string mcastIntf,
                     int encapVrfId,
                     int vni);

    /**
     * Set interface flags - to bring the interface up or down
     *
     * @param name - interface name
     * @param flags - interface state flags (up/down, promiscuous etc.)
     */
    int setIntfFlags(string name, int flags);

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
    int setIntfIpAddr(string name,
                       u8 is_add,
                       u8 del_all,
                       u8 is_ipv6,
                       ip46_address ip_addr,
                       u8 mask);

    /**
     * set interface ip table (BVI)
     */
    int setIntfIpTable(string portName, u8 is_ipv6, u8 vrfId);

    /**
     * set ip route
     */
    int ipRouteAddDel(ip46_route *route);

    /**
     * set interface in bridge-domain
     */
    int setIntfL2Bridge(string bridgeName, string portName, u8 bviIntf);

    /**
     * Add or Replace an ACL and return pair int32_t rv and u32 VPP ACL Index
     */
    std::pair<int32_t, u32> aclAddReplace(const VppAcl acl);

    static const u32 VPP_SWIFINDEX_NONE = UINT32_MAX;

    /**
     * Return the number of API messages
     */
    int getMsgTableSize();

    /**
     * Return the API message ID based on message name
     */
    u16 getMsgIndex(string name);

    /**
     * Return the list of ACLs
     */
    VppAcls_t aclDump(u32 aclIndex);

    /**
     * Dump interface details
     */
    void interfaceDump(u32 sw_if_index);

    /**
     * Return in portMap known VPP interfaces
     */
    portMap_t dumpInterfaces( );

    /**
     * Assign a set of ACLs via index to an sw_if_index
     */
    int setIntfAclList(u32 sw_if_index, std::vector<u32> inAclIndexes,
                        std::vector<u32> outAclIndexes);

    /**
     * VppApi state management functions for maps
     */

    /**
     * get/set/delete sw_if_index by name in map: intfIndexByName
     */
    std::pair<bool, u32> getIntfIndexByName (string name);
    bool setIntfIndexByName (string name, int sw_if_index);
    bool deleteIntfIndexByName (string name);

    /**
     * get/set VppAcl by acl_index in map: aclsByIndex
     */
    std::pair<bool, VppAcl> getAclByIndex (u32 index);
    bool setAclByIndex(VppAcl acl);

    /**
     * get/set/delete Vpp bridge id by bridge domain name in map: bridgeIdByName
     */
    std::pair<bool, u32> getBridgeIdByName (string name);
    bool setBridgeIdByName (string name, int id);
    bool deleteBridgeIdByName (string name);

    /**
     * get/set/delete Vpp bridge Name by interface in map: bridgeNameByIntf
     */
    std::pair<bool, string> getBridgeNameByIntf (string name);
    bool setBridgeNameByIntf (string name, string intf);
    bool deleteBridgeNameByIntf (string name);

    /**
     * TODO this could be far better done with a generic template
     * Return a diff between two portMap states
     */
    std::pair< portMap_t, portMap_t> diffPortMaps (portMap_t lhs, portMap_t rhs);

    /**
     * Address conversion utils
     */
    ip46_address boostToVppIp46 (boost::asio::ip::address addr);
    ip4_address boostToVppIp4 (boost::asio::ip::address_v4 addr);
    ip6_address boostToVppIp6 (boost::asio::ip::address_v6 addr);

    /**
     * VPP uses port tags to implement policy (EPGs, SecGroups, direction)
     * Currently re-using convention from networking-vpp ML2 driver
     */
    static const int VPP_TO_VM;
    static const int VM_TO_VPP;
    static const std::string VPP_TO_VM_MARK;
    static const std::string VM_TO_VPP_MARK;
    static const std::string SECGROUP_TAG;
    static const std::string EPG_TAG;
    static const std::string TAG_UPLINK_PREFIX;
    static const std::string TAG_L2IFACE_PREFIX;

};

} //namespace ovsagent
#endif /* VPPAPI_H */
