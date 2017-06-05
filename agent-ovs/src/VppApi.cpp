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

using namespace std;

using boost::asio::ip::address;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

namespace ovsagent {

  VppApi::VppApi(unique_ptr<VppConnection> conn) :
    vppConn(move(conn)) {
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

  //TODO
  ip6_address
  VppApi :: boostToVppIp6 (address_v6 addr) {
    ip6_address ip6;

    return ip6;
  }


  void VppApi::printIpAddr (ip46_address ip_addr)
  {
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

  /*
   * VppApi state management functions for maps
   */

  std::pair<bool, u32> VppApi::getIntfIndexByName (string name)
  {
    map<string, u32>::iterator intfIterator;
    std::pair<bool, u32> rv = make_pair(false, ~0);

    if (name.empty()) {
      LOG(ERROR) << "Interface name empty.";
      return rv;
    }

    intfIterator = intfIndexByName.find(name);

    if (intfIterator == intfIndexByName.end()) {
      LOG(INFO) << "Interface name not found.";
      return rv;
    }

    rv.first = true;
    rv.second = intfIterator->second;
    return rv;
  }

  bool VppApi::setIntfIndexByName (string name, int sw_if_index)
  {
    if (getIntfIndexByName(name).first == false) {
      intfIndexByName.insert(make_pair(name,sw_if_index));
      return true;
    }
    return false;
  }

  bool VppApi::deleteIntfIndexByName (string name)
  {
    if (getIntfIndexByName(name).first == true) {
      intfIndexByName.erase(name);
      return true;
    }
    return false;
  }

  std::pair<bool, u32> VppApi::getBridgeIdByName (string name)
  {
    map<string, u32>::iterator bridgeIterator;
    std::pair<bool, u32> rv = make_pair(false, ~0);

    if (name.empty()) {
      LOG(ERROR) << "Bridge name empty";
      return rv;
    }

    bridgeIterator = bridgeIdByName.find(name);

    if (bridgeIterator == bridgeIdByName.end()) {
      LOG(INFO) << "Bridge name not found";
      return rv;
    }

    rv.first = true;
    rv.second = bridgeIterator->second;
    return rv;
  }

  bool VppApi::setBridgeIdByName (string name, int id)
  {
    if (getBridgeIdByName(name).first == false) {
      bridgeIdByName.insert( make_pair( name, id));
      return true;
    }
    return false;
  }

  bool VppApi::deleteBridgeIdByName (string name)
  {
    if (getBridgeIdByName(name).first == true) {
      bridgeIdByName.erase(name);
      return true;
    }
    return false;
  }

  std::pair<bool, string> VppApi::getBridgeNameByIntf (string intf)
  {
    map<string, string>::iterator bridgeIterator;
    std::pair<bool, string> rv = make_pair(false, std::string());

    if (intf.empty()) {
        LOG(ERROR) << "No interface name passed";
        return rv;
    }

    bridgeIterator = bridgeNameByIntf.find(intf);

    if (bridgeIterator == bridgeNameByIntf.end()) {
      LOG(INFO) << "Interface name not associated with bridge.";
      return rv;
    }

    rv.first = true;
    rv.second = bridgeIterator->second;
    return rv;
  }

  bool VppApi::setBridgeNameByIntf (string name, string intf)
  {
    if (getBridgeNameByIntf(name).first == false) {
      bridgeNameByIntf.insert( make_pair( name, intf));
      return true;
    }
    return false;
  }

  bool VppApi::deleteBridgeNameByIntf (string name)
  {
    if (getBridgeNameByIntf(name).first == true) {
      bridgeNameByIntf.erase(name);
      return true;
    }
    return false;
  }

  // Create a bridge by name and maintain VPP Bridge ID as Map
  int VppApi::createBridge(string bridgeName)
  {
    vl_api_bridge_domain_add_del_t _mp, *mp;
    vl_api_bridge_domain_add_del_reply_t *rmp;

    int apiMsgId = getVppConnection()->getMessageIdByName({"bridge_domain_add_del"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"bridge_domain_add_del_reply"});
    int msgSize = sizeof(*mp);
    u32 context = getVppConnection()->getNextContextId();

    char *reply;
    int reply_len;
    int rv = 0;

    // Default values for the moment. But it will be struct to pass this function
    int flood = 1, forward = 1, learn = 1, uu_flood = 1, arp_term = 0;
    int  mac_age = 0;

    //Have we already created this bridge?
    if (getBridgeIdByName(name).first) {
      return -1; //bridge exists in our map already
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
      LOG(ERROR) << "VPP had an error";
      return -1;
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
      LOG(INFO) << "Created bridge " << bridgeName << " with bdId " << nextBridge ;
      if (setBridgeIdByName(bridgeName, nextBridge))
        nextBridge++;
    }
    return rv;
  }

  // Delete a bridge by name and maintain VPP Bridge ID as Map
  int VppApi::deleteBridge(string bridgeName)
  {
    vl_api_bridge_domain_add_del_t _mp, *mp;
    vl_api_bridge_domain_add_del_reply_t *rmp;

    int apiMsgId = getVppConnection()->getMessageIdByName({"bridge_domain_add_del"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"bridge_domain_add_del_reply"});
    int msgSize = sizeof(*mp);
    u32 context = getVppConnection()->getNextContextId();
    char *reply;
    int reply_len;
    int rv = 0;
    int bdId;

    //Do we have this bridge?
    if (getBridgeIdByName(bridgeName).first == false) {
      return -2; //bridge doesn't exist in our map
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
      LOG(ERROR) << "VPP had an error";
      return -1;
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
        LOG(INFO) << "Deleted bridge " << bridgeName << " with bdId " << bdId ;
    }
    return rv;
  }

  int VppApi::createAfPacketIntf(string name)
  {
    vl_api_af_packet_create_t _mp, *mp;
    vl_api_af_packet_create_reply_t *rmp;

    int apiMsgId = getVppConnection()->getMessageIdByName({"af_packet_create"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"af_packet_create_reply"});
    int msgSize = sizeof(*mp);
    char *reply;
    int reply_len;
    int rv = 0;
    int sw_if_index = ~0;
    u32 context;

    //Have we already created this interface?
    if (getIntfIndexByName(name).first == true) {
      LOG(ERROR) << "Already created interface : " << name ;
      return -1; // Intf exists in our map already
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
      LOG(ERROR) << "VPP had an error";
      return -1;
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
      LOG(INFO) << "Created interface host-" << name << " with sw_if_index " << sw_if_index ;
      setIntfIndexByName(name, sw_if_index);
    }
    return rv;
  }

  int VppApi::deleteAfPacketIntf(string name)
  {
    vl_api_af_packet_delete_t _mp, *mp;
    vl_api_af_packet_delete_reply_t *rmp;

    int apiMsgId = getVppConnection()->getMessageIdByName({"af_packet_delete"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"af_packet_delete_reply"});
    int msgSize = sizeof(*mp);
    char *reply;
    int reply_len;
    int rv = 0;
    int sw_if_index;
    u32 context;

    auto intfPair = getIntfIndexByName(name);

    if (intfPair.first == false)
      return -1; // Interface doesn't exist

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
      LOG(ERROR) << "VPP had an error";
      return -1;
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
      LOG(INFO) << "delete interface host-" << name << " with sw_if_index " << sw_if_index ;
      deleteIntfIndexByName(name);
    }

    return rv;
  }

  int VppApi::setIntfL2Bridge(string bridgeName, string portName, u8 bviIntf)
  {
    vl_api_sw_interface_set_l2_bridge_t _mp, *mp;
    vl_api_sw_interface_set_l2_bridge_reply_t *rmp;

    int apiMsgId = getVppConnection()->getMessageIdByName({"sw_interface_set_l2_bridge"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"sw_interface_set_l2_bridge_reply"});
    int msgSize = sizeof(*mp);
    char *reply;
    int reply_len;
    int rv = ~0;
    u32 context;
    int rx_sw_if_index;
    int bd_id;
    int shg = 0;
    uint8_t enable = 1;

    if (portName.empty() && bridgeName.empty()) {
      LOG(ERROR) << "One of these was empty -> bridgeName: " << bridgeName <<
                                                " portname : " << portName ;
      return -200; // Interface and bridge names must be given
    }

    auto bdPair = getBridgeIdByName(bridgeName);
    auto intfPair = getIntfIndexByName(portName);

    if (intfPair.first == false) {
      LOG(ERROR) << "portName: " << portName << " not in our map" ;
      return -201;
    }

    if (bdPair.first == false) {
      LOG(ERROR) << "bridgeName: " << bridgeName << " not in our map" ;
      return -202;
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
      LOG(ERROR) << "VPP had an error";
      return -1;
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
      LOG(INFO) << "Interface " << portName << " set in bridge domain " << bridgeName ;
      setBridgeNameByIntf(portName, bridgeName);
    }

    return rv;
  }

  int VppApi::setIntfIpAddr(string name,
                             u8 is_add,
                             u8 del_all,
                             u8 is_ipv6,
                             ip46_address ip_addr,
                             u8 mask)
  {
    vl_api_sw_interface_add_del_address_t _mp, *mp;
    vl_api_sw_interface_add_del_address_reply_t *rmp;
    int apiMsgId = getVppConnection()->getMessageIdByName({"sw_interface_add_del_address"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"sw_interface_add_del_address_reply"});
    int msgSize = sizeof(*mp);
    char *reply;
    int reply_len;
    int rv = ~0;
    u32 context;
    int sw_if_index;
    ip6_address ip6;
    ip4_address ip4;

    auto intfPair = getIntfIndexByName(name);

    if (intfPair.first == false) {
      LOG(ERROR) << "Interface does not exist.";
      return -201; // Interface doesn't exist
    }

    sw_if_index = intfPair.second;
    mp = &_mp;
    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;

    mp->sw_if_index = ntohl (sw_if_index);
    mp->is_add = is_add;
    mp->del_all = del_all;

    if (is_ipv6 == 1) {
      mp->is_ipv6 = is_ipv6;

      for (int i = 0; i < 8; i++)
        ip6.as_u16[i] = htons(ip_addr.ip6.as_u16[i]);

      memcpy (mp->address, &ip6, sizeof(ip6));
    } else {
      ip4.as_u32 = ip_addr.ip4.as_u32;
      memcpy (mp->address, &ip4, sizeof(ip4));
    }

    mp->address_length = mask;

    context = getVppConnection()->getNextContextId();
    mp->context = ntohl(context);

    rv = getVppConnection()->send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error";
      return -1;
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
      ip_addr.is_ipv6 = is_ipv6;
      cout << "Set Interface " << name << " Ip Address ";
      printIpAddr(ip_addr);
      cout << "/" << unsigned(mask) ;
    }

    return rv;
  }

  int VppApi::setIntfFlags(string name, int flags)
  {
    vl_api_sw_interface_set_flags_t _mp, *mp;
    vl_api_sw_interface_set_flags_reply_t *rmp;
    int apiMsgId = getVppConnection()->getMessageIdByName({"sw_interface_set_flags"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"sw_interface_set_flags_reply"});
    int msgSize = sizeof(*mp);
    char *reply;
    int reply_len;
    int rv = 0;
    u32 context;
    int sw_if_index;

    if (name.empty())
      return -1; // Interface name must be given

    auto intfPair = getIntfIndexByName(name);

    if (intfPair.first == false)
      return -2; // Interface doesn't exist

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
      LOG(ERROR) << "VPP had an error";
      return -1;
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
      LOG(INFO) << "Interface " << name << " sets up " ;
    }

    return rv;
  }

} // namespace ovsagent
