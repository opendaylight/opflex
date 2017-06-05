/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string.h>
#include <iostream>
#include <iomanip>
#include <stdlib.h>
#include <signal.h>
#include <netinet/in.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>


#include <utility>
#include <functional>
#include "logging.h"

//Debugging
#include <unistd.h>
// End debugging includes

#include "VppApi.h"
#include "VppConnection.h"
#include "VppApiError.h"

using namespace std;

using boost::asio::ip::address;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

namespace ovsagent {

  VppApi::VppApi(unique_ptr<VppConnection> conn) :
    vppConn(move(conn)), nextBridge(1) {
  }

  VppApi::~VppApi() {
  }

  void VppApi::setName(string n)
  {
    this->name = n;
  }

  string VppApi::getName()
  {
    return name;
  }

  // Establish connection if not already done so
  int VppApi::connect(string n)
  {
    name = n;
    auto rv = getVppConnection()->connect(name);
    if (rv) {
      LOG(ERROR) << "Error connecting to VPP: " << rv;
      return rv;
    }
    //TODO supported versions should be defined in conf file
    std::vector<string> supported{"17.04"};
    if (!checkVersion(supported)) {
      disconnect();
      return rv;
    }

  }

  void VppApi::disconnect()
  {
    getVppConnection()->lockedDisconnect();
  }

  bool VppApi::isConnected()
  {
    return getVppConnection()->isConnected();
  }

  VppConnection* VppApi::getVppConnection()
  {
    return vppConn.get();
  }

  /*
   *  TODO: Decide on sub-class structure. I don't see need for Abstract bollocks...
   *  ... its like peeing in a brown suit... feels good but nobody really notices...
   * ... maybe thats how 10x programmers pump out 8x of their code?
   */
  VppVersion VppApi::getVersion()
  {
    /*
     * TODO: This whole guitar lick of typedef'ing, sizeof, cast etc
     * can be better handled via Templates template<class S_MSG, class R_MSG> etc
     * ... but early optimisation, root of all evil etc.
     */
    vl_api_show_version_t _mp, *mp;
    vl_api_show_version_reply_t *rmp;
    int apiMsgId = getVppConnection()->getMessageIdByName({"show_version"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"show_version_reply"});
    int msgSize = sizeof(*mp);
    u32 context = getVppConnection()->getNextContextId();
    char *reply;
    int reply_len;
    int rv;

    mp = &_mp;

    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;
    mp->context = ntohl(context);


    rv = getVppConnection()->send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error";
      return version; //TODO alagalah - just horrible.
    }

    rv = -1;

    std::vector<char*> msgs = getVppConnection()->messagesReceived(context);

    for (const auto& msg: msgs) {
      rmp = (vl_api_show_version_reply_t *) msg;
      if ( htons(rmp->_vl_msg_id) == apiReplyMsgId) {
        rv = ntohl (rmp->retval);
      }
    }

    if ( rv == 0 ) {
      version.program.assign((char *)rmp->program);
      version.version.assign((char *)rmp->version);
      version.buildDate.assign((char *)rmp->build_date);
      version.buildDirectory.assign((char *)rmp->build_directory);
    } else {
      LOG(ERROR) << "VPP had an error";
    }

    return version;
  }

  bool VppApi::checkVersion(std::vector<string> versions)
  {
    VppVersion versionInfo = getVersion();

    for (auto const v: versions) {
      if (boost::starts_with(versionInfo.version, v))
          return true;
    }
    return false;
  }

  ip46_address
  VppApi :: boostToVppIp46 (address addr) {
    ip46_address vppAddr;

    if (addr.is_v4() ) {
      vppAddr.is_ipv6 = 0;
      vppAddr.ip4 = boostToVppIp4(addr.to_v4());
    } else {
      vppAddr.is_ipv6 = 1;
      vppAddr.ip6 = boostToVppIp6(addr.to_v6());
    }

    return vppAddr;
  }

  ip4_address
  VppApi :: boostToVppIp4 (address_v4 addr) {
    ip4_address ip4;

    array<uint8_t, 4> as_u8 = addr.to_bytes();

    ip4.as_u8[0] = as_u8[0];
    ip4.as_u8[1] = as_u8[1];
    ip4.as_u8[2] = as_u8[2];
    ip4.as_u8[3] = as_u8[3];

    return ip4;
  }

  ip6_address
  VppApi :: boostToVppIp6 (address_v6 addr) {
    ip6_address ip6;

    array<uint8_t, 16> as_u8 = addr.to_bytes();

    ip6.as_u8[0] = as_u8[0];
    ip6.as_u8[1] = as_u8[1];
    ip6.as_u8[2] = as_u8[2];
    ip6.as_u8[3] = as_u8[3];
    ip6.as_u8[4] = as_u8[4];
    ip6.as_u8[5] = as_u8[5];
    ip6.as_u8[6] = as_u8[6];
    ip6.as_u8[7] = as_u8[7];
    ip6.as_u8[8] = as_u8[8];
    ip6.as_u8[9] = as_u8[9];
    ip6.as_u8[10] = as_u8[10];
    ip6.as_u8[11] = as_u8[11];
    ip6.as_u8[12] = as_u8[12];
    ip6.as_u8[13] = as_u8[13];
    ip6.as_u8[14] = as_u8[14];
    ip6.as_u8[15] = as_u8[15];

    return ip6;
  }

  void VppApi::printIpAddr (ip46_address ip_addr) {

    if (ip_addr.is_ipv6) {
      for (int i = 0; i < 8; i++) {
          if (i > 0)
            cout << ':';

          if (ip_addr.ip6.as_u16[i] == 0 && i < 8 && ip_addr.ip6.as_u16[i + 1] == 0) {
            while (ip_addr.ip6.as_u16[i + 1] == 0)
              ++i;
            continue;
          }
          cout << setfill('0') << setw(4) << hex << ip_addr.ip6.as_u16[i];
      }
      cout << dec;
    } else {
      cout << unsigned(ip_addr.ip4.as_u8[0]) << "." ;
      cout << unsigned(ip_addr.ip4.as_u8[1]) << "." ;
      cout << unsigned(ip_addr.ip4.as_u8[2]) << "." ;
      cout << unsigned(ip_addr.ip4.as_u8[3]) ;
    }
  }

void VppApi::initializeRoute(ip46_route *route)
{

  route->next_hop_sw_if_index = ~0;
  route->table_id = 0;
  route->classify_table_index = ~0;
  route->next_hop_table_id = 0;
  route->create_vrf_if_needed = 0;
  route->is_add = 1;
  route->is_drop = 0;
  route->is_unreach = 0;
  route->is_prohibit = 0;
  route->is_ipv6 = 0;
  route->is_local = 0;
  route->is_classify = 0;
  route->is_multipath = 0;
  route->is_resolve_host = 0;
  route->is_resolve_attached = 0;
  /* Is last/not-last message in group of multiple add/del messages. */
  route->not_last = 0;
  route->next_hop_weight = 1;
  route->dst_address_length = 0;
  route->next_hop_address_set = 0;
  route->next_hop_n_out_labels = MPLS_LABEL_INVALID;
  route->next_hop_via_label = MPLS_LABEL_INVALID;
  route->next_hop_out_label_stack = { };
}


  /*
   * VppApi state management functions for maps
   */

  std::pair<bool, u32> VppApi::getIntfIndexByName (string name) {

    map<string, u32>::iterator intfIterator;
    std::pair<bool, u32> rv = make_pair(false, ~0);

    if (name.empty()) {
      LOG(ERROR) << "Interface name empty.";
      return rv;
    }

    intfIterator = intfIndexByName.find(name);

    if (intfIterator == intfIndexByName.end()) {
      LOG(DEBUG) << "Interface name not found.";
      return rv;
    }

    rv.first = true;
    rv.second = intfIterator->second;
    return rv;
  }

  bool VppApi::setIntfIndexByName (string name, int sw_if_index) {

    if (getIntfIndexByName(name).first == false) {
      intfIndexByName.insert(make_pair(name,sw_if_index));
      return true;
    }
    return false;
  }

  bool VppApi::deleteIntfIndexByName (string name) {

    if (getIntfIndexByName(name).first == true) {
      intfIndexByName.erase(name);
      return true;
    }
    return false;
  }

  std::pair<bool, u32> VppApi::getBridgeIdByName (string name) {

    map<string, u32>::iterator bridgeIterator;
    std::pair<bool, u32> rv = make_pair(false, ~0);

    if (name.empty()) {
      LOG(ERROR) << "Bridge name empty";
      return rv;
    }

    bridgeIterator = bridgeIdByName.find(name);

    if (bridgeIterator == bridgeIdByName.end()) {
      LOG(DEBUG) << "Bridge name not found";
      return rv;
    }

    rv.first = true;
    rv.second = bridgeIterator->second;
    return rv;
  }

  bool VppApi::setBridgeIdByName (string name, int id) {

    if (getBridgeIdByName(name).first == false) {
      bridgeIdByName.insert( make_pair( name, id));
      return true;
    }
    return false;
  }

  bool VppApi::deleteBridgeIdByName (string name) {

    if (getBridgeIdByName(name).first == true) {
      bridgeIdByName.erase(name);
      return true;
    }
    return false;
  }

  std::pair<bool, string> VppApi::getBridgeNameByIntf (string intf) {

    map<string, string>::iterator bridgeIterator;
    std::pair<bool, string> rv = make_pair(false, std::string());

    if (intf.empty()) {
        LOG(ERROR) << "No interface name passed";
        return rv;
    }

    bridgeIterator = bridgeNameByIntf.find(intf);

    if (bridgeIterator == bridgeNameByIntf.end()) {
      LOG(DEBUG) << "Interface name not associated with bridge.";
      return rv;
    }

    rv.first = true;
    rv.second = bridgeIterator->second;
    return rv;
  }

  bool VppApi::setBridgeNameByIntf (string name, string intf) {

    if (getBridgeNameByIntf(name).first == false) {
      bridgeNameByIntf.insert( make_pair( name, intf));
      return true;
    }
    return false;
  }

  bool VppApi::deleteBridgeNameByIntf (string name) {

    if (getBridgeNameByIntf(name).first == true) {
      bridgeNameByIntf.erase(name);
      return true;
    }
    return false;
  }

  // Create a bridge by name and maintain VPP Bridge ID as Map
  int VppApi::createBridge(string bridgeName) {

    vl_api_bridge_domain_add_del_t _mp, *mp;
    vl_api_bridge_domain_add_del_reply_t *rmp;

    int apiMsgId = getVppConnection()->getMessageIdByName({"bridge_domain_add_del"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"bridge_domain_add_del_reply"});
    int msgSize = sizeof(*mp);
    u32 context = getVppConnection()->getNextContextId();
    int rv = 0;

    // Default values for the moment. But it will be struct to pass this function
    int flood = 1, forward = 1, learn = 1, uu_flood = 1, arp_term = 0;
    int  mac_age = 0;

    //Have we already created this bridge?
    if (getBridgeIdByName(name).first) {
      return BD_ALREADY_EXISTS; //bridge exists in our map already
    }

    mp = &_mp;
    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;
    mp->context = ntohl(context);
    mp->bd_id = ntohl (nextBridge);
    mp->is_add = 1;
    mp->flood = flood;
    mp->uu_flood = flood;
    mp->forward = forward;
    mp->learn = learn;
    mp->arp_term = arp_term;
    mp->mac_age = mac_age;

    rv = getVppConnection()->send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error" << rv;
      return UNSPECIFIED;
    }

    std::vector<char*> msgs = getVppConnection()->messagesReceived(context);

    for (const auto& msg: msgs) {
      rmp = (vl_api_bridge_domain_add_del_reply_t *) msg;
      if ( htons(rmp->_vl_msg_id) == apiReplyMsgId) {
        rv = ntohl (rmp->retval);
      }
    }

    if ( rv != 0 ) {
      LOG(ERROR) << "createBridge failed: " << rv ;
    } else {
      LOG(DEBUG) << "Created bridge " << bridgeName << " with bdId " << nextBridge ;
      if (setBridgeIdByName(bridgeName, nextBridge))
        nextBridge++;
    }
    return rv;
  }

  // Delete a bridge by name and maintain VPP Bridge ID as Map
  int VppApi::deleteBridge(string bridgeName) {

    vl_api_bridge_domain_add_del_t _mp, *mp;
    vl_api_bridge_domain_add_del_reply_t *rmp;

    int apiMsgId = getVppConnection()->getMessageIdByName({"bridge_domain_add_del"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"bridge_domain_add_del_reply"});
    int msgSize = sizeof(*mp);
    u32 context = getVppConnection()->getNextContextId();
    int rv = 0;
    int bdId;

    //Do we have this bridge?
    if (getBridgeIdByName(bridgeName).first == false) {
      return BD_NOT_MODIFIABLE; //bridge doesn't exist in our map
    }

    bdId = getBridgeIdByName(bridgeName).second;

    mp = &_mp;
    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;
    mp->context = ntohl(context);
    mp->bd_id = ntohl (bdId);
    mp->is_add = 0;

    rv = getVppConnection()->send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error" << rv;
      return UNSPECIFIED;
    }

    std::vector<char*> msgs = getVppConnection()->messagesReceived(context);

    for (const auto& msg: msgs) {
      rmp = (vl_api_bridge_domain_add_del_reply_t *) msg;
      if ( htons(rmp->_vl_msg_id) == apiReplyMsgId) {
        rv = ntohl (rmp->retval);
      }
    }

    if ( rv != 0 ) {
      LOG(ERROR) << "deleteBridge failed: " << rv ;
    } else {
      if (deleteBridgeIdByName(bridgeName))
        LOG(DEBUG) << "Deleted bridge " << bridgeName << " with bdId " << bdId ;
    }
    return rv;
  }

  int VppApi::createAfPacketIntf(string name) {

    vl_api_af_packet_create_t _mp, *mp;
    vl_api_af_packet_create_reply_t *rmp;

    int apiMsgId = getVppConnection()->getMessageIdByName({"af_packet_create"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"af_packet_create_reply"});
    int msgSize = sizeof(*mp);
    int rv = 0;
    int sw_if_index = ~0;
    u32 context;

    //Have we already created this interface?
    if (getIntfIndexByName(name).first == true) {
      LOG(ERROR) << "Already created interface : " << name ;
      return IF_ALREADY_EXISTS; // Intf exists in our map already
    }

    mp = &_mp;
    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;

    strcpy((char *)mp->host_if_name, name.c_str());
    mp->use_random_hw_addr = 1;

    context = getVppConnection()->getNextContextId();
    mp->context = ntohl(context);

    rv = getVppConnection()->send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error" << rv;
      return UNSPECIFIED;
    }

    std::vector<char*> msgs = getVppConnection()->messagesReceived(context);

    for (const auto& msg: msgs) {
      rmp = (vl_api_af_packet_create_reply_t *) msg;
      if ( htons(rmp->_vl_msg_id) == apiReplyMsgId) {
        rv = ntohl(rmp->retval);
        sw_if_index = ntohl(rmp->sw_if_index);
      }
    }


    if ( rv != 0 ) {
      LOG(ERROR) << "createAfPacketIntf failed: " << rv ;
    } else {
      LOG(DEBUG) << "Created interface host-" << name << " with sw_if_index " << sw_if_index ;
      setIntfIndexByName(name, sw_if_index);
    }
    return rv;
  }

  int VppApi::deleteAfPacketIntf(string name) {

    vl_api_af_packet_delete_t _mp, *mp;
    vl_api_af_packet_delete_reply_t *rmp;

    int apiMsgId = getVppConnection()->getMessageIdByName({"af_packet_delete"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"af_packet_delete_reply"});
    int msgSize = sizeof(*mp);
    int rv = 0;
    int sw_if_index;
    u32 context;

    auto intfPair = getIntfIndexByName(name);

    if (intfPair.first == false)
      return NO_MATCHING_INTERFACE; // Interface doesn't exist

    sw_if_index = intfPair.second;
    mp = &_mp;
    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;

    strcpy((char *)mp->host_if_name, name.c_str());

    context = getVppConnection()->getNextContextId();
    mp->context = ntohl(context);

    rv = getVppConnection()->send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error" << rv;
      return UNSPECIFIED;
    }

    std::vector<char*> msgs = getVppConnection()->messagesReceived(context);

    for (const auto& msg: msgs) {
      rmp = (vl_api_af_packet_delete_reply_t *) msg;
      if ( htons(rmp->_vl_msg_id) == apiReplyMsgId) {
        rv = ntohl(rmp->retval);
      }
    }

    if ( rv != 0 ) {
      LOG(ERROR) << "deleteAfPacketIntf failed: " << rv ;
    } else {
      LOG(DEBUG) << "delete interface host-" << name << " with sw_if_index " << sw_if_index ;
      deleteIntfIndexByName(name);
    }

    return rv;
  }

  int VppApi::setIntfL2Bridge(string bridgeName, string portName, u8 bviIntf) {

    vl_api_sw_interface_set_l2_bridge_t _mp, *mp;
    vl_api_sw_interface_set_l2_bridge_reply_t *rmp;

    int apiMsgId = getVppConnection()->getMessageIdByName({"sw_interface_set_l2_bridge"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"sw_interface_set_l2_bridge_reply"});
    int msgSize = sizeof(*mp);
    int rv = ~0;
    u32 context;
    int rx_sw_if_index;
    int bd_id;
    int shg = 0;
    uint8_t enable = 1;

    if (portName.empty() && bridgeName.empty()) {
      LOG(ERROR) << "One of these was empty -> bridgeName: " << bridgeName <<
                                                " portname : " << portName ;
      return INVALID_VALUE; // Interface and bridge names must be given
    }

    auto bdPair = getBridgeIdByName(bridgeName);
    auto intfPair = getIntfIndexByName(portName);

    if (intfPair.first == false) {
      return NO_MATCHING_INTERFACE;
    }

    if (bdPair.first == false) {
      return BD_NOT_MODIFIABLE;
    }

    if (getBridgeNameByIntf(portName).first == true) {
      LOG(INFO) << "bridgeName: " << bridgeName << " portname : " << portName << " already exists" ;
      return 0; //intf already exists in bridge in our map so assuming ok.
    }

    bd_id = bdPair.second;
    rx_sw_if_index = intfPair.second;

    mp = &_mp;
    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;

    mp->rx_sw_if_index = ntohl (rx_sw_if_index);
    mp->bd_id = ntohl (bd_id);
    //mp->shg = (u8) shg;
    mp->bvi = bviIntf;
    mp->enable = enable;
    context = getVppConnection()->getNextContextId();
    mp->context = ntohl(context);

    rv = getVppConnection()->send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error" << rv;
      return UNSPECIFIED;
    }

    std::vector<char*> msgs = getVppConnection()->messagesReceived(context);

    for (const auto& msg: msgs) {
      rmp = (vl_api_sw_interface_set_l2_bridge_reply_t *) msg;
      if ( htons(rmp->_vl_msg_id) == apiReplyMsgId) {
        rv = ntohl(rmp->retval);
      }
    }

    if ( rv != 0 ) {
      LOG(ERROR) << "setIntfL2Bridge failed: " << rv ;
    } else {
      LOG(DEBUG) << "Interface " << portName << " set in bridge domain " << bridgeName ;
      setBridgeNameByIntf(portName, bridgeName);
    }

    return rv;
  }

  int VppApi::setIntfIpAddr(string name,
                             u8 isAdd,
                             u8 delAll,
                             u8 isIpv6,
                             ip46_address ipAddr,
                             u8 mask) {

    vl_api_sw_interface_add_del_address_t _mp, *mp;
    vl_api_sw_interface_add_del_address_reply_t *rmp;
    int apiMsgId = getVppConnection()->getMessageIdByName({"sw_interface_add_del_address"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"sw_interface_add_del_address_reply"});
    int msgSize = sizeof(*mp);
    int rv = ~0;
    u32 context;
    int sw_if_index;
    ip6_address ip6;
    ip4_address ip4;

    auto intfPair = getIntfIndexByName(name);

    if (intfPair.first == false) {
      return NO_MATCHING_INTERFACE; // Interface doesn't exist
    }

    sw_if_index = intfPair.second;
    mp = &_mp;
    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;

    mp->sw_if_index = htonl (sw_if_index);
    mp->is_add = isAdd;
    mp->del_all = delAll;

    if (isIpv6 == 1) {
      mp->is_ipv6 = isIpv6;

      for (int i = 0; i < 8; i++)
        ip6.as_u16[i] = htons(ipAddr.ip6.as_u16[i]);

      memcpy (mp->address, &ip6, sizeof(ip6));
    } else {
      ip4.as_u32 = ipAddr.ip4.as_u32;
      memcpy (mp->address, &ip4, sizeof(ip4));
    }

    mp->address_length = mask;

    context = getVppConnection()->getNextContextId();
    mp->context = ntohl(context);

    rv = getVppConnection()->send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error" << rv;
      return UNSPECIFIED;
    }

    std::vector<char*> msgs = getVppConnection()->messagesReceived(context);

    for (const auto& msg: msgs) {
      rmp = (vl_api_sw_interface_add_del_address_reply_t *) msg;
      if ( htons(rmp->_vl_msg_id) == apiReplyMsgId) {
        rv = ntohl(rmp->retval);
      }
    }

    if ( rv != 0 ) {
      LOG(ERROR) << "setIntfIpAddr failed: " << rv ;
    } else {
      ipAddr.is_ipv6 = isIpv6;
      cout << "Set Interface " << name << " Ip Address ";
      printIpAddr(ipAddr);
      cout << "/" << unsigned(mask) ;
    }

    return rv;
  }

  int VppApi::setIntfFlags(string name, u8 flags) {

    vl_api_sw_interface_set_flags_t _mp, *mp;
    vl_api_sw_interface_set_flags_reply_t *rmp;
    int apiMsgId = getVppConnection()->getMessageIdByName({"sw_interface_set_flags"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"sw_interface_set_flags_reply"});
    int msgSize = sizeof(*mp);
    int rv = 0;
    u32 context;
    int sw_if_index;

    if (name.empty())
      return UNSPECIFIED; // Interface name must be given

    auto intfPair = getIntfIndexByName(name);

    if (intfPair.first == false)
      return NO_MATCHING_INTERFACE; // Interface doesn't exist

    sw_if_index = intfPair.second;
    mp = &_mp;
    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;

    mp->sw_if_index = ntohl (sw_if_index);
    mp->admin_up_down = flags;
    mp->link_up_down = flags;

    context = getVppConnection()->getNextContextId();
    mp->context = ntohl(context);

    rv = getVppConnection()->send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error" << rv;
      return UNSPECIFIED;
    }

    std::vector<char*> msgs = getVppConnection()->messagesReceived(context);

    for (const auto& msg: msgs) {
      rmp = (vl_api_sw_interface_set_flags_reply_t *) msg;
      if ( htons(rmp->_vl_msg_id) == apiReplyMsgId) {
        rv = ntohl(rmp->retval);
      }
    }

    if ( rv != 0 ) {
      LOG(ERROR) << "setIntfFlags failed: " << rv ;
    } else {
      LOG(DEBUG) << "Interface " << name << " sets up " ;
    }

    return rv;
  }

int VppApi::createLoopback(string loopbackName, mac_addr_t macAddr) {
    int rv = 0;
    vl_api_create_loopback_t _mp, *mp;
    vl_api_create_loopback_reply_t *rmp;
    int apiMsgId = getVppConnection()->getMessageIdByName({"create_loopback"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"create_loopback_reply"});
    int msgSize = sizeof(*mp);
    int sw_if_index;
    u32 context;

    if (loopbackName.empty())
        return UNSPECIFIED;

    auto intfPair = getIntfIndexByName(loopbackName);

    if (intfPair.first == true)
        return IF_ALREADY_EXISTS; // TODO Interface already exist - correct the return error code

    mp = &_mp;
    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;

    memcpy(mp->mac_address, macAddr, sizeof(*macAddr));
    context = getVppConnection()->getNextContextId();
    mp->context = ntohl(context);

    rv = getVppConnection()->send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error" << rv;
      return UNSPECIFIED;
    }

    std::vector<char*> msgs = getVppConnection()->messagesReceived(context);

    for (const auto& msg: msgs) {
      rmp = (vl_api_create_loopback_reply_t *) msg;
      if (htons(rmp->_vl_msg_id) == apiReplyMsgId) {
        rv = ntohl(rmp->retval);
        sw_if_index = ntohl(rmp->sw_if_index);
      }
    }

    if ( rv != 0 ) {
      LOG(ERROR) << "createLoopback failed: " << rv ;
    } else {
      LOG(DEBUG) << "Created Loop Back interface" << name << " with sw_if_index " << sw_if_index ;
      setIntfIndexByName(loopbackName, sw_if_index);
    }
    return rv;
}

int VppApi::deleteLoopback(string loopbackName) {
    return 0;
}

int VppApi::createVxlan(string vxlanIntfName, u8 isAdd, u8 isIpv6,
                        ip46_address srcAddr, ip46_address dstAddr,
                        string mcastIntf, u32 encapVrfId, u32 vni) {
    vl_api_vxlan_add_del_tunnel_t _mp, *mp;
    vl_api_vxlan_add_del_tunnel_reply_t *rmp;
    int apiMsgId = getVppConnection()->getMessageIdByName({"vxlan_add_del_tunnel"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"vxlan_add_del_tunnel_reply"});
    int msgSize = sizeof(*mp);
    int mcast_sw_if_index;
    int sw_if_index;
    ip6_address src_ip6, dst_ip6;
    ip4_address src_ip4, dst_ip4;
    int rv = -1;
    u32 context;

    if (vxlanIntfName.empty()) {
        LOG(ERROR) << "vxlan interface name must be given";
        return UNSPECIFIED;
    }

    auto intfPair = getIntfIndexByName(vxlanIntfName);
    if (intfPair.first == true)
        return IF_ALREADY_EXISTS; // TODO Interface already exist - correct the return error code

    mp = &_mp;
    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;
    context = getVppConnection()->getNextContextId();
    mp->context = ntohl(context);
    mp->is_add = isAdd;
    mp->encap_vrf_id = htonl(encapVrfId);
    mp->decap_next_index = ~0;
    mp->vni = htonl(vni);

    if (isIpv6 == 1) {
        mp->is_ipv6 = isIpv6;
        memcpy(mp->src_address, &srcAddr.ip6, sizeof(ip6_address));
        memcpy(mp->dst_address, &dstAddr.ip6, sizeof(ip6_address));
    } else {
        memcpy(mp->src_address, &srcAddr.ip4, sizeof(ip4_address));
        memcpy(mp->dst_address, &dstAddr.ip4, sizeof(ip4_address));
    }

    rv = getVppConnection()->send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error" << rv;
      return UNSPECIFIED;
    }

    std::vector<char*> msgs = getVppConnection()->messagesReceived(context);

    for (const auto& msg: msgs) {
      rmp = (vl_api_vxlan_add_del_tunnel_reply_t *) msg;
      if (htons(rmp->_vl_msg_id) == apiReplyMsgId) {
        rv = ntohl(rmp->retval);
        sw_if_index = ntohl(rmp->sw_if_index);
      }
    }

    if ( rv != 0 ) {
      LOG(ERROR) << "CreateVxlan failed: " << rv ;
    } else {
      LOG(DEBUG) << "Create VxLAN interface" << name << " with sw_if_index " << sw_if_index ;
      setIntfIndexByName(vxlanIntfName, sw_if_index);
    }
    return rv;
  }

int VppApi::setIntfIpTable(string intfName, u8 isIpv6, u8 vrfId) {
    int rv = -1;
    vl_api_sw_interface_set_table_t _mp, *mp;
    vl_api_sw_interface_set_table_reply_t *rmp;
    int apiMsgId = getVppConnection()->getMessageIdByName({"sw_interface_set_table"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"sw_interface_set_table_reply"});
    int msgSize = sizeof(*mp);
    int sw_if_index;
    u32 context;

    if (intfName.empty())
        return UNSPECIFIED; // Interface name must be given

    auto intfPair = getIntfIndexByName(intfName);

    if (intfPair.first == false)
      return NO_MATCHING_INTERFACE; // Interface doesn't exist

    sw_if_index = intfPair.second;
    mp = &_mp;
    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;
    mp->sw_if_index = ntohl (sw_if_index);
    mp->is_ipv6 = isIpv6;
    mp->vrf_id = vrfId;

    rv = getVppConnection()->send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error" << rv;
      return UNSPECIFIED;
    }

    std::vector<char*> msgs = getVppConnection()->messagesReceived(context);

    for (const auto& msg: msgs) {
      rmp = (vl_api_sw_interface_set_table_reply_t *) msg;
      if (htons(rmp->_vl_msg_id) == apiReplyMsgId) {
        rv = ntohl(rmp->retval);
      }
    }

    if (rv != 0) {
        LOG(ERROR) << "setIntfIpTable failed: " << rv;
    } else {
        LOG(DEBUG) << "Set Interface " << intfName << " Ip Table ";
    }

    return rv;
}

int VppApi::ipRouteAddDel(ip46_route *route) {
    return 0; // TODO implement the 'ip route'
}

} // namespace ovsagent
